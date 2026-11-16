#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include "Config.h"
#include "DisplayManager.h"

#if __has_include("Secrets.h")
#include "Secrets.h"
#elif __has_include("Secrets.example.h")
#include "Secrets.example.h"
#endif

#ifndef WIFI_SSID
#define WIFI_SSID "YOUR_WIFI_SSID"
#endif

#ifndef WIFI_PASS
#define WIFI_PASS "YOUR_WIFI_PASSWORD"
#endif

class GitHubOtaManager {
public:
    GitHubOtaManager();

    // Cold boot check: connects to Wi-Fi with strict 5s timeout, pulls version.json from GitHub,
    // streams firmware.bin if newer version found, and shuts down Wi-Fi radio immediately.
    void checkAndPerformUpdate(DisplayManager* displayMgr);

private:
    bool connectWifi(uint32_t timeoutMs = 5000);
    int checkRemoteVersion(WiFiClientSecure& client);
    bool streamFirmwareUpdate(DisplayManager* displayMgr, const char* binaryUrl);
};

