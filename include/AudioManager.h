#pragma once

#include <Arduino.h>
#include <M5Unified.h>
#include "Config.h"

struct NoteStep {
    uint16_t freq;
    uint16_t durationMs;
};

class AudioManager {
public:
    AudioManager();

    void begin();
    void setVolume(uint8_t vol);
    void update(uint32_t nowMillis);

    // Required sound effects
    void beepSelect();
    void beepAlert();
    void beepRoar();
    void beepLevelUp();

    // Additional atmospheric effects
    void beepSnore();
    void beepSecretReward();
    void beepEat();
    void beepJump();

    bool isPlaying() const { return _isPlaying; }

private:
    uint8_t _volume;
    bool _isPlaying;
    uint32_t _noteStartTime;
    uint16_t _currentNoteDuration;
    uint8_t _stepIndex;
    uint8_t _stepCount;
    NoteStep _sequence[8];

    void playSequence(const NoteStep* steps, uint8_t count);
};
