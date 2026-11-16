#include "AudioManager.h"

AudioManager::AudioManager()
    : _volume(AUDIO_DEFAULT_VOLUME),
      _isPlaying(false),
      _noteStartTime(0),
      _currentNoteDuration(0),
      _stepIndex(0),
      _stepCount(0) {
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

void AudioManager::update(uint32_t nowMillis) {
    if (!M5.Speaker.isEnabled() || !_isPlaying) return;

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
}

void AudioManager::beepSelect() {
    static const NoteStep selectStep[] = {
        { 2400, 35 }
    };
    playSequence(selectStep, 1);
}

void AudioManager::beepAlert() {
    // Classic 3-tone Bandai V-Pet call alert: High, Medium, Higher with pause
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
    // Low aggressive frequency sweep for battle / shake
    static const NoteStep roarSteps[] = {
        { 340, 80 },
        { 260, 90 },
        { 190, 100 },
        { 130, 120 }
    };
    playSequence(roarSteps, 4);
}

void AudioManager::beepLevelUp() {
    // Triumphant arpeggio fanfare (C5, E5, G5, C6, E6)
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
    // Faint bedtime gentle buzz
    static const NoteStep snoreStep[] = {
        { 110, 250 }
    };
    playSequence(snoreStep, 1);
}

void AudioManager::beepSecretReward() {
    // Star sparkle reward arpeggio
    static const NoteStep starSteps[] = {
        { 1318, 60 },
        { 1568, 60 },
        { 2093, 140 }
    };
    playSequence(starSteps, 3);
}

void AudioManager::beepEat() {
    // Munching crunch chirp
    static const NoteStep eatSteps[] = {
        { 880,  40 },
        { 0,    20 },
        { 1174, 50 }
    };
    playSequence(eatSteps, 3);
}

void AudioManager::beepJump() {
    // Rising hop sound
    static const NoteStep jumpSteps[] = {
        { 480, 40 },
        { 760, 60 }
    };
    playSequence(jumpSteps, 2);
}
