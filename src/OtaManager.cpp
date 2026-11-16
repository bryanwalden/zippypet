#include "OtaManager.h"

OtaManager::OtaManager()
    : _display(nullptr),
      _isConnected(false),
      _otaStarted(false),
      _isUpdating(false),
      _lastWifiCheck(0) {}

void OtaManager::begin(DisplayManager* displayMgr) {
    _display = displayMgr;

    // Start Wi-Fi connection in non-blocking mode
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(DEFAULT_WIFI_SSID, DEFAULT_WIFI_PASS);

    // Configure ArduinoOTA parameters
    ArduinoOTA.setHostname(OTA_HOSTNAME);
    ArduinoOTA.setPort(OTA_PORT);

    ArduinoOTA.onStart([this]() {
        _isUpdating = true;
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH) {
            type = "sketch";
        } else {
            type = "filesystem";
        }
        if (_display) {
            _display->showOtaProgress(0, 100);
        }
    });

    ArduinoOTA.onEnd([this]() {
        _isUpdating = false;
        if (_display) {
            _display->showOtaProgress(100, 100);
        }
    });

    ArduinoOTA.onProgress([this](unsigned int progress, unsigned int total) {
        if (_display) {
            _display->showOtaProgress(progress, total);
        }
    });

    ArduinoOTA.onError([this](ota_error_t error) {
        _isUpdating = false;
        (void)error;
    });
}

void OtaManager::update() {
    uint32_t now = millis();

    // Check Wi-Fi status periodically every 3 seconds
    if (now - _lastWifiCheck >= 3000) {
        _lastWifiCheck = now;
        bool connectedNow = (WiFi.status() == WL_CONNECTED);

        if (connectedNow && !_isConnected) {
            _isConnected = true;
            if (!_otaStarted) {
                ArduinoOTA.begin();
                _otaStarted = true;
            }
        } else if (!connectedNow && _isConnected) {
            _isConnected = false;
        }
    }

    if (_otaStarted && _isConnected) {
        ArduinoOTA.handle();
    }
}
