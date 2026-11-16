#pragma once

#include <Arduino.h>
#include <M5Unified.h>

enum InputEvent : uint8_t {
    INPUT_EVENT_NONE = 0,
    INPUT_EVENT_A_CLICK,
    INPUT_EVENT_B_SHORT_CLICK,
    INPUT_EVENT_B_LONG_PRESS,
    INPUT_EVENT_SECRET_REWARD,
    INPUT_EVENT_STAGE_1,
    INPUT_EVENT_STAGE_2,
    INPUT_EVENT_STAGE_3,
    INPUT_EVENT_STAGE_4,
    INPUT_EVENT_STAGE_5,
    INPUT_EVENT_STAGE_6,
    INPUT_EVENT_STAGE_7,
    INPUT_EVENT_ACTION_ROAR,
    INPUT_EVENT_ACTION_JUMP,
    INPUT_EVENT_ACTION_EAT,
    INPUT_EVENT_ACTION_SLEEP,
    INPUT_EVENT_ACTION_HAPPY,
    INPUT_EVENT_TOGGLE_SKY
};

class InputManager {
public:
    InputManager();

    void begin();
    void update(uint32_t nowMillis);
    InputEvent popEvent();

    bool hasInteracted() const { return _interactedThisFrame; }

private:
    InputEvent _pendingEvent;
    bool _interactedThisFrame;

    // Secret combo double click detection on Btn A while Btn B is held
    uint32_t _lastBtnAClickTime;
    uint8_t _btnAClickStreak;
    bool _bHandledAsHoldOrCombo;
    uint32_t _bPressStartTime;

    // External GPIO buttons for simulator (GPIO 1 for Btn A, GPIO 2 for Btn B)
    bool _extALastState;
    bool _extBLastState;
    uint32_t _extBPressStartTime;
    bool _extBHandledAsHold;
};
