#include "PowerManager.h"
#include <driver/gpio.h>

PowerManager::PowerManager()
    : _lastInteractionTime(0),
      _targetBrightness(128) {}

void PowerManager::begin() {
    _lastInteractionTime = millis();
}

void PowerManager::recordInteraction(uint32_t nowMillis) {
    _lastInteractionTime = nowMillis;
}

void PowerManager::performLightSleep(DisplayManager& display, PetStateManager& petState) {
    // 1. Save pet state to NVS before sleeping
    petState.save();

    // 2. Gradually dim LCD brightness to 0
    uint8_t currentB = display.getBrightness();
    if (currentB > 0) {
        for (int b = currentB; b >= 0; b -= 8) {
            display.setBrightness(max(0, b));
            delay(15);
        }
    }

    // 3. Enter M5Unified light sleep with periodic timer wakeup and button/touch wakeup
    M5.Power.lightSleep((uint64_t)LIGHT_SLEEP_WAKE_INTERVAL_S * 1000000ULL, true);

    // === WAKING UP ===
    _lastInteractionTime = millis();

    // 4. Restore display brightness smoothly if not in full bedtime sleep
    if (!petState.getState().isSleeping) {
        for (int b = 0; b <= _targetBrightness; b += 8) {
            display.setBrightness(b);
            delay(10);
        }
    }
}

bool PowerManager::checkInactivity(uint32_t nowMillis, DisplayManager& display, PetStateManager& petState) {
#if defined(ARDUINO_M5STACK_CORES3) || defined(ZP_DEBUG_NO_SLEEP)
    (void)nowMillis;
    (void)display;
    (void)petState;
    return false; // No inactivity sleep in simulator / debug builds
#else
    if (nowMillis - _lastInteractionTime >= INACTIVITY_SLEEP_TIMEOUT_MS) {
        performLightSleep(display, petState);
        return true;
    }
    return false;
#endif
}

void PowerManager::enterBedtimeSleep(DisplayManager& display, PetStateManager& petState) {
    petState.setSleep(true);
    performLightSleep(display, petState);
}
