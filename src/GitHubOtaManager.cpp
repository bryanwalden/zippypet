#include "GitHubOtaManager.h"
#include <Preferences.h>
#include <esp_system.h>

[[maybe_unused]] static const char* GITHUB_VERSION_URL = "https://github.com/bryanwalden/zippypet/releases/latest/download/version.json";
[[maybe_unused]] static const char* GITHUB_FIRMWARE_URL = "https://github.com/bryanwalden/zippypet/releases/latest/download/firmware.bin";
static const char* WIFI_NVS_NAMESPACE = "zp_wifi";

GitHubOtaManager::GitHubOtaManager() {}

void GitHubOtaManager::checkAndPerformUpdate(DisplayManager* displayMgr) {
#if defined(WOKWI_SIM) || defined(ARDUINO_M5STACK_CORES3)
    Serial.println("[OTA] Simulation detected (Wokwi/CoreS3) - skipping GitHub OTA check.");
    return;
#else
    // Cold boot only: Skip if waking up from deep sleep
    if (esp_reset_reason() == ESP_RST_DEEPSLEEP) {
        Serial.println("[OTA] Wake from DeepSleep - skipping GitHub OTA check.");
        return;
    }

    Serial.printf("[OTA] Cold boot detected. Local Firmware Version: %d\n", FIRMWARE_VERSION);

    // Connect to Wi-Fi with strict 5-second timeout for seamless offline startup
    if (!connectWifi(5000)) {
        Serial.println("[OTA] Wi-Fi offline or timed out after 5s. Proceeding to normal gameplay.");
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        return;
    }

    Serial.println("[OTA] Connected to Wi-Fi. Checking for updates on GitHub...");

    WiFiClientSecure client;
    client.setInsecure(); // GitHub uses standard HTTPS; setInsecure avoids certificate bundle overhead

    int remoteVersion = checkRemoteVersion(client);
    Serial.printf("[OTA] Remote Version: %d (Local: %d)\n", remoteVersion, FIRMWARE_VERSION);

    if (remoteVersion > FIRMWARE_VERSION) {
        Serial.printf("[OTA] New version %d available! Starting firmware stream...\n", remoteVersion);
        if (displayMgr) {
            displayMgr->showUpdateScreen("POWERING UP DIGI-CORE...", "DOWNLOADING ROM", 0, 100);
        }

        streamFirmwareUpdate(displayMgr, GITHUB_FIRMWARE_URL);
    } else {
        Serial.println("[OTA] Firmware is up to date.");
    }

    // Disconnect Wi-Fi immediately to conserve the 250mAh battery during gameplay
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    Serial.println("[OTA] Wi-Fi radio powered off.");
#endif
}

bool GitHubOtaManager::connectWifi(uint32_t timeoutMs) {
    Preferences prefs;
    prefs.begin(WIFI_NVS_NAMESPACE, false);

    String ssid = WIFI_SSID;
    String pass = WIFI_PASS;

    if (ssid.length() > 0 && ssid != "YOUR_WIFI_SSID") {
        // Persist valid compile-time credentials to NVS so future GitHub OTA builds inherit them
        prefs.putString("ssid", ssid);
        prefs.putString("pass", pass);
    } else {
        // Running a GitHub Actions OTA binary: load stored Wi-Fi credentials from NVS
        ssid = prefs.getString("ssid", "");
        pass = prefs.getString("pass", "");
    }
    prefs.end();

    if (ssid.length() == 0 || ssid == "YOUR_WIFI_SSID") {
        Serial.println("[OTA] No Wi-Fi credentials in Secrets.h or NVS. Skipping OTA check.");
        return false;
    }

    Serial.printf("[OTA] Attempting Wi-Fi connect to '%s' (%ums timeout)...\n", ssid.c_str(), (unsigned)timeoutMs);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());

    uint32_t startTime = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - startTime < timeoutMs)) {
        delay(100);
    }
    return (WiFi.status() == WL_CONNECTED);
}

int GitHubOtaManager::checkRemoteVersion(WiFiClientSecure& client) {
    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(4000);

    if (!http.begin(client, GITHUB_VERSION_URL)) {
        Serial.println("[OTA] Failed to begin HTTP connection to version.json");
        return -1;
    }

    int httpCode = http.GET();
    int remoteVer = -1;

    if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY || httpCode == HTTP_CODE_FOUND) {
        String payload = http.getString();
        Serial.printf("[OTA] Version payload received: %s\n", payload.c_str());

        // Parse {"version": N}
        int verIdx = payload.indexOf("\"version\"");
        if (verIdx != -1) {
            int colonIdx = payload.indexOf(':', verIdx);
            if (colonIdx != -1) {
                remoteVer = payload.substring(colonIdx + 1).toInt();
            }
        }
    } else {
        Serial.printf("[OTA] version.json request failed, HTTP Code: %d\n", httpCode);
    }

    http.end();
    return remoteVer;
}

bool GitHubOtaManager::streamFirmwareUpdate(DisplayManager* displayMgr, const char* binaryUrl) {
    WiFiClientSecure updateClient;
    updateClient.setInsecure();

    httpUpdate.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    httpUpdate.rebootOnUpdate(true); // Automatically reboot when completed

    httpUpdate.onProgress([displayMgr](int current, int total) {
        if (total > 0 && displayMgr) {
            displayMgr->showUpdateScreen("POWERING UP DIGI-CORE...", "UPDATING FIRMWARE", current, total);
        }
    });

    t_httpUpdate_return ret = httpUpdate.update(updateClient, binaryUrl);

    switch (ret) {
        case HTTP_UPDATE_FAILED:
            Serial.printf("[OTA] HTTP Update failed! Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
            return false;
        case HTTP_UPDATE_NO_UPDATES:
            Serial.println("[OTA] HTTP Update: No updates.");
            return false;
        case HTTP_UPDATE_OK:
            Serial.println("[OTA] HTTP Update OK! Rebooting...");
            return true;
    }
    return false;
}
