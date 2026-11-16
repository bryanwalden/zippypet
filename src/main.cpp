#include <Arduino.h>
#include <M5Unified.h>
#include <lgfx/v1/panel/Panel_ILI9342.hpp>

#include "Config.h"
#include "PetState.h"
#include "DisplayManager.h"
#include "MotionManager.h"
#include "AudioManager.h"
#include "InputManager.h"
#include "PowerManager.h"
#include "GitHubOtaManager.h"

// Subsystem instances
static PetStateManager petMgr;
static DisplayManager  displayMgr;
static MotionManager   motionMgr;
static AudioManager    audioMgr;
static InputManager    inputMgr;
static PowerManager    powerMgr;
static GitHubOtaManager gitHubOtaMgr;

// Timers for alerts and periodic actions
static uint32_t lastAlertSoundTime = 0;

void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println("\n=================================");
    Serial.println("[BOOT] Zippypet Starting...");
    Serial.println("=================================");

    // Initialize M5Unified hardware abstraction
    auto cfg = M5.config();
#if defined(WOKWI_SIM)
    cfg.internal_imu = false; // Simulator only: prevents I2C probe hang when the IMU is absent
#else
    cfg.internal_imu = true;  // Real M5StickS3: BMI270 IMU drives jump/shake/tilt/pedometer
#endif
#if defined(ARDUINO_M5STACK_CORES3)
    cfg.internal_spk = false; // Prevent AW88298 amplifier probe hang in Wokwi
#else
    cfg.internal_spk = true;
#endif
    cfg.internal_mic = false;
    M5.begin(cfg);

    if (M5.Display.width() == 0) {
        Serial.println("[BOOT] Display: 0x0 detected. Initializing CoreS3 Wokwi panel (320x240)...");
        auto bus = new lgfx::Bus_SPI();
        auto bus_cfg = bus->config();
        bus_cfg.spi_host = SPI2_HOST;
        bus_cfg.freq_write = 40000000;
        bus_cfg.freq_read  = -1;
        bus_cfg.spi_mode = 0;
        bus_cfg.pin_sclk = 36;
        bus_cfg.pin_mosi = 37;
        bus_cfg.pin_miso = -1;
        bus_cfg.pin_dc   = 35;
        bus_cfg.dma_channel = 0;
        bus->config(bus_cfg);

        auto panel = new lgfx::Panel_ILI9342();
        auto panel_cfg = panel->config();
        panel_cfg.pin_cs = 3;
        panel_cfg.pin_rst = -1;
        panel_cfg.panel_width = 320;
        panel_cfg.panel_height = 240;
        panel_cfg.offset_rotation = 0;
        panel_cfg.invert = false;
        panel->config(panel_cfg);
        panel->setBus(bus);

        M5.Display.setPanel(panel);
        panel->init(true);
        M5.Display.setColorDepth(16);
        M5.Display.setBrightness(255);
        M5.Display.fillScreen(COLOR_BLACK);
    }

    Serial.printf("[BOOT] Board ID: %d, Display: %dx%d\n", (int)M5.getBoard(), (int)M5.Display.width(), (int)M5.Display.height());

    displayMgr.begin();
    Serial.println("[BOOT] Display initialized.");

    petMgr.begin();
    motionMgr.begin();
    audioMgr.begin();
    inputMgr.begin();
    powerMgr.begin();

    // Check for GitHub Release OTA on cold boot (Wi-Fi 5s timeout, powers off Wi-Fi after)
    gitHubOtaMgr.checkAndPerformUpdate(&displayMgr);

    audioMgr.beepSelect();
    displayMgr.update(petMgr, 0.0f, millis());
    Serial.println("[BOOT] Pet simulation running!");

    Serial.println("\n[INPUT] Ready! Controls:");
    Serial.println("  - Btn A: Activate Menu / Strike / Cycle Gear in Stats");
    Serial.println("  - Btn B: Cycle 5-Icon Dock (FEED, TRAIN, CLEAN, BATTLE, STATS)");
    Serial.println("  - Btn B Hold (1.5s): Reset to Level 0 (Dino Egg) / Legacy Gen+");
    Serial.println("  - Keys '0' to '7': Switch Stage (0=Egg, 1..7=Levels 1..7)");
    Serial.println("  - 't': Toggle Sky | 'w': Cycle Weather (Clear, Ash, Acid Rain, Eruption)\n");
}

