#include "MotionManager.h"
#include <math.h>

MotionManager::MotionManager()
    : _ax(0), _ay(0), _az(0),
      _prevMag(1.0f),
      _tiltBiasX(0.0f),
      _isUpsideDown(false),
      _isFaceDown(false),
      _faceDownStartTime(0),
      _lastJumpTime(0),
      _lastShakeTime(0),
      _jumpArmed(false),
      _jumpArmTime(0),
      _pendingEvent(MOTION_EVENT_NONE) {}

bool MotionManager::begin() {
    // M5Unified automatically initializes BMI270 / MPU6886 during M5.begin()
    return M5.Imu.isEnabled();
}

void MotionManager::reset() {
    _prevMag = 1.0f;
    _tiltBiasX = 0.0f;
    _isUpsideDown = false;
    _isFaceDown = false;
    _faceDownStartTime = 0;
    _jumpArmed = false;
    _pendingEvent = MOTION_EVENT_NONE;
}

MotionEvent MotionManager::popEvent() {
    MotionEvent ev = _pendingEvent;
    _pendingEvent = MOTION_EVENT_NONE;
    return ev;
}

void MotionManager::update(uint32_t nowMillis) {
    if (!M5.Imu.isEnabled()) {
        return;
    }

    M5.Imu.update();
    M5.Imu.getAccel(&_ax, &_ay, &_az);

    // Calculate overall vector magnitude
    float mag = sqrtf(_ax * _ax + _ay * _ay + _az * _az);
    float deltaMag = fabsf(mag - _prevMag);
    _prevMag = mag;

    // 1. Tilt Detection (Pitch / Roll)
    // Lateral tilt along X axis biases walking velocity downhill
    if (fabsf(_ax) > MOTION_TILT_DEADZONE) {
        _tiltBiasX = _ax * 1.6f;
        if (_tiltBiasX > MOTION_MAX_WALK_BIAS) _tiltBiasX = MOTION_MAX_WALK_BIAS;
        if (_tiltBiasX < -MOTION_MAX_WALK_BIAS) _tiltBiasX = -MOTION_MAX_WALK_BIAS;
    } else {
        _tiltBiasX = 0.0f;
    }

    // Calculate approximate pitch and roll angles
    float pitch = atan2f(-_ax, sqrtf(_ay * _ay + _az * _az)) * (180.0f / M_PI);
    float roll = atan2f(_ay, _az) * (180.0f / M_PI);

    // 2. Jump Detection (Z / Vertical Impulse)
    // Positive vertical impulse (> 1.8G) followed by low-G
    if (!_jumpArmed && mag > MOTION_JUMP_THRESHOLD_G && (nowMillis - _lastJumpTime > 800)) {
        _jumpArmed = true;
        _jumpArmTime = nowMillis;
    } else if (_jumpArmed) {
        // Spike followed by low-G drop confirms a physical hop/jump
        if (mag < 1.1f || (nowMillis - _jumpArmTime > 80)) {
            _jumpArmed = false;
            _lastJumpTime = nowMillis;
            _pendingEvent = MOTION_EVENT_JUMP;
        } else if (nowMillis - _jumpArmTime > 400) {
            _jumpArmed = false; // Timed out
        }
    }

    // 3. Shake / Roar Detection (Vector Magnitude)
    // High rapid acceleration magnitude (> 2.5G or delta > 1.8G)
    if (mag > MOTION_SHAKE_THRESHOLD_G || deltaMag > 1.8f) {
        if (nowMillis - _lastShakeTime > 1500) {
            _lastShakeTime = nowMillis;
            _pendingEvent = MOTION_EVENT_ROAR;
        }
    }

    // 4. Inversion Detection (> 150 deg flip or device held upside down)
    bool inverted = (fabsf(pitch) > MOTION_INVERT_THRESHOLD_DEG || fabsf(roll) > MOTION_INVERT_THRESHOLD_DEG || _ay < -0.75f);
    if (inverted && !_isUpsideDown) {
        _isUpsideDown = true;
        _pendingEvent = MOTION_EVENT_INVERT_START;
    } else if (!inverted && _isUpsideDown) {
        _isUpsideDown = false;
        _pendingEvent = MOTION_EVENT_INVERT_END;
    }

    // 5. Bedtime Detection (Face Down)
    // Screen flat facing down: az negative (< -0.8G), ax and ay near zero
    bool faceDownNow = (_az < -0.80f && fabsf(_ax) < 0.40f && fabsf(_ay) < 0.40f);
    if (faceDownNow) {
        if (!_isFaceDown) {
            _isFaceDown = true;
            _faceDownStartTime = nowMillis;
        } else {
            if (_faceDownStartTime > 0 && (nowMillis - _faceDownStartTime >= MOTION_FACE_DOWN_TIME_MS)) {
                _faceDownStartTime = 0; // Trigger once
                _pendingEvent = MOTION_EVENT_BEDTIME;
            }
        }
    } else {
        _isFaceDown = false;
        _faceDownStartTime = 0;
    }
}
