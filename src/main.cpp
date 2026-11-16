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
    cfg.internal_imu = true;  // Real M5StickS3: BMI270 IMU drives jump/shake/tilt
#endif
#if defined(ARDUINO_M5STACK_CORES3)
    cfg.internal_spk = false; // Prevent AW88298 amplifier probe hang in Wokwi
#else
    cfg.internal_spk = true;
#endif
    cfg.internal_mic = false;
    M5.begin(cfg);

    // In Wokwi simulation, M5Unified autodetect falls back to AtomS3Lite / StampS3 (Display: 0x0)
    // because AW9523 & AXP2101 power management ICs are not simulated.
    // Manually attach CoreS3 SPI ILI9342 panel in native 320x240 landscape:
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
        bus_cfg.pin_miso = -1; // Must be -1 to prevent pin conflict with DC
        bus_cfg.pin_dc   = 35; // Pin 35 is DC
        bus_cfg.dma_channel = 0; // Disable DMA in simulation to prevent 32KB buffer truncation in Wokwi!
        bus->config(bus_cfg);

        auto panel = new lgfx::Panel_ILI9342();
        auto panel_cfg = panel->config();
        panel_cfg.pin_cs = 3;
        panel_cfg.pin_rst = -1;
        panel_cfg.panel_width = 320;
        panel_cfg.panel_height = 240;
        panel_cfg.offset_rotation = 0; // No offset; rotation is chosen once, in DisplayManager::begin()
        panel_cfg.invert = false;
        panel->config(panel_cfg);
        panel->setBus(bus);

        M5.Display.setPanel(panel);
        panel->init(true);
        // NOTE: no setRotation() here - DisplayManager::begin() owns rotation.
        M5.Display.setColorDepth(16);
        M5.Display.setBrightness(255);
        M5.Display.fillScreen(COLOR_BLACK);
    }

    Serial.printf("[BOOT] Board ID: %d, Display: %dx%d\n", (int)M5.getBoard(), (int)M5.Display.width(), (int)M5.Display.height());

    // Initialize display first
    displayMgr.begin();
    Serial.println("[BOOT] Display initialized.");

    // Initialize all modular subsystems
    petMgr.begin();
    motionMgr.begin();
    audioMgr.begin();
    inputMgr.begin();
    powerMgr.begin();

    // Check for GitHub Release OTA on cold boot (Wi-Fi 5s timeout, powers off Wi-Fi after)
    gitHubOtaMgr.checkAndPerformUpdate(&displayMgr);

    // Play startup chirp if audio is enabled
    audioMgr.beepSelect();

    // Render initial frame immediately so display is visible at boot
    displayMgr.update(petMgr, 0.0f, millis());
    Serial.println("[BOOT] Pet simulation running!");

    Serial.println("\n[INPUT] Ready! Controls:");
    Serial.println("  - Btn A (Red Button / GPIO 1 / 'a'): Select / Poke / Confirm");
    Serial.println("  - Btn B (Blue Button / GPIO 2 / 'b'): Open Menu / Cycle Item");
    Serial.println("  - Btn B Hold (Hold Blue Button / 'h'): Dismiss / Return to Idle");
    Serial.println("  - Keys '1' to '7': Switch Evolution Level (Lv1 Baby to Lv7 Mecha Titan)");
    Serial.println("  - 'r': Roar (Attack / Laser blast)");
    Serial.println("  - 'e': Eat (Food / Energy core)");
    Serial.println("  - 'j': Jump");
    Serial.println("  - 'z': Sleep / Wake toggle");
    Serial.println("  - 'p': Happy celebration");
    Serial.println("  - 't': Toggle Prehistoric Sky (Dawn, Day, Sunset, Night with Stars)");
    Serial.println("  - 's': Secret Parent Reward (+10 Disc)\n");
}

