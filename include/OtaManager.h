#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include "Config.h"
#include "DisplayManager.h"

class OtaManager {
public:
    OtaManager();

    void begin(DisplayManager* displayMgr);
    void update();
    bool isConnected() const { return _isConnected; }
    bool isUpdating() const { return _isUpdating; }

private:
    DisplayManager* _display;
    bool _isConnected;
    bool _otaStarted;
    bool _isUpdating;
    uint32_t _lastWifiCheck;
};
