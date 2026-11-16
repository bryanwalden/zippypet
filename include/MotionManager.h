#pragma once

#include <Arduino.h>
#include <M5Unified.h>
#include "Config.h"

enum MotionEvent : uint8_t {
    MOTION_EVENT_NONE = 0,
    MOTION_EVENT_JUMP,
    MOTION_EVENT_ROAR,
    MOTION_EVENT_INVERT_START,
    MOTION_EVENT_INVERT_END,
    MOTION_EVENT_BEDTIME
};

class MotionManager {
public:
    MotionManager();

    bool begin();
    void update(uint32_t nowMillis);

    // Results & queries
    float getTiltBiasX() const { return _tiltBiasX; }
    bool isUpsideDown() const { return _isUpsideDown; }
    bool isFaceDown() const { return _isFaceDown; }
    MotionEvent popEvent();

    // Reset state after sleep
    void reset();

private:
    float _ax, _ay, _az;
    float _prevMag;
    float _tiltBiasX;
    bool _isUpsideDown;
    bool _isFaceDown;
    uint32_t _faceDownStartTime;
    uint32_t _lastJumpTime;
    uint32_t _lastShakeTime;

    // Jump detection state machine
    bool _jumpArmed;
    uint32_t _jumpArmTime;

    MotionEvent _pendingEvent;
};
