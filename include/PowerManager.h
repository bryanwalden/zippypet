#pragma once

#include <Arduino.h>
#include <M5Unified.h>
#include <esp_sleep.h>
#include "Config.h"
#include "DisplayManager.h"
#include "PetState.h"

class PowerManager {
public:
    PowerManager();

    void begin();
    void recordInteraction(uint32_t nowMillis);
    bool checkInactivity(uint32_t nowMillis, DisplayManager& display, PetStateManager& petState);
    void enterBedtimeSleep(DisplayManager& display, PetStateManager& petState);

    uint32_t getLastInteractionTime() const { return _lastInteractionTime; }

private:
    uint32_t _lastInteractionTime;
    uint8_t _targetBrightness;

    void performLightSleep(DisplayManager& display, PetStateManager& petState);
};