void loop() {
    uint32_t now = millis();

    M5.update();

    inputMgr.update(now);
    motionMgr.update(now);
    audioMgr.update(now);

    // ==========================================
    // 1. Process Input Events
    // ==========================================
    InputEvent inEv = inputMgr.popEvent();
    static uint32_t lastButtonEventMs = 0;
    if (inEv != INPUT_EVENT_NONE) {
        powerMgr.recordInteraction(now);
        lastButtonEventMs = now;
        Serial.printf("[INPUT] event=%d menuOpen=%d statsOpen=%d\n", (int)inEv,
                      (int)displayMgr.isMenuOpen(), (int)displayMgr.isStatsOpen());
    }

    switch (inEv) {
        case INPUT_EVENT_SECRET_REWARD:
            petMgr.rewardDiscipline(10);
            displayMgr.triggerSecretStars();
            audioMgr.beepSecretReward();
            break;

        case INPUT_EVENT_B_SHORT_CLICK:
            if (displayMgr.isMiniGameActive() || displayMgr.isBattleActive()) {
                displayMgr.closeAllOverlays();
                audioMgr.beepSelect();
            } else if (displayMgr.isStatsOpen()) {
                displayMgr.toggleStatsOverlay();
                audioMgr.beepSelect();
            } else if (!displayMgr.isMenuOpen()) {
                displayMgr.setMenuOpen(true);
                displayMgr.cycleMenuItem();
                audioMgr.beepSelect();
            } else {
                displayMgr.cycleMenuItem();
                audioMgr.beepSelect();
            }
            break;

        case INPUT_EVENT_B_LONG_PRESS:
            // Reset pet to Level 0 (Egg) — advances Generation if Level 5+ (Idea 10)
            petMgr.resetToEgg();
            petMgr.save();
            displayMgr.closeAllOverlays();
            displayMgr.setAnimState(ANIM_IDLE);
            if (petMgr.getState().generation > 1) {
                char buf[24];
                snprintf(buf, sizeof(buf), "GEN %d LEGACY EGG!", (int)petMgr.getState().generation);
                displayMgr.showToast(buf);
            } else {
                displayMgr.showToast("RESET TO EGG!");
            }
            audioMgr.beepLevelUp();
            Serial.printf("[RESET] Pet reset to Stage 0 (Egg), Gen %d!\n", (int)petMgr.getState().generation);
            break;

        case INPUT_EVENT_A_CLICK:
            if (displayMgr.isMiniGameActive()) {
                displayMgr.handleMiniGameInput();
                audioMgr.beepJump();
                break;
            }

            if (displayMgr.isBattleActive()) {
                displayMgr.handleBattleInput();
                audioMgr.beepRoar();
                break;
            }

            if (petMgr.getState().isSleeping) {
                petMgr.setSleep(false);
                displayMgr.setAnimState(ANIM_IDLE);
                displayMgr.setBrightness(255);
                audioMgr.beepSelect();
                break;
            }

            // Inside Stats screen, pressing A cycles equipped Relic/Accessory (Idea 7)!
            if (displayMgr.isStatsOpen()) {
                petMgr.cycleAccessory();
                char toastBuf[24];
                snprintf(toastBuf, sizeof(toastBuf), "GEAR: %s", petMgr.getAccessoryName());
                displayMgr.showToast(toastBuf);
                audioMgr.beepSelect();
            } else if (displayMgr.isMenuOpen()) {
                MenuItem item = displayMgr.getSelectedMenuItem();
                Serial.printf("[MENU] A pressed -> select item %d\n", (int)item);
                switch (item) {
                    case MENU_FEED:
                        if (!petMgr.feed(false)) {
                            displayMgr.showToast("FULL! YUM!");
                        }
                        displayMgr.triggerEat(false);
                        audioMgr.beepEat();
                        break;

                    case MENU_TRAIN:
                        displayMgr.startMiniGame();
                        audioMgr.beepSelect();
                        Serial.println("[GAME] Training Mini-Game started!");
                        break;

                    case MENU_CLEAN:
                        if (!petMgr.clean()) {
                            displayMgr.showToast("SPARKLING!");
                        }
                        if (petMgr.getState().isSick) {
                            petMgr.cure();
                        }
                        displayMgr.triggerCleanGeyser();
                        audioMgr.beepSelect();
                        Serial.println("[ACTION] Geyser waterfall bath triggered!");
                        break;

                    case MENU_BATTLE:
                        displayMgr.startBattle(petMgr.getState().stage);
                        audioMgr.beepRoar();
                        Serial.println("[BATTLE] Wireless ESP-NOW / Rival Dino Battle started!");
                        break;

                    case MENU_STATS:
                        displayMgr.toggleStatsOverlay();
                        audioMgr.beepSelect();
                        break;

                    default:
                        break;
                }
            } else {
                displayMgr.triggerJump();
                audioMgr.beepJump();
            }
            break;

        case INPUT_EVENT_STAGE_0:
            petMgr.resetToEgg();
            petMgr.save();
            displayMgr.closeAllOverlays();
            displayMgr.setAnimState(ANIM_IDLE);
            displayMgr.showToast("RESET TO EGG!");
            audioMgr.beepLevelUp();
            Serial.println("[EVO] Reset to Level 0: Egg");
            break;

        case INPUT_EVENT_STAGE_1:
        case INPUT_EVENT_STAGE_2:
        case INPUT_EVENT_STAGE_3:
        case INPUT_EVENT_STAGE_4:
        case INPUT_EVENT_STAGE_5:
        case INPUT_EVENT_STAGE_6:
        case INPUT_EVENT_STAGE_7: {
            uint8_t targetStage = (uint8_t)(inEv - INPUT_EVENT_STAGE_1 + 1);
            petMgr.getStateMutable().stage = targetStage;
            petMgr.getStateMutable().species = targetStage - 1;
            petMgr.checkUnlocks();
            petMgr.save();
            displayMgr.triggerEvolutionSequence(targetStage);
            audioMgr.beepLevelUp();
            Serial.printf("[EVO] Level changed to %d: %s (%s)\n", targetStage, petMgr.getSpeciesName(), petMgr.getStageName());
            break;
        }

        case INPUT_EVENT_TOGGLE_SKY:
            displayMgr.toggleSkyTime();
            Serial.println("[SKY] Toggled day/night sky cycle!");
            break;

        case INPUT_EVENT_CYCLE_WEATHER:
            displayMgr.cycleWeather();
            audioMgr.beepSelect();
            Serial.println("[WEATHER] Cycled prehistoric weather!");
            break;

        case INPUT_EVENT_ACTION_ROAR:
            displayMgr.triggerRoar();
            audioMgr.beepRoar();
            break;

        case INPUT_EVENT_ACTION_JUMP:
            displayMgr.triggerJump();
            audioMgr.beepJump();
            break;

        case INPUT_EVENT_ACTION_EAT:
            displayMgr.triggerEat(false);
            audioMgr.beepEat();
            break;

        case INPUT_EVENT_ACTION_SLEEP:
            if (petMgr.getState().isSleeping) {
                petMgr.setSleep(false);
                displayMgr.setAnimState(ANIM_IDLE);
            } else {
                petMgr.setSleep(true);
                displayMgr.setAnimState(ANIM_SLEEP);
            }
            break;

        case INPUT_EVENT_ACTION_HAPPY:
            displayMgr.triggerSecretStars();
            audioMgr.beepSecretReward();
            break;

        default:
            break;
    }

    // ==========================================
    // 2. Process Motion & IMU Events
    // ==========================================
    MotionEvent mEv = motionMgr.popEvent();
    if ((mEv == MOTION_EVENT_JUMP || mEv == MOTION_EVENT_ROAR || mEv == MOTION_EVENT_STEP) && (now - lastButtonEventMs) < 600) {
        mEv = MOTION_EVENT_NONE;
    }
    if (mEv != MOTION_EVENT_NONE) {
        powerMgr.recordInteraction(now);
    }

    switch (mEv) {
        case MOTION_EVENT_STEP:
            // Upgrade 1: Pedometer Step Counter + Fossil Loot Drops
            if (petMgr.recordStep()) {
                displayMgr.showToast("FOSSIL FOUND! +EXP");
                displayMgr.triggerSecretStars();
                audioMgr.beepSecretReward();
            }
            break;

        case MOTION_EVENT_JUMP:
            if (displayMgr.isBattleActive()) {
                displayMgr.handleBattleInput();
                audioMgr.beepRoar();
            } else if (!petMgr.getState().isSleeping && displayMgr.getAnimState() == ANIM_IDLE) {
                displayMgr.triggerJump();
                audioMgr.beepJump();
            }
            break;

        case MOTION_EVENT_ROAR:
            if (displayMgr.isBattleActive()) {
                displayMgr.handleBattleInput();
                audioMgr.beepRoar();
            } else if (!petMgr.getState().isSleeping && displayMgr.getAnimState() == ANIM_IDLE) {
                // Shaking the stick also cycles prehistoric weather when in Eruption or triggers Roar!
                displayMgr.triggerRoar();
                audioMgr.beepRoar();
            }
            break;

        case MOTION_EVENT_INVERT_START:
            displayMgr.setAnimState(ANIM_FALLING);
            break;

        case MOTION_EVENT_INVERT_END:
            if (displayMgr.getAnimState() == ANIM_FALLING) {
                displayMgr.setAnimState(ANIM_IDLE);
            }
            break;

        case MOTION_EVENT_BEDTIME:
            audioMgr.beepSnore();
            powerMgr.enterBedtimeSleep(displayMgr, petMgr);
            break;

        default:
            break;
    }

    // ==========================================
    // 3. Pet State Simulation, Growth & Auto-Save
    // ==========================================
    petMgr.tick(now);
    petMgr.checkAutoSave(now);

    if (petMgr.hasPendingEvolution()) {
        petMgr.clearPendingEvolution();
        displayMgr.triggerEvolutionSequence(petMgr.getState().stage);
        audioMgr.beepLevelUp();
    }

    if (petMgr.needsAlert() && (now - lastAlertSoundTime >= 60000)) {
        lastAlertSoundTime = now;
        audioMgr.beepAlert();
    }

    // ==========================================
    // 4. Inactivity & Light Sleep Check
    // ==========================================
    if (fabsf(motionMgr.getTiltBiasX()) > 0.2f) {
        powerMgr.recordInteraction(now);
    }
    powerMgr.checkInactivity(now, displayMgr, petMgr);

    // ==========================================
    // 5. Render Display Canvas Frame
    // ==========================================
    displayMgr.update(petMgr, motionMgr.getTiltBiasX(), now);

    delay(16);
}