void loop() {
    uint32_t now = millis();

    // Refresh M5 hardware states (buttons, imu, power)
    M5.update();

    // Update input, motion, and audio subsystems
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
            // Holding Btn B while double-clicking Btn A
            petMgr.rewardDiscipline(10);
            displayMgr.triggerSecretStars();
            audioMgr.beepSecretReward();
            break;

        case INPUT_EVENT_B_SHORT_CLICK:
            if (displayMgr.isMiniGameActive()) {
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
            // Return to Idle / Dismiss menu
            displayMgr.closeAllOverlays();
            audioMgr.beepSelect();
            break;

        case INPUT_EVENT_A_CLICK:
            // If training mini-game is active, handle strike timing
            if (displayMgr.isMiniGameActive()) {
                displayMgr.handleMiniGameInput();
                audioMgr.beepJump();
                break;
            }

            // Wake pet if sleeping
            if (petMgr.getState().isSleeping) {
                petMgr.setSleep(false);
                displayMgr.setAnimState(ANIM_IDLE);
                displayMgr.setBrightness(255);
                audioMgr.beepSelect();
                break;
            }

            if (displayMgr.isStatsOpen()) {
                displayMgr.toggleStatsOverlay();
                audioMgr.beepSelect();
            } else if (displayMgr.isMenuOpen()) {
                // Select highlighted HUD item (keep menu active so A always activates the highlighted icon)
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
                        Serial.println("[GAME] Meteor Strike Training started!");
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

                    case MENU_STATS:
                        displayMgr.toggleStatsOverlay();
                        audioMgr.beepSelect();
                        break;

                    default:
                        break;
                }
            } else {
                // Normal view: Interact / Jump
                displayMgr.triggerJump();
                audioMgr.beepJump();
            }
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
            displayMgr.triggerEvolutionSequence(targetStage);
            audioMgr.beepLevelUp();
            Serial.printf("[EVO] Level changed to %d: %s (%s)\n", targetStage, petMgr.getSpeciesName(), petMgr.getStageName());
            break;
        }

        case INPUT_EVENT_TOGGLE_SKY:
            displayMgr.toggleSkyTime();
            Serial.println("[SKY] Toggled day/night sky cycle!");
            break;

        case INPUT_EVENT_ACTION_ROAR:
            displayMgr.triggerRoar();
            audioMgr.beepRoar();
            Serial.println("[ACTION] Roar blast!");
            break;

        case INPUT_EVENT_ACTION_JUMP:
            displayMgr.triggerJump();
            audioMgr.beepJump();
            Serial.println("[ACTION] Jump!");
            break;

        case INPUT_EVENT_ACTION_EAT:
            displayMgr.triggerEat(false);
            audioMgr.beepEat();
            Serial.println("[ACTION] Eat!");
            break;

        case INPUT_EVENT_ACTION_SLEEP:
            if (petMgr.getState().isSleeping) {
                petMgr.setSleep(false);
                displayMgr.setAnimState(ANIM_IDLE);
                Serial.println("[ACTION] Woke up!");
            } else {
                petMgr.setSleep(true);
                displayMgr.setAnimState(ANIM_SLEEP);
                Serial.println("[ACTION] Sleeping...");
            }
            break;

        case INPUT_EVENT_ACTION_HAPPY:
            displayMgr.triggerSecretStars();
            audioMgr.beepSecretReward();
            Serial.println("[ACTION] Happy celebration!");
            break;

        default:
            break;
    }

    // ==========================================
    // 2. Process Motion & IMU Events
    // ==========================================
    MotionEvent mEv = motionMgr.popEvent();
    // A button tap jolts the stick: ignore IMU jump/roar for 600ms after any button event
    if ((mEv == MOTION_EVENT_JUMP || mEv == MOTION_EVENT_ROAR) && (now - lastButtonEventMs) < 600) {
        mEv = MOTION_EVENT_NONE;
    }
    if (mEv != MOTION_EVENT_NONE) {
        powerMgr.recordInteraction(now);
    }

    switch (mEv) {
        case MOTION_EVENT_JUMP:
            if (!petMgr.getState().isSleeping && displayMgr.getAnimState() == ANIM_IDLE) {
                displayMgr.triggerJump();
                audioMgr.beepJump();
            }
            break;

        case MOTION_EVENT_ROAR:
            if (!petMgr.getState().isSleeping && displayMgr.getAnimState() == ANIM_IDLE) {
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
            // Put pet to sleep and enter power saving
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

    // Evolution trigger check
    if (petMgr.hasPendingEvolution()) {
        petMgr.clearPendingEvolution();
        displayMgr.setAnimState(ANIM_EVOLVING);
        audioMgr.beepLevelUp();
    }

    // Periodic attention alert call sound if hunger == 0 or sick
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

    // Frame pacing (~60 FPS)
    delay(16);
}
