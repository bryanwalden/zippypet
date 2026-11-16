#include "InputManager.h"

InputManager::InputManager()
    : _pendingEvent(INPUT_EVENT_NONE),
      _interactedThisFrame(false),
      _lastBtnAClickTime(0),
      _btnAClickStreak(0),
      _bHandledAsHoldOrCombo(false),
      _bPressStartTime(0),
      _extALastState(false),
      _extBLastState(false),
      _extBPressStartTime(0),
      _extBHandledAsHold(false) {}

void InputManager::begin() {
    // M5Unified button configuration
    M5.BtnA.setHoldThresh(1500);
    M5.BtnB.setHoldThresh(1500);

    // External GPIO pins for Wokwi simulation pushbuttons (GPIO 1 for Btn A, GPIO 2 for Btn B).
    // Simulator only: on a real M5StickS3 those pins may be wired to on-board peripherals.
#if defined(WOKWI_SIM)
    pinMode(1, INPUT_PULLUP);
    pinMode(2, INPUT_PULLUP);
#endif
}

InputEvent InputManager::popEvent() {
    InputEvent ev = _pendingEvent;
    _pendingEvent = INPUT_EVENT_NONE;
    return ev;
}

void InputManager::update(uint32_t nowMillis) {
    _interactedThisFrame = false;

    // ==========================================
    // 0. Serial Monitor Key Commands (for Wokwi / Headless)
    // ==========================================
    if (Serial.available()) {
        char c = (char)Serial.read();
        if (c == 'a' || c == 'A') {
            _pendingEvent = INPUT_EVENT_A_CLICK;
            _interactedThisFrame = true;
            return;
        } else if (c == 'b' || c == 'B') {
            _pendingEvent = INPUT_EVENT_B_SHORT_CLICK;
            _interactedThisFrame = true;
            return;
        } else if (c == 'h' || c == 'H') {
            _pendingEvent = INPUT_EVENT_B_LONG_PRESS;
            _interactedThisFrame = true;
            return;
        } else if (c == 's' || c == 'S') {
            _pendingEvent = INPUT_EVENT_SECRET_REWARD;
            _interactedThisFrame = true;
            return;
        } else if (c >= '0' && c <= '7') {
            _pendingEvent = (InputEvent)(INPUT_EVENT_STAGE_0 + (c - '0'));
            _interactedThisFrame = true;
            return;
        } else if (c == 'r' || c == 'R') {
            _pendingEvent = INPUT_EVENT_ACTION_ROAR;
            _interactedThisFrame = true;
            return;
        } else if (c == 'j' || c == 'J') {
            _pendingEvent = INPUT_EVENT_ACTION_JUMP;
            _interactedThisFrame = true;
            return;
        } else if (c == 'e' || c == 'E') {
            _pendingEvent = INPUT_EVENT_ACTION_EAT;
            _interactedThisFrame = true;
            return;
        } else if (c == 'z' || c == 'Z') {
            _pendingEvent = INPUT_EVENT_ACTION_SLEEP;
            _interactedThisFrame = true;
            return;
        } else if (c == 'p' || c == 'P') {
            _pendingEvent = INPUT_EVENT_ACTION_HAPPY;
            _interactedThisFrame = true;
            return;
        } else if (c == 't' || c == 'T') {
            _pendingEvent = INPUT_EVENT_TOGGLE_SKY;
            _interactedThisFrame = true;
            return;
        } else if (c == 'w' || c == 'W') {
            _pendingEvent = INPUT_EVENT_CYCLE_WEATHER;
            _interactedThisFrame = true;
            return;
        }
    }

    // ==========================================
    // 1. External GPIO Buttons (Wokwi pushbuttons)
    // ==========================================
#if defined(WOKWI_SIM)
    bool extA = (digitalRead(1) == LOW);
    bool extB = (digitalRead(2) == LOW);
#else
    bool extA = false;
    bool extB = false;
#endif

    if (extA && !_extALastState) {
        _pendingEvent = INPUT_EVENT_A_CLICK;
        _interactedThisFrame = true;
        _extALastState = extA;
        return;
    }
    _extALastState = extA;

    if (extB && !_extBLastState) {
        _extBPressStartTime = nowMillis;
        _extBHandledAsHold = false;
    } else if (extB && !_extBHandledAsHold) {
        if (nowMillis - _extBPressStartTime >= 1500) {
            _extBHandledAsHold = true;
            _pendingEvent = INPUT_EVENT_B_LONG_PRESS;
            _interactedThisFrame = true;
            _extBLastState = extB;
            return;
        }
    } else if (!extB && _extBLastState) {
        if (!_extBHandledAsHold) {
            _pendingEvent = INPUT_EVENT_B_SHORT_CLICK;
            _interactedThisFrame = true;
            _extBLastState = extB;
            return;
        }
    }
    _extBLastState = extB;

    // ==========================================
    // 2. M5 Hardware Buttons / Touch Buttons
    // ==========================================
    // Track button B press start time
    if (M5.BtnB.wasPressed()) {
        _bPressStartTime = nowMillis;
        _bHandledAsHoldOrCombo = false;
    }

    // Check Long Press on Btn B (> 1.5 seconds)
    if (M5.BtnB.isPressed() && !_bHandledAsHoldOrCombo) {
        if (nowMillis - _bPressStartTime >= 1500) {
            _bHandledAsHoldOrCombo = true;
            _pendingEvent = INPUT_EVENT_B_LONG_PRESS;
            _interactedThisFrame = true;
            return;
        }
    }

    // Check Secret Combo: Holding Btn B while double-clicking Btn A
    if (M5.BtnB.isPressed()) {
        if (M5.BtnA.wasClicked()) {
            if (_btnAClickStreak >= 1 && (nowMillis - _lastBtnAClickTime < 420)) {
                // Secret combo triggered!
                _pendingEvent = INPUT_EVENT_SECRET_REWARD;
                _btnAClickStreak = 0;
                _bHandledAsHoldOrCombo = true;
                _interactedThisFrame = true;
                return;
            } else {
                _lastBtnAClickTime = nowMillis;
                _btnAClickStreak = 1;
                // Suppress normal Btn A action while B is held
                return;
            }
        }
    } else {
        // Reset streak if B is not pressed
        if (nowMillis - _lastBtnAClickTime >= 420) {
            _btnAClickStreak = 0;
        }
    }

    // Normal Btn A Click (instant response on press)
    if (M5.BtnA.wasPressed()) {
        _pendingEvent = INPUT_EVENT_A_CLICK;
        _interactedThisFrame = true;
        return;
    }

    // Btn B Short Press (instant response on press so cycling feels snappy)
    if (M5.BtnB.wasPressed()) {
        _pendingEvent = INPUT_EVENT_B_SHORT_CLICK;
        _interactedThisFrame = true;
        return;
    }
}
