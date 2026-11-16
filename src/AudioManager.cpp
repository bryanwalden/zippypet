#include "AudioManager.h"

AudioManager::AudioManager()
    : _volume(AUDIO_DEFAULT_VOLUME),
      _isPlaying(false),
      _noteStartTime(0),
      _currentNoteDuration(0),
      _stepIndex(0),
      _stepCount(0),
      _lastMicSampleTime(0),
      _lastClapTime(0),
      _voiceTriggered(false),
      _voiceLevel(0) {
    memset(_sequence, 0, sizeof(_sequence));
}

void AudioManager::begin() {
    if (M5.Speaker.isEnabled()) {
        M5.Speaker.begin();
        M5.Speaker.setVolume(_volume);
    }
}

void AudioManager::setVolume(uint8_t vol) {
    _volume = vol;
    if (M5.Speaker.isEnabled()) {
        M5.Speaker.setVolume(_volume);
    }
}

void AudioManager::playSequence(const NoteStep* steps, uint8_t count) {
    if (!M5.Speaker.isEnabled()) return;
    if (count == 0 || count > 8) return;

    // Ensure speaker owns I2S bus before playing tone
    if (M5.Mic.isEnabled()) {
        M5.Mic.end();
    }
    M5.Speaker.begin();
    M5.Speaker.setVolume(_volume);

    memcpy(_sequence, steps, sizeof(NoteStep) * count);
    _stepCount = count;
    _stepIndex = 0;
    _isPlaying = true;
    _noteStartTime = millis();
    _currentNoteDuration = _sequence[0].durationMs;

    if (_sequence[0].freq > 0) {
        M5.Speaker.tone(_sequence[0].freq, _currentNoteDuration);
    } else {
        M5.Speaker.stop();
    }
}

bool AudioManager::popVoiceTrigger() {
    bool v = _voiceTriggered;
    _voiceTriggered = false;
    return v;
}

void AudioManager::update(uint32_t nowMillis) {
    if (_isPlaying) {
        if (!M5.Speaker.isEnabled()) {
            _isPlaying = false;
            return;
        }
        if (nowMillis - _noteStartTime >= _currentNoteDuration) {
            _stepIndex++;
            if (_stepIndex < _stepCount) {
                _noteStartTime = nowMillis;
                _currentNoteDuration = _sequence[_stepIndex].durationMs;
                if (_sequence[_stepIndex].freq > 0) {
                    M5.Speaker.tone(_sequence[_stepIndex].freq, _currentNoteDuration);
                } else {
                    M5.Speaker.stop();
                }
            } else {
                _isPlaying = false;
                M5.Speaker.stop();
            }
        }
        return;
    }

#if !defined(WOKWI_SIM) && !defined(ARDUINO_M5STACK_CORES3)
    // Upgrade 2: Periodic non-blocking Microphone Clap / Voice check when speaker is idle
    if (M5.Mic.isEnabled() && (nowMillis - _lastMicSampleTime >= 260) && (nowMillis - _noteStartTime >= 400)) {
        _lastMicSampleTime = nowMillis;
        int16_t samples[32];
        M5.Speaker.end();
        M5.Mic.begin();
        if (M5.Mic.record(samples, 32, 16000)) {
            int32_t peak = 0;
            for (int i = 0; i < 32; ++i) {
                int32_t v = abs((int32_t)samples[i]);
                if (v > peak) peak = v;
            }
            _voiceLevel = (uint8_t)min((int32_t)100, peak / 220);
            if (peak > 8500 && (nowMillis - _lastClapTime > 1800)) {
                _lastClapTime = nowMillis;
                _voiceTriggered = true;
            }
        }
        M5.Mic.end();
        M5.Speaker.begin();
        M5.Speaker.setVolume(_volume);
    }
#endif
}

void AudioManager::beepSelect() {
    static const NoteStep selectStep[] = {
        { 2400, 35 }
    };
    playSequence(selectStep, 1);
}

void AudioManager::beepAlert() {
    static const NoteStep alertSteps[] = {
        { 1046, 90 }, // C6
        { 0,    40 },
        { 1318, 90 }, // E6
        { 0,    40 },
        { 1568, 140 } // G6
    };
    playSequence(alertSteps, 5);
}

void AudioManager::beepRoar() {
    static const NoteStep roarSteps[] = {
        { 340, 80 },
        { 260, 90 },
        { 190, 100 },
        { 130, 120 }
    };
    playSequence(roarSteps, 4);
}

void AudioManager::beepLevelUp() {
    static const NoteStep fanSteps[] = {
        { 523,  70 },  // C5
        { 659,  70 },  // E5
        { 784,  70 },  // G5
        { 1046, 80 },  // C6
        { 1318, 180 }  // E6
    };
    playSequence(fanSteps, 5);
}

void AudioManager::beepSnore() {
    static const NoteStep snoreStep[] = {
        { 110, 250 }
    };
    playSequence(snoreStep, 1);
}

void AudioManager::beepSecretReward() {
    static const NoteStep starSteps[] = {
        { 1318, 60 },
        { 1568, 60 },
        { 2093, 140 }
    };
    playSequence(starSteps, 3);
}

void AudioManager::beepEat() {
    static const NoteStep eatSteps[] = {
        { 880,  40 },
        { 0,    20 },
        { 1174, 50 }
    };
    playSequence(eatSteps, 3);
}

void AudioManager::beepJump() {
    static const NoteStep jumpSteps[] = {
        { 480, 40 },
        { 760, 60 }
    };
    playSequence(jumpSteps, 2);
}
