#include "DisplayManager.h"
#include <math.h>
#include <WiFi.h>
#include <esp_now.h>

// Upgrade 3: Lightweight ESP-NOW peer-to-peer battle packet
struct BattlePacket {
    uint32_t magic;      // 0x5A504254 ("ZPBT")
    uint8_t  senderStage;
    uint8_t  damageDealt;
};

static volatile bool     s_espNowPeerSeen   = false;
static volatile uint8_t  s_espNowPeerStage  = 3;
static volatile uint8_t  s_espNowIncomingDmg = 0;
static bool              s_espNowInitialized = false;

static void onEspNowRecv(const esp_now_recv_info_t* recvInfo, const uint8_t* incomingData, int len) {
    (void)recvInfo;
    if (len == (int)sizeof(BattlePacket)) {
        BattlePacket pkt;
        memcpy(&pkt, incomingData, sizeof(BattlePacket));
        if (pkt.magic == 0x5A504254) {
            s_espNowPeerSeen = true;
            s_espNowPeerStage = constrain(pkt.senderStage, (uint8_t)1, (uint8_t)7);
            s_espNowIncomingDmg += pkt.damageDealt;
        }
    }
}

static void broadcastEspNowBattlePacket(uint8_t stage, uint8_t dmg) {
#if !defined(WOKWI_SIM) && !defined(ARDUINO_M5STACK_CORES3)
    if (!s_espNowInitialized) {
        WiFi.mode(WIFI_STA);
        if (esp_now_init() == ESP_OK) {
            esp_now_register_recv_cb(onEspNowRecv);
            esp_now_peer_info_t peerInfo = {};
            memset(peerInfo.peer_addr, 0xFF, 6);
            peerInfo.channel = 0;
            peerInfo.encrypt = false;
            esp_now_add_peer(&peerInfo);
            s_espNowInitialized = true;
        }
    }
    if (s_espNowInitialized) {
        BattlePacket pkt = { 0x5A504254, stage, dmg };
        uint8_t broadcastAddr[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        esp_now_send(broadcastAddr, (const uint8_t*)&pkt, sizeof(pkt));
    }
#else
    (void)stage;
    (void)dmg;
#endif
}

static void stopEspNowBattleRadio() {
#if !defined(WOKWI_SIM) && !defined(ARDUINO_M5STACK_CORES3)
    if (s_espNowInitialized) {
        esp_now_deinit();
        WiFi.mode(WIFI_OFF);
        s_espNowInitialized = false;
    }
#endif
}

// Upgrade 10: Shiny Palette Color Filter for Generation 2+
static uint16_t applyShinyTint(uint16_t color, uint8_t shinyVariant) {
    if (color == SPRITE_CHROMA_KEY || shinyVariant == 0) {
        return color;
    }
    uint8_t r5 = (color >> 11) & 0x1F;
    uint8_t g6 = (color >> 5)  & 0x3F;
    uint8_t b5 =  color        & 0x1F;

    if (shinyVariant == 1) {
        // Frost / Albino Cerato (Ice-Blue & Silver)
        uint8_t nr = min(31, (r5 + b5) / 2 + 4);
        uint8_t ng = min(63, g6 + 10);
        uint8_t nb = min(31, r5 + 6);
        uint16_t out = (nr << 11) | (ng << 5) | nb;
        return (out == SPRITE_CHROMA_KEY) ? 0xF81E : out;
    } else if (shinyVariant == 2) {
        // Obsidian / Shadow Cerato (Dark Amethyst & Crimson)
        uint8_t nr = min(31, (r5 * 3) / 4 + 3);
        uint8_t ng = g6 / 3;
        uint8_t nb = min(31, (r5 * 2) / 3 + 6);
        uint16_t out = (nr << 11) | (ng << 5) | nb;
        return (out == SPRITE_CHROMA_KEY) ? 0xF81E : out;
    } else {
        // Solar Gold Cerato (Radiant Emerald & Gold)
        uint8_t nr = min(31, r5 + 3);
        uint8_t ng = min(63, (r5 * 2) + 4);
        uint8_t nb = b5 / 2;
        uint16_t out = (nr << 11) | (ng << 5) | nb;
        return (out == SPRITE_CHROMA_KEY) ? 0xF81E : out;
    }
}

DisplayManager::DisplayManager()
    : _canvas(&M5.Display),
      _spriteCanvas(&_canvas),
      _brightness(255),
      _animState(ANIM_IDLE),
      _animStartTime(0),
      _lastFrameTime(0),
      _frameIndex(0),
      _lastWalkTime(0),
      _walkFrameIndex(0),
      _lastRenderTime(0),
      _petX(67.0f),
      _petY(0.0f),
      _petVx(0.40f),
      _facingRight(true),
      _isTurning(false),
      _turnStartTime(0),
      _turnTargetFacingRight(true),
      _jumpPhase(JUMP_PHASE_NONE),
      _jumpPhaseStartTime(0),
      _jumpVy(0.0f),
      _isJumping(false),
      _menuOpen(true),
      _selectedMenu(MENU_FEED),
      _showStats(false),
      _currentSky(SKY_SUNSET),
      _lastSkyCycleTime(0),
      _currentWeather(WEATHER_CLEAR),
      _lastWeatherChangeTime(0),
      _lastThoughtTime(0),
      _thoughtStartTime(0),
      _activeThoughtType(0),
      _miniGameActive(false),
      _miniGameMode(MINIGAME_METEOR_STRIKE),
      _nextMiniGameToggle(0),
      _miniGameStartTime(0),
      _meteorX(67.0f),
      _meteorY(0.0f),
      _meteorVy(2.5f),
      _bonusMeatX(40.0f),
      _bonusMeatY(-40.0f),
      _miniGameScore(0),
      _miniGameRound(0),
      _miniGameStruck(false),
      _miniGameStrikeTime(0),
      _battleActive(false),
      _battleStartTime(0),
      _playerHp(100),
      _rivalHp(100),
      _battleCharge(0),
      _rivalStage(3),
      _lastRivalAttackTime(0),
      _peerDiscovered(false),
      _isCleaningGeyser(false),
      _cleanStartTime(0),
      _isEvolvingSeq(false),
      _evoStartTime(0),
      _evoTargetStage(3) {}

static void pushFrameToDisplay(M5Canvas& canvas) {
#if defined(WOKWI_SIM)
    int x = (M5.Display.width() - SCREEN_WIDTH) / 2;
    if (x < 0) x = 0;
  #if WOKWI_SOFT_FLIP
    canvas.pushRotateZoom(&M5.Display, x + SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f, 180.0f, 1.0f, 1.0f);
    return;
  #else
    canvas.pushSprite(x, 0);
    return;
  #endif
#else
    canvas.pushSprite(0, 0);
#endif
}

void DisplayManager::begin() {
#if defined(WOKWI_SIM)
    M5.Display.setRotation(WOKWI_ROTATION);
#else
    M5.Display.setRotation(SCREEN_ROTATION);
#endif
    Serial.printf("[DISPLAY] rotation=%d size=%dx%d board=%d\n",
                  (int)M5.Display.getRotation(), (int)M5.Display.width(),
                  (int)M5.Display.height(), (int)M5.getBoard());
    M5.Display.setColorDepth(16);
    M5.Display.setBrightness(255);
    M5.Display.fillScreen(COLOR_BLACK);

    _canvas.setColorDepth(16);
    if (psramFound()) {
        _canvas.setPsram(true);
    } else {
        _canvas.setPsram(false);
    }
    _canvas.createSprite(SCREEN_WIDTH, SCREEN_HEIGHT);
    _canvas.setTextWrap(false);

    _spriteCanvas.setColorDepth(16);
    _spriteCanvas.setPsram(false);
    _spriteCanvas.createSprite(SPRITE_WIDTH, SPRITE_HEIGHT);
    _spriteCanvas.setSwapBytes(true);
    _spriteCanvas.setPivot(SPRITE_WIDTH / 2, SPRITE_HEIGHT - 1);
}

void DisplayManager::setBrightness(uint8_t brightness) {
    _brightness = brightness;
    M5.Display.setBrightness(brightness);
}

void DisplayManager::setAnimState(AnimState state) {
    _animState = state;
    _animStartTime = millis();
    _frameIndex = 0;
}

void DisplayManager::triggerRoar() {
    setAnimState(ANIM_ROARING);
}

void DisplayManager::triggerJump() {
    if (_jumpPhase == JUMP_PHASE_NONE) {
        _jumpPhase = JUMP_PHASE_CROUCH_PRE;
        _jumpPhaseStartTime = millis();
        _petY = 0.0f;
        _isJumping = true;
        setAnimState(ANIM_JUMPING);
    }
}

void DisplayManager::triggerSecretStars() {
    setAnimState(ANIM_SECRET_STARS);
}

void DisplayManager::triggerEat(bool isPill) {
    (void)isPill;
    setAnimState(ANIM_EATING);
}

void DisplayManager::triggerCleanGeyser() {
    _isCleaningGeyser = true;
    _cleanStartTime = millis();
}

void DisplayManager::triggerEvolutionSequence(uint8_t targetStage) {
    _isEvolvingSeq = true;
    _evoStartTime = millis();
    _evoTargetStage = targetStage;
    setAnimState(ANIM_EVOLVING);
}

void DisplayManager::toggleSkyTime() {
    _currentSky = (SkyTime)((_currentSky + 1) % SKY_COUNT);
}

void DisplayManager::cycleWeather() {
    _currentWeather = (WeatherType)((_currentWeather + 1) % WEATHER_COUNT);
    switch (_currentWeather) {
        case WEATHER_CLEAR:     showToast("SKY: CLEAR"); break;
        case WEATHER_ASH_FALL:  showToast("VOLCANIC ASH!"); break;
        case WEATHER_ACID_RAIN: showToast("ACID RAIN!"); break;
        case WEATHER_ERUPTION:  showToast("ERUPTION!!"); break;
        default: break;
    }
}

// Upgrade 6: Dual Mini-Game Launcher (Alternates Meteor Strike & Tilt Lava Dodge)
void DisplayManager::startMiniGame() {
    _miniGameActive = true;
    _miniGameMode = (_nextMiniGameToggle % 2 == 0) ? MINIGAME_METEOR_STRIKE : MINIGAME_TILT_LAVA_DODGE;
    _nextMiniGameToggle++;
    _miniGameStartTime = millis();
    _miniGameRound = 1;
    _miniGameScore = 0;
    _meteorX = 25.0f + (rand() % 85);
    _meteorY = -15.0f;
    _meteorVy = (_miniGameMode == MINIGAME_TILT_LAVA_DODGE) ? 2.8f : 2.4f;
    _bonusMeatX = 20.0f + (rand() % 95);
    _bonusMeatY = -55.0f;
    _miniGameStruck = false;
}

void DisplayManager::handleMiniGameInput() {
    if (!_miniGameActive) return;
    uint32_t now = millis();

    if (_miniGameMode == MINIGAME_METEOR_STRIKE) {
        // Strike zone: meteor between y=135 and y=186
        if (_meteorY >= 135.0f && _meteorY <= 186.0f && !_miniGameStruck) {
            _miniGameStruck = true;
            _miniGameStrikeTime = now;
            _miniGameScore++;
            triggerRoar();
        }
    } else {
        // In Tilt Lava Dodge, pressing BtnA triggers an evasive hop!
        triggerJump();
    }
}

// Upgrade 3: Wireless ESP-NOW / IR PvP & Rival Dino Battle
void DisplayManager::startBattle(uint8_t currentStage) {
    _battleActive = true;
    _battleStartTime = millis();
    _lastRivalAttackTime = millis();
    _playerHp = 100;
    _rivalHp = 100;
    _battleCharge = 20;
    _rivalStage = constrain((int)max((uint8_t)1, currentStage) + (rand() % 2), 1, 7);
    s_espNowPeerSeen = false;
    s_espNowIncomingDmg = 0;
    broadcastEspNowBattlePacket(max((uint8_t)1, currentStage), 0);
}

void DisplayManager::handleBattleInput() {
    if (!_battleActive) return;
    _battleCharge += 18;
    triggerRoar();
    // Broadcast attack pulse over ESP-NOW to any nearby M5StickS3!
    broadcastEspNowBattlePacket(_rivalStage, 14);
    if (_battleCharge >= 35) {
        int dmg = 14 + (rand() % 10);
        _rivalHp = max(0, (int)_rivalHp - dmg);
        _battleCharge = 0;
    }
}

void DisplayManager::setMenuOpen(bool open) {
    _menuOpen = open;
    _menuLastActivity = millis();
    if (!open) {
        _showStats = false;
    }
}

void DisplayManager::cycleMenuItem() {
    _selectedMenu = (MenuItem)((_selectedMenu + 1) % MENU_COUNT);
    _menuLastActivity = millis();
}

void DisplayManager::showToast(const char* text, uint32_t durationMs) {
    strncpy(_toastText, text, sizeof(_toastText) - 1);
    _toastText[sizeof(_toastText) - 1] = '\0';
    _toastUntil = millis() + durationMs;
}

void DisplayManager::toggleStatsOverlay() {
    _showStats = !_showStats;
}

void DisplayManager::closeAllOverlays() {
    _showStats = false;
    _miniGameActive = false;
    if (_battleActive) {
        _battleActive = false;
        stopEspNowBattleRadio();
    }
}

// ==========================================
// Full-Color Living Environment Rendering
// ==========================================

void DisplayManager::drawBackground() {
    uint32_t now = millis();

    // Upgrade 5: Volcanic Eruption sky override tint
    bool isErupting = (_currentWeather == WEATHER_ERUPTION);

    switch (_currentSky) {
        case SKY_DAY: {
            for (int y = 0; y < 186; ++y) {
                float t = (float)y / 185.0f;
                uint8_t r = (uint8_t)(isErupting ? (120 + t * 130) : (30  + t * 210));
                uint8_t g = (uint8_t)(isErupting ? (30  + t * 70)  : (130 + t * 90));
                uint8_t b = (uint8_t)(isErupting ? (20  + t * 20)  : (230 - t * 140));
                uint16_t col = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
                _canvas.drawFastHLine(0, y, SCREEN_WIDTH, col);
            }
            _canvas.fillCircle(105, 38, 14, 0xFFE0);
            _canvas.fillCircle(105, 38, 18, 0xFD80);
            _canvas.fillRoundRect(15, 45, 45, 10, 5, 0xFFFF);
            _canvas.fillRoundRect(25, 40, 25, 12, 6, 0xFFFF);
            break;
        }
        case SKY_NIGHT: {
            for (int y = 0; y < 186; ++y) {
                float t = (float)y / 185.0f;
                uint8_t r = (uint8_t)(isErupting ? (80 + t * 120) : (10 + t * 40));
                uint8_t g = (uint8_t)(8  + t * 15);
                uint8_t b = (uint8_t)(35 + t * 25);
                uint16_t col = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
                _canvas.drawFastHLine(0, y, SCREEN_WIDTH, col);
            }
            _canvas.fillCircle(24, 30, 9, 0xFFFF);
            _canvas.fillCircle(27, 28, 8, 0x18C3);

            static const int starCoords[12][2] = {
                {15, 18}, {48, 12}, {85, 24}, {120, 16}, {68, 42},
                {112, 55}, {32, 65}, {95, 78}, {52, 95}, {125, 90},
                {18, 110}, {82, 120}
            };
            for (int i = 0; i < 12; ++i) {
                if (((now / (250 + i * 40)) + i) % 3 != 0) {
                    int sx = starCoords[i][0];
                    int sy = starCoords[i][1];
                    _canvas.drawPixel(sx, sy, 0xFFFF);
                    _canvas.drawPixel(sx - 1, sy, 0x9CD3);
                    _canvas.drawPixel(sx + 1, sy, 0x9CD3);
                    _canvas.drawPixel(sx, sy - 1, 0x9CD3);
                    _canvas.drawPixel(sx, sy + 1, 0x9CD3);
                }
            }
            break;
        }
        case SKY_DAWN: {
            for (int y = 0; y < 186; ++y) {
                float t = (float)y / 185.0f;
                uint8_t r = (uint8_t)(80  + t * 170);
                uint8_t g = (uint8_t)(40  + t * 140);
                uint8_t b = (uint8_t)(120 - t * 40);
                uint16_t col = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
                _canvas.drawFastHLine(0, y, SCREEN_WIDTH, col);
            }
            break;
        }
        case SKY_SUNSET:
        default: {
            for (int y = 0; y < 186; ++y) {
                float t = (float)y / 185.0f;
                uint8_t r = (uint8_t)(45  + t * 210);
                uint8_t g = (uint8_t)(18  + t * 105);
                uint8_t b = (uint8_t)(75  - t * 65);
                uint16_t col = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
                _canvas.drawFastHLine(0, y, SCREEN_WIDTH, col);
            }
            break;
        }
    }

    // Distant smoking volcano silhouette (left peak)
    _canvas.fillTriangle(0, 186, 32, 108, 70, 186, 0x2104);
    _canvas.fillTriangle(26, 114, 32, 108, 38, 114, isErupting ? 0xFFE0 : 0xFA20);

    // Volcano silhouette (right peak)
    _canvas.fillTriangle(65, 186, 108, 122, 135, 186, 0x28C5);
    _canvas.fillTriangle(104, 126, 108, 122, 112, 126, isErupting ? 0xFFE0 : 0xFCE0);

    // Drifting volcanic smoke puffs
    int smokeOffset = (now / 150) % 36;
    _canvas.fillCircle(32 - (smokeOffset / 3), 104 - smokeOffset, 4 + (smokeOffset / 6), 0x51E7);
    _canvas.fillCircle(108 - (smokeOffset / 4), 118 - smokeOffset, 3 + (smokeOffset / 7), 0x51E7);

    // Dynamic fire embers drifting upward
    for (int i = 0; i < 6; ++i) {
        int ex = (int)(20 + (i * 21) + sinf((now + i * 400) * 0.003f) * 12);
        int ey = 180 - ((int)((now / 35) + i * 30) % 170);
        _canvas.drawPixel(ex, ey, 0xFDE0);
        _canvas.drawPixel(ex, ey + 1, 0xFA20);
    }

    // Upgrade 5: Render active weather particles
    drawWeather(now);
}

void DisplayManager::drawWeather(uint32_t nowMillis) {
    if (_currentWeather == WEATHER_CLEAR) return;

    if (_currentWeather == WEATHER_ASH_FALL) {
        // Drifting gray/white volcanic ash flakes
        for (int i = 0; i < 14; ++i) {
            int ax = (int)((i * 19 + (int)(sinf(nowMillis * 0.002f + i) * 10) + SCREEN_WIDTH) % SCREEN_WIDTH);
            int ay = (int)((nowMillis / 28 + i * 17) % 184);
            _canvas.drawPixel(ax, ay, 0xCE59);
            _canvas.drawPixel(ax + 1, ay, 0x9CD3);
        }
    } else if (_currentWeather == WEATHER_ACID_RAIN) {
        // Slanted neon green prehistoric acid rain streaks
        for (int i = 0; i < 12; ++i) {
            int rx = (int)((i * 23 + (nowMillis / 12)) % SCREEN_WIDTH);
            int ry = (int)((nowMillis / 8 + i * 29) % 180);
            _canvas.drawLine(rx, ry, rx - 2, ry + 6, 0x07E0);
        }
    } else if (_currentWeather == WEATHER_ERUPTION) {
        // Erupting lava fountains shooting from both volcano craters!
        for (int i = 0; i < 10; ++i) {
            float phase = fmodf((nowMillis * 0.004f) + i * 0.6f, 3.0f);
            int peakX = (i % 2 == 0) ? 32 : 108;
            int peakY = (i % 2 == 0) ? 108 : 122;
            int spread = (i - 5) * 5;
            int lx = peakX + (int)(spread * phase);
            int ly = peakY - (int)(32.0f * phase - 12.0f * phase * phase);
            if (ly > 0 && ly < 186) {
                _canvas.fillCircle(lx, ly, 2, 0xFFE0);
                _canvas.drawPixel(lx, ly - 2, 0xF800);
            }
        }
    }
}

void DisplayManager::drawGround() {
    for (int y = 186; y < SCREEN_HEIGHT; ++y) {
        float gt = (float)(y - 186) / 53.0f;
        uint8_t r = (uint8_t)(45 - gt * 25);
        uint8_t g = (uint8_t)(38 - gt * 22);
        uint8_t b = (uint8_t)(35 - gt * 20);
        uint16_t col = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
        _canvas.drawFastHLine(0, y, SCREEN_WIDTH, col);
    }

    _canvas.drawFastHLine(0, 186, SCREEN_WIDTH, 0x5248);

    _canvas.drawLine(15, 192, 38, 196, 0xFA20);
    _canvas.drawLine(38, 196, 62, 193, 0xFDE0);
    _canvas.drawLine(82, 195, 105, 198, 0xFA20);
    _canvas.drawLine(105, 198, 124, 194, 0xFDE0);

    _canvas.drawPixel(45, 189, 0x7BCF);
    _canvas.drawPixel(88, 190, 0x7BCF);
    _canvas.drawPixel(112, 188, 0x7BCF);
}

// ==========================================
// Bottom Dock HUD & 5-Icon Menu
// ==========================================

void DisplayManager::drawHud(const PetStateManager& petMgr) {
    const PetState& st = petMgr.getState();

    // Top status micro-bar: Steps walked (Idea 1) & Gen badge (Idea 10)
    _canvas.fillRoundRect(4, 2, SCREEN_WIDTH - 8, 12, 3, 0x10A2);
    _canvas.setTextSize(1);
    _canvas.setTextColor(0x07FF, 0x10A2);
    _canvas.setCursor(8, 4);
    _canvas.printf("STP:%lu", (unsigned long)st.totalSteps);
    _canvas.setTextColor(0xFDE0, 0x10A2);
    _canvas.setCursor(78, 4);
    _canvas.printf("G%d LV%d", (int)st.generation, (int)st.stage);

    // Dock at bottom of screen: y = 212..238
    int dockY = 212;
    int dockH = 26;

    _canvas.fillRoundRect(3, dockY, SCREEN_WIDTH - 6, dockH, 6, 0x10A2);
    _canvas.drawRoundRect(3, dockY, SCREEN_WIDTH - 6, dockH, 6, 0x3186);

    // 5 Icons spaced every 25px: 8, 33, 58, 83, 108
    drawBitmapScaled(8,   dockY + 5, icon_feed,   16, 16, 1, 0xF9C6); // Coral Red Meat
    drawBitmapScaled(33,  dockY + 5, icon_train,  16, 16, 1, 0xFDE0); // Radiant Gold Target
    drawBitmapScaled(58,  dockY + 5, icon_clean,  16, 16, 1, 0x07FF); // Cyan Geyser/Bath
    drawBitmapScaled(83,  dockY + 5, icon_battle, 16, 16, 1, 0xFA20); // Crossed Swords Battle
    drawBitmapScaled(108, dockY + 5, icon_stats,  16, 16, 1, 0xC81F); // Purple Heart/Stats

    int iconX = 8 + ((int)_selectedMenu * 25) - 2;
    if (_menuOpen) {
        bool pulse = ((millis() / 250) % 2) == 0;
        _canvas.drawRoundRect(iconX, dockY + 3, 20, 20, 4, 0xFFFF);
        _canvas.drawRoundRect(iconX - 1, dockY + 2, 22, 22, 5, pulse ? 0xFD80 : 0xFFE0);
    } else {
        _canvas.drawRoundRect(iconX, dockY + 3, 20, 20, 4, 0x4208);
    }

    const char* labelText = "B:MENU  A:POKE";
    uint16_t badgeColor = 0x7BEF;
    if (_menuOpen) {
        switch (_selectedMenu) {
            case MENU_FEED:   labelText = "B:NEXT  A:FEED";   badgeColor = 0xF9C6; break;
            case MENU_TRAIN:  labelText = "B:NEXT  A:TRAIN";  badgeColor = 0xFDE0; break;
            case MENU_CLEAN:  labelText = "B:NEXT  A:CLEAN";  badgeColor = 0x07FF; break;
            case MENU_BATTLE: labelText = "B:NEXT  A:BATTLE"; badgeColor = 0xFA20; break;
            case MENU_STATS:  labelText = "B:NEXT  A:STATS";  badgeColor = 0xC81F; break;
            default: break;
        }
    }

    _canvas.fillRoundRect(4, 192, SCREEN_WIDTH - 8, 17, 4, 0x18C3);
    _canvas.drawRoundRect(4, 192, SCREEN_WIDTH - 8, 17, 4, badgeColor);
    _canvas.fillCircle(11, 200, 3, badgeColor);

    _canvas.setTextColor(0xFFFF, 0x18C3);
    _canvas.setTextSize(1);
    _canvas.setCursor(17, 197);
    _canvas.print(labelText);

    if (petMgr.needsAlert() && ((millis() / 400) % 2 == 0)) {
        drawBitmapScaled(SCREEN_WIDTH - 20, 16, icon_alert, 16, 16, 1, COLOR_ACCENT_RED);
    }
}

void DisplayManager::drawMenu() {}

void DisplayManager::drawStats(const PetStateManager& petMgr) {
    const PetState& state = petMgr.getState();

    _canvas.fillRoundRect(6, 16, SCREEN_WIDTH - 12, 172, 7, 0x18C3);
    _canvas.drawRoundRect(6, 16, SCREEN_WIDTH - 12, 172, 7, 0xFDE0);
    _canvas.drawFastHLine(10, 34, SCREEN_WIDTH - 20, 0x39E8);

    _canvas.setTextColor(0xFDE0, 0x18C3);
    _canvas.setTextSize(1);
    _canvas.setCursor(12, 22);
    _canvas.printf("G%d LV.%d %s", state.generation, state.stage, petMgr.getSpeciesName());

    // Hunger & Strength Hearts
    _canvas.setTextColor(0xFFFF, 0x18C3);
    _canvas.setCursor(12, 40);
    _canvas.print("Hng:");
    for (int i = 0; i < 4; ++i) {
        const uint8_t* hIcon = (i < state.hunger) ? sprite_heart_full : sprite_heart_empty;
        drawBitmapScaled(42 + i * 15, 36, hIcon, 16, 16, 1, 0xF9C6);
    }

    _canvas.setCursor(12, 56);
    _canvas.print("Str:");
    for (int i = 0; i < 4; ++i) {
        const uint8_t* sIcon = (i < state.strength) ? sprite_heart_full : sprite_heart_empty;
        drawBitmapScaled(42 + i * 15, 52, sIcon, 16, 16, 1, 0xFDE0);
    }

    // EXP & Discipline
    _canvas.setCursor(12, 72);
    _canvas.printf("EXP:%u  Disc:%u%%", state.exp, state.discipline);

    // Upgrade 1: Pedometer Steps & Fossils
    _canvas.setTextColor(0x07FF, 0x18C3);
    _canvas.setCursor(12, 86);
    _canvas.printf("Steps:%lu  Fossils:%u", (unsigned long)state.totalSteps, state.fossils);

    // Upgrade 3: PvP / Rival Battle Record
    _canvas.setTextColor(0xFA20, 0x18C3);
    _canvas.setCursor(12, 100);
    _canvas.printf("Battles: %uW - %uL", state.battleWins, state.battleLosses);

    // Upgrade 7: Equipped Relic / Accessory
    _canvas.setTextColor(0xFFE0, 0x18C3);
    _canvas.setCursor(12, 114);
    _canvas.printf("Gear: %s", petMgr.getAccessoryName());

    // Upgrade 10: Shiny Lineage Info
    const char* shinyNames[4] = {"Classic", "Frost Shiny", "Shadow Shiny", "Solar Shiny"};
    _canvas.setTextColor(0xC81F, 0x18C3);
    _canvas.setCursor(12, 128);
    _canvas.printf("Lineage: %s", shinyNames[state.shinyVariant % 4]);

    // Controls hint inside Stats
    _canvas.drawFastHLine(10, 142, SCREEN_WIDTH - 20, 0x39E8);
    _canvas.setTextColor(0x9CD3, 0x18C3);
    _canvas.setCursor(12, 148);
    _canvas.print("A:Wear Gear   B:Exit");
    _canvas.setCursor(12, 162);
    _canvas.print("Hold B (1.5s): Egg");
}

// ==========================================
// Upgrade 7: Pixel Accessories & Auras
// ==========================================

void DisplayManager::drawAccessory(uint8_t acc, int headX, int headY, int centerX, int centerY, uint32_t nowMillis) {
    if (acc == ACC_NONE) return;

    if (acc == ACC_CROWN) {
        // Golden Royal Dino Crown atop head
        int cx = headX - 7;
        int cy = headY - 6;
        _canvas.fillRect(cx, cy + 4, 14, 4, 0xFDE0);
        _canvas.fillTriangle(cx, cy + 4, cx + 2, cy - 2, cx + 5, cy + 4, 0xFFE0);
        _canvas.fillTriangle(cx + 4, cy + 4, cx + 7, cy - 3, cx + 10, cy + 4, 0xFFE0);
        _canvas.fillTriangle(cx + 9, cy + 4, cx + 12, cy - 2, cx + 14, cy + 4, 0xFFE0);
        _canvas.drawPixel(cx + 7, cy + 5, 0xF800); // Ruby gem
    } else if (acc == ACC_CYBER_VISOR) {
        // Glowing Neon Cyan Tactical Visor across eyes
        int vx = headX + (_facingRight ? 0 : -12);
        int vy = headY + 5;
        _canvas.fillRoundRect(vx, vy, 13, 5, 2, 0x07FF);
        _canvas.drawFastHLine(vx + 2, vy + 1, 9, 0xFFFF);
    } else if (acc == ACC_CHEF_HAT) {
        // Puffy White Prehistoric Chef Toque
        int hx = headX - 6;
        int hy = headY - 8;
        _canvas.fillRoundRect(hx + 1, hy + 4, 12, 5, 1, 0xFFFF);
        _canvas.fillCircle(hx + 4, hy + 2, 4, 0xFFFF);
        _canvas.fillCircle(hx + 9, hy + 2, 4, 0xFFFF);
        _canvas.drawFastHLine(hx + 1, hy + 7, 12, 0x9CD3);
    } else if (acc == ACC_THUNDER_AURA) {
        // Crackling Super-Saiyan Golden Lightning Aura around pet!
        for (int i = 0; i < 4; ++i) {
            float ang = (nowMillis * 0.008f) + i * 1.57f;
            int lx = centerX + (int)(cosf(ang) * 20.0f);
            int ly = centerY + (int)(sinf(ang) * 18.0f);
            _canvas.drawLine(lx, ly - 5, lx - 3, ly, 0xFFE0);
            _canvas.drawLine(lx - 3, ly, lx + 2, ly + 1, 0xFFFF);
            _canvas.drawLine(lx + 2, ly + 1, lx - 1, ly + 6, 0x07FF);
        }
    }
}

// ==========================================
// Pet Drawing: Programmatic 32x32 pushRotateZoom()
// ==========================================

void DisplayManager::drawPet(const PetStateManager& petMgr, uint32_t nowMillis) {
    const PetState& state = petMgr.getState();
    uint32_t elapsed = nowMillis - _animStartTime;

    uint8_t stage = state.stage;
    uint8_t frameIdx = 0;

    float baseScale = (stage == STAGE_EGG) ? 1.35f : (1.25f + (stage - 1) * 0.15f);
    float zoomX = baseScale;
    float zoomY = baseScale;
    float angle = 0.0f;
    float groundY = 186.0f;

    if (stage == STAGE_EGG) {
        float eggPhase = (float)nowMillis * 0.005f;
        float eggWobble = sinf(eggPhase);
        frameIdx = (fabsf(eggWobble) > 0.65f || state.totalInteractions >= 2) ? 1 : 0;
        if (fabsf(eggWobble) > 0.65f) {
            angle = eggWobble * 15.0f;
            zoomY = baseScale * (1.0f + 0.08f * fabsf(eggWobble));
            zoomX = baseScale * (1.0f - 0.08f * fabsf(eggWobble));
        }
    } else {
        switch (_animState) {
            case ANIM_ROARING: {
                frameIdx = min((uint8_t)2, (uint8_t)(getStageFrameCount(stage) - 1));
                float roarElapsed = (float)(nowMillis - _animStartTime);
                float roarPhase = roarElapsed * 0.035f;
                float tremor = sinf(roarPhase * 3.5f) * 5.0f;
                float pulse = sinf(roarPhase);
                zoomY = baseScale * (1.18f + 0.12f * pulse);
                zoomX = baseScale * (1.25f + 0.10f * pulse);
                angle = (_facingRight ? 10.0f : -10.0f) + tremor;
                break;
            }
            case ANIM_JUMPING: {
                switch (_jumpPhase) {
                    case JUMP_PHASE_CROUCH_PRE:
                        frameIdx = 0;
                        zoomY = baseScale * 0.68f;
                        zoomX = baseScale * 1.32f;
                        angle = 0.0f;
                        break;
                    case JUMP_PHASE_ASCENT:
                        frameIdx = min((uint8_t)1, (uint8_t)(getStageFrameCount(stage) - 1));
                        zoomY = baseScale * 1.35f;
                        zoomX = baseScale * 0.85f;
                        angle = _facingRight ? -12.0f : 12.0f;
                        break;
                    case JUMP_PHASE_APEX_DESCENT:
                        frameIdx = 0;
                        zoomY = baseScale * 1.05f;
                        zoomX = baseScale * 0.95f;
                        angle = _facingRight ? 6.0f : -6.0f;
                        break;
                    case JUMP_PHASE_CROUCH_LAND:
                        frameIdx = 0;
                        zoomY = baseScale * 0.65f;
                        zoomX = baseScale * 1.35f;
                        angle = 0.0f;
                        break;
                    default:
                        break;
                }
                groundY += _petY;
                break;
            }
            case ANIM_FALLING:
            case ANIM_SLEEP: {
                frameIdx = min((uint8_t)3, (uint8_t)(getStageFrameCount(stage) - 1));
                float sleepPhase = (float)nowMillis * 0.0025f;
                float sleepSine = sinf(sleepPhase);
                zoomY = baseScale * (0.92f + 0.05f * sleepSine);
                zoomX = baseScale * (1.08f - 0.04f * sleepSine);
                angle = (_animState == ANIM_FALLING) ? 180.0f : 0.0f;
                break;
            }
            case ANIM_EATING: {
                frameIdx = min((uint8_t)1, (uint8_t)(getStageFrameCount(stage) - 1));
                float eatPhase = (float)elapsed * 0.025f;
                float chomp = sinf(eatPhase);
                zoomY = baseScale * (1.0f + 0.12f * fabsf(chomp));
                zoomX = baseScale * (1.0f - 0.08f * fabsf(chomp));
                angle = (_facingRight ? 4.0f : -4.0f) * chomp;
                break;
            }
            case ANIM_SECRET_STARS: {
                frameIdx = min((uint8_t)1, (uint8_t)(getStageFrameCount(stage) - 1));
                float hopPhase = (float)elapsed * 0.015f;
                float hop = fabsf(sinf(hopPhase));
                zoomY = baseScale * (0.90f + 0.25f * hop);
                zoomX = baseScale * (1.10f - 0.15f * hop);
                angle = sinf(hopPhase) * 12.0f;
                groundY -= hop * 10.0f;
                break;
            }
            case ANIM_IDLE:
            default: {
                bool isWalking = (fabsf(_petVx) > 0.05f) && !_isTurning;
                if (isWalking) {
                    float walkPhase = (float)nowMillis * 0.012f;
                    float stepSine = sinf(walkPhase);
                    float stepCos = cosf(walkPhase);
                    angle = 7.5f * stepSine;
                    float squash = fabsf(stepSine);
                    zoomY = baseScale * (0.92f + 0.15f * squash);
                    zoomX = baseScale * (1.08f - 0.10f * squash);
                    groundY -= 3.5f * fabsf(stepCos);
                    frameIdx = (stepSine > 0.0f) ? 1 : 0;
                    if (frameIdx >= getStageFrameCount(stage)) frameIdx = 0;
                } else {
                    frameIdx = 0;
                    float breathPhase = (float)nowMillis * 0.0035f;
                    float breathSine = sinf(breathPhase);
                    zoomY = baseScale * (1.0f + 0.06f * breathSine);
                    zoomX = baseScale * (1.0f - 0.04f * breathSine);
                    angle = 1.5f * sinf(breathPhase * 0.5f);
                }
                break;
            }
        }
    }

    const uint16_t* frameData = getStageFrame(stage, frameIdx);
    bool flipHorizontal = (stage == STAGE_EGG) ? false : !_facingRight;

    float finalZoomX = zoomX;
    if (_isTurning && stage != STAGE_EGG) {
        uint32_t turnElapsed = nowMillis - _turnStartTime;
        if (turnElapsed < 120) {
            float t = (float)turnElapsed / 120.0f;
            float turnFactor = fabsf(cosf(t * 3.14159f));
            finalZoomX = zoomX * max(0.15f, turnFactor);
            if (t >= 0.5f) {
                flipHorizontal = !_turnTargetFacingRight;
            } else {
                flipHorizontal = !_facingRight;
            }
        } else {
            _isTurning = false;
            _facingRight = _turnTargetFacingRight;
            flipHorizontal = !_facingRight;
            finalZoomX = zoomX;
        }
    }

    // Apply horizontal flip + optional Generation 2+ Shiny tint
    uint16_t processedFrame[SPRITE_WIDTH * SPRITE_HEIGHT];
    uint8_t shiny = (stage == STAGE_EGG) ? 0 : state.shinyVariant;
    for (int y = 0; y < SPRITE_HEIGHT; ++y) {
        int rowOffset = y * SPRITE_WIDTH;
        for (int x = 0; x < SPRITE_WIDTH; ++x) {
            int srcX = flipHorizontal ? (SPRITE_WIDTH - 1 - x) : x;
            uint16_t rawCol = pgm_read_word(&frameData[rowOffset + srcX]);
            processedFrame[rowOffset + x] = applyShinyTint(rawCol, shiny);
        }
    }
    _spriteCanvas.pushImage(0, 0, SPRITE_WIDTH, SPRITE_HEIGHT, processedFrame);

    _spriteCanvas.pushRotateZoom(&_canvas, _petX, groundY, angle, finalZoomX, zoomY, SPRITE_CHROMA_KEY);

    float headOffsetH = 26.0f * zoomY;
    int headX = (int)_petX + (_facingRight ? (int)(4 * finalZoomX) : -(int)(4 * finalZoomX));
    int headY = (int)(groundY - headOffsetH);

    // Upgrade 7: Draw equipped relic/accessory on pet
    if (stage != STAGE_EGG) {
        drawAccessory(state.equippedAccessory, headX, headY, (int)_petX, (int)(groundY - 14.0f * zoomY), nowMillis);
    }

    drawThoughtBubble((int)_petX, (int)(groundY - 30.0f * zoomY), nowMillis);

    switch (_animState) {
        case ANIM_EATING:
            drawEatingEffect(elapsed);
            if (elapsed > 1600) setAnimState(ANIM_IDLE);
            break;
        case ANIM_ROARING:
            drawRoarEffect(elapsed, state);
            if (elapsed > 1200) setAnimState(ANIM_IDLE);
            break;
        case ANIM_SLEEP:
            drawZzzEffect(elapsed);
            break;
        case ANIM_SECRET_STARS:
            drawStarsEffect(elapsed);
            if (elapsed > 2000) setAnimState(ANIM_IDLE);
            break;
        default:
            break;
    }
}

void DisplayManager::drawThoughtBubble(int petHeadX, int petHeadY, uint32_t nowMillis) {
    if (_animState != ANIM_IDLE || _activeThoughtType == 0) return;
    uint32_t elapsed = nowMillis - _thoughtStartTime;
    if (elapsed > 2800) {
        _activeThoughtType = 0;
        return;
    }

    int bx = constrain(petHeadX - 10, 10, SCREEN_WIDTH - 30);
    int by = max(16, petHeadY - 26);

    _canvas.fillCircle(petHeadX - 2, petHeadY - 5, 2, 0xFFFF);
    _canvas.fillCircle(petHeadX + 2, petHeadY - 12, 3, 0xFFFF);

    _canvas.fillRoundRect(bx, by, 26, 18, 5, 0xFFFF);
    _canvas.drawRoundRect(bx, by, 26, 18, 5, 0x18C3);

    if (_activeThoughtType == 1) {
        _canvas.fillCircle(bx + 8, by + 8, 4, 0xF9C6);
        _canvas.drawLine(bx + 11, by + 8, bx + 18, by + 12, 0x18C3);
    } else if (_activeThoughtType == 2) {
        _canvas.fillCircle(bx + 9, by + 11, 2, 0x07FF);
        _canvas.fillCircle(bx + 16, by + 9, 2, 0x07FF);
        _canvas.drawFastVLine(bx + 11, by + 4, 8, 0x07FF);
        _canvas.drawFastVLine(bx + 18, by + 2, 8, 0x07FF);
        _canvas.drawFastHLine(bx + 11, by + 3, 8, 0x07FF);
    } else if (_activeThoughtType == 3) {
        _canvas.setTextColor(0x07FF, 0xFFFF);
        _canvas.setCursor(bx + 8, by + 5);
        _canvas.print("Zz");
    } else if (_activeThoughtType == 4) {
        drawBitmapScaled(bx + 5, by + 2, sprite_heart_full, 16, 16, 1, 0xF9C6);
    } else {
        _canvas.drawLine(bx + 14, by + 3, bx + 9, by + 9, 0xFDE0);
        _canvas.drawLine(bx + 9, by + 9, bx + 16, by + 9, 0xFDE0);
        _canvas.drawLine(bx + 16, by + 9, bx + 11, by + 15, 0xFDE0);
    }
}

// ==========================================
// Feature 3 & Upgrade 6: Dual Training Mini-Games
// ==========================================

void DisplayManager::drawMiniGame(uint32_t nowMillis, float tiltBiasX, PetStateManager& petMgr) {
    if (!_miniGameActive) return;

    if (_miniGameMode == MINIGAME_TILT_LAVA_DODGE) {
        // === Upgrade 6: Tilt Lava Dodge Arcade Game ===
        _canvas.fillRoundRect(6, 16, SCREEN_WIDTH - 12, 20, 5, 0x18C3);
        _canvas.drawRoundRect(6, 16, SCREEN_WIDTH - 12, 20, 5, 0x07FF);
        _canvas.setTextColor(0x07FF, 0x18C3);
        _canvas.setCursor(12, 22);
        _canvas.printf("TILT DODGE %d/3 [Pts:%d]", _miniGameRound, _miniGameScore);

        // Steer pet directly with accelerometer tilt (or A to hop)
        _petX = constrain(_petX + tiltBiasX * 2.8f, 18.0f, (float)(SCREEN_WIDTH - 18));

        // Falling Lava Boulder
        _meteorY += _meteorVy;
        int rx = (int)_meteorX;
        int ry = (int)_meteorY;
        _canvas.fillCircle(rx, ry, 8, 0x8A22);
        _canvas.fillCircle(rx - 2, ry - 2, 4, 0xF800);
        _canvas.drawCircle(rx, ry, 8, 0xFA20);

        // Falling Bonus Roast Meat
        _bonusMeatY += 2.2f;
        int mx = (int)_bonusMeatX;
        int my = (int)_bonusMeatY;
        if (my > 0 && my < 186) {
            drawBitmapScaled(mx - 8, my - 8, sprite_meat, 16, 16, 1, 0xF9C6);
        }

        // Check Meat Catch
        if (fabsf(_bonusMeatY - 165.0f) < 16.0f && fabsf(_bonusMeatX - _petX) < 20.0f) {
            _miniGameScore++;
            _bonusMeatY = -40.0f;
            _bonusMeatX = 20.0f + (rand() % 95);
            triggerEat(false);
        } else if (_bonusMeatY > 188.0f) {
            _bonusMeatY = -30.0f;
            _bonusMeatX = 20.0f + (rand() % 95);
        }

        // Check Lava Boulder Ground / Dodge
        if (_meteorY >= 165.0f) {
            bool hit = (fabsf(_meteorX - _petX) < 16.0f) && !_isJumping;
            if (!hit) {
                _miniGameScore++;
            }
            _meteorY = -15.0f;
            _meteorX = 20.0f + (rand() % 95);
            _miniGameRound++;
            if (_miniGameRound > 3) {
                _miniGameActive = false;
                petMgr.train();
                petMgr.rewardDiscipline(_miniGameScore * 3);
                triggerSecretStars();
                showToast("DODGE COMPLETE!");
            }
        }
        return;
    }

    // === Classic Mode: Meteor Strike Training ===
    _canvas.fillRoundRect(8, 16, SCREEN_WIDTH - 16, 20, 5, 0x18C3);
    _canvas.drawRoundRect(8, 16, SCREEN_WIDTH - 16, 20, 5, 0xFDE0);
    _canvas.setTextColor(0xFDE0, 0x18C3);
    _canvas.setCursor(14, 22);
    _canvas.printf("STRIKE! R%d/3 [Score:%d]", _miniGameRound, _miniGameScore);

    _canvas.drawFastHLine(10, 160, SCREEN_WIDTH - 20, 0xF81F);
    _canvas.drawFastHLine(10, 161, SCREEN_WIDTH - 20, 0xFA20);

    if (!_miniGameStruck) {
        _meteorY += _meteorVy;

        int mx = (int)_meteorX;
        int my = (int)_meteorY;
        _canvas.fillCircle(mx, my, 7, 0xFDE0);
        _canvas.fillCircle(mx, my, 4, 0xFFFF);
        _canvas.fillCircle(mx + 2, my - 6, 5, 0xFA20);
        _canvas.fillCircle(mx + 4, my - 12, 4, 0xD082);
        _canvas.fillCircle(mx + 5, my - 18, 2, 0x51E7);

        if (_meteorY > 186.0f) {
            _meteorY = -15.0f;
            _meteorX = 25.0f + (rand() % 85);
            _miniGameRound++;
            if (_miniGameRound > 3) {
                _miniGameActive = false;
                petMgr.train();
                petMgr.rewardDiscipline(_miniGameScore * 3);
                triggerSecretStars();
            }
        }
    } else {
        uint32_t strikeElapsed = nowMillis - _miniGameStrikeTime;
        int mx = (int)_meteorX;
        int my = (int)_meteorY;

        _canvas.fillCircle(mx, my, max(1, 12 - (int)(strikeElapsed / 30)), 0xFFFF);
        for (int p = 0; p < 8; ++p) {
            float ang = p * 0.785f;
            int px = mx + (int)(cosf(ang) * (strikeElapsed * 0.15f));
            int py = my + (int)(sinf(ang) * (strikeElapsed * 0.15f));
            _canvas.fillCircle(px, py, 2, 0xFDE0);
        }

        _canvas.setTextColor(0xFFFF, 0xFA20);
        _canvas.setCursor(mx - 24, my - 18);
        _canvas.print("PERFECT!");

        if (strikeElapsed > 600) {
            _miniGameStruck = false;
            _meteorY = -15.0f;
            _meteorX = 25.0f + (rand() % 85);
            _miniGameRound++;
            if (_miniGameRound > 3) {
                _miniGameActive = false;
                petMgr.train();
                petMgr.rewardDiscipline(_miniGameScore * 4);
                triggerSecretStars();
            }
        }
    }
}

// ==========================================
// Upgrade 3: Wireless ESP-NOW / Rival Dino Battle Arena
// ==========================================

void DisplayManager::drawBattle(uint32_t nowMillis, PetStateManager& petMgr) {
    if (!_battleActive) return;

    // Check if an ESP-NOW peer sent incoming damage
    if (s_espNowPeerSeen) {
        _peerDiscovered = true;
        _rivalStage = s_espNowPeerStage;
    }
    if (s_espNowIncomingDmg > 0) {
        _playerHp = max(0, (int)_playerHp - (int)s_espNowIncomingDmg);
        s_espNowIncomingDmg = 0;
    }

    // Periodic rival attack every 1.4s
    if (nowMillis - _lastRivalAttackTime >= 1400) {
        _lastRivalAttackTime = nowMillis;
        int rDmg = 8 + (rand() % 9);
        _playerHp = max(0, (int)_playerHp - rDmg);
    }

    // Top Battle HUD Banner
    _canvas.fillRoundRect(4, 16, SCREEN_WIDTH - 8, 36, 5, 0x18C3);
    _canvas.drawRoundRect(4, 16, SCREEN_WIDTH - 8, 36, 5, 0xFA20);
    _canvas.setTextColor(0xFFE0, 0x18C3);
    _canvas.setCursor(10, 20);
    _canvas.print(_peerDiscovered ? "ESP-NOW PVP CLASH!" : "RIVAL DINO RAID!");

    // HP Bars (You vs Rival)
    _canvas.setTextColor(0x07E0, 0x18C3);
    _canvas.setCursor(10, 32);
    _canvas.print("YOU");
    _canvas.drawRect(32, 33, 34, 6, 0xFFFF);
    _canvas.fillRect(33, 34, (32 * constrain(_playerHp, 0, 100)) / 100, 4, 0x07E0);

    _canvas.setTextColor(0xF800, 0x18C3);
    _canvas.setCursor(70, 32);
    _canvas.print("FOE");
    _canvas.drawRect(92, 33, 34, 6, 0xFFFF);
    _canvas.fillRect(93, 34, (32 * constrain(_rivalHp, 0, 100)) / 100, 4, 0xF800);

    // Hint bar
    _canvas.setTextColor(0x07FF, 0x18C3);
    _canvas.setCursor(12, 42);
    _canvas.print("Mash A or Shake to Blast!");

    // Position player on left, draw Rival Dino on right
    _petX = 34.0f;
    _facingRight = true;

    const uint16_t* rivalFrame = getStageFrame(_rivalStage, (nowMillis / 180) % 2);
    uint16_t rivalFlipped[SPRITE_WIDTH * SPRITE_HEIGHT];
    for (int y = 0; y < SPRITE_HEIGHT; ++y) {
        for (int x = 0; x < SPRITE_WIDTH; ++x) {
            uint16_t c = pgm_read_word(&rivalFrame[y * SPRITE_WIDTH + (SPRITE_WIDTH - 1 - x)]);
            rivalFlipped[y * SPRITE_WIDTH + x] = applyShinyTint(c, 2); // Shadow tint for rival
        }
    }
    _spriteCanvas.pushImage(0, 0, SPRITE_WIDTH, SPRITE_HEIGHT, rivalFlipped);
    _spriteCanvas.pushRotateZoom(&_canvas, 102.0f, 186.0f, 0.0f, 1.45f, 1.45f, SPRITE_CHROMA_KEY);

    // Energy clash fireball in center!
    int clashX = 67 + (int)(sinf(nowMillis * 0.02f) * 14.0f);
    _canvas.fillCircle(clashX, 152, 6, 0xFFE0);
    _canvas.drawCircle(clashX, 152, 9, 0xF800);

    // Check Win / Loss
    if (_rivalHp <= 0 || _playerHp <= 0) {
        bool won = (_rivalHp <= 0);
        _battleActive = false;
        stopEspNowBattleRadio();
        petMgr.recordBattleResult(won);
        if (won) {
            triggerSecretStars();
            showToast("VICTORY! +15 EXP");
        } else {
            showToast("DEFEAT! TRAIN MORE");
        }
    }
}

// ==========================================
// Feature 4: Geyser Bath Waterfall Cleaning
// ==========================================

void DisplayManager::drawCleanGeyser(uint32_t nowMillis, PetStateManager& petMgr) {
    if (!_isCleaningGeyser) return;
    uint32_t elapsed = nowMillis - _cleanStartTime;

    if (elapsed < 1600) {
        for (int x = 8; x < SCREEN_WIDTH - 8; x += 10) {
            int waveY = (int)((elapsed * 0.35f) + (sinf(x * 0.2f) * 12.0f));
            _canvas.drawFastVLine(x, 0, min(186, waveY), 0x07FF);
            _canvas.drawFastVLine(x + 2, 0, min(186, waveY), 0xFFFF);
            _canvas.drawFastVLine(x + 4, 0, min(186, waveY), 0x05BF);
        }

        for (int b = 0; b < 10; ++b) {
            int bx = 20 + (b * 10) + (int)(sinf(elapsed * 0.01f + b) * 8);
            int by = 180 - (int)((elapsed * 0.12f) + b * 15) % 90;
            _canvas.drawCircle(bx, by, 3, 0xFFFF);
            _canvas.drawPixel(bx - 1, by - 1, 0x07FF);
        }

        _canvas.setTextColor(0xFFFF, 0x07FF);
        _canvas.setCursor(22, 50);
        _canvas.print("GEYSER BATH!");
    } else {
        _isCleaningGeyser = false;
        petMgr.getStateMutable().poops = 0;
        triggerSecretStars();
    }
}

// ==========================================
// Feature 5: Evolution Metamorphosis Sequence
// ==========================================

void DisplayManager::drawEvolutionSequence(uint32_t nowMillis) {
    if (!_isEvolvingSeq) return;
    uint32_t elapsed = nowMillis - _evoStartTime;

    if (elapsed < 2000) {
        int cx = (int)_petX;
        int cy = 145;
        int ringR = (int)((elapsed / 25) % 65);

        _canvas.drawCircle(cx, cy, ringR, 0xFDE0);
        _canvas.drawCircle(cx, cy, max(1, ringR - 4), 0xFFFF);
        _canvas.drawCircle(cx, cy, max(1, ringR - 8), 0xFA20);

        if ((elapsed / 80) % 2 == 0) {
            _canvas.drawRect(2, 2, SCREEN_WIDTH - 4, SCREEN_HEIGHT - 4, 0xFFFF);
        }

        _canvas.fillRoundRect(12, 35, SCREEN_WIDTH - 24, 34, 6, 0x18C3);
        _canvas.drawRoundRect(12, 35, SCREEN_WIDTH - 24, 34, 6, 0xFDE0);
        _canvas.setTextColor(0xFDE0, 0x18C3);
        _canvas.setCursor(22, 42);
        _canvas.print("EVOLUTION BURST!");
        _canvas.setTextColor(0xFFFF, 0x18C3);
        _canvas.setCursor(20, 54);
        _canvas.printf("Morphing to Lv.%d...", _evoTargetStage);
    } else {
        _isEvolvingSeq = false;
        setAnimState(ANIM_SECRET_STARS);
    }
}

// ==========================================
// Action Overlays (Roar, Eat, Poops)
// ==========================================

void DisplayManager::drawPoops(uint8_t count) {
    if (count == 0) return;
    for (int i = 0; i < min(3, (int)count); ++i) {
        int px = 18 + i * 36;
        int py = 176;
        _canvas.fillRoundRect(px, py + 4, 14, 6, 2, 0x8A40);
        _canvas.fillRoundRect(px + 2, py + 1, 10, 5, 2, 0x9260);
        _canvas.fillCircle(px + 7, py + 1, 3, 0xA280);
        _canvas.drawPixel(px + 5, py + 3, 0xFFFF);
        _canvas.drawPixel(px + 9, py + 3, 0xFFFF);
        if ((millis() / 300 + i) % 2 == 0) {
            _canvas.drawLine(px + 4, py - 4, px + 2, py - 8, 0x39E8);
            _canvas.drawLine(px + 10, py - 4, px + 12, py - 8, 0x39E8);
        }
    }
}

void DisplayManager::drawEatingEffect(uint32_t elapsed) {
    int ex = (int)_petX + (_facingRight ? 4 : -8);
    int ey = 186 - 22;
    if ((elapsed / 150) % 2 == 0) {
        _canvas.fillCircle(ex, ey, 2, 0xF9C6);
        _canvas.fillCircle(ex + 4, ey - 3, 1, 0xFDE0);
    }
}

void DisplayManager::drawRoarEffect(uint32_t elapsed, const PetState& state) {
    int stageIdx = constrain(state.stage - 1, 0, 6);
    float baseScale = 1.25f + stageIdx * 0.15f;
    int mouthDx = (int)(14 * baseScale);
    int mouthDy = (int)(18 * baseScale);
    int drawY = 186 - (int)(30 * baseScale);

    int mx = (int)_petX + (_facingRight ? mouthDx : -mouthDx);
    int my = drawY + mouthDy;

    int waveDist = (int)(elapsed * 0.16f) % 45;
    int wx = mx + (_facingRight ? waveDist : -waveDist);

    _canvas.drawCircle(wx, my, 6 + (waveDist / 4), 0xFFFF);
    _canvas.drawCircle(wx, my, 8 + (waveDist / 3), 0xFDE0);
    _canvas.drawCircle(wx, my, 12 + (waveDist / 2), 0xFA20);
}

void DisplayManager::drawZzzEffect(uint32_t elapsed) {
    (void)elapsed;
}

void DisplayManager::drawStarsEffect(uint32_t elapsed) {
    int cx = (int)_petX;
    int cy = 186 - 36;
    for (int i = 0; i < 4; ++i) {
        float ang = (i * 1.57f) + (elapsed * 0.006f);
        int sx = cx + (int)(cosf(ang) * 22.0f);
        int sy = cy + (int)(sinf(ang) * 14.0f);
        _canvas.fillCircle(sx, sy, 2, 0xFFFF);
        _canvas.drawPixel(sx, sy, 0xFDE0);
    }
}

// ==========================================
// Main Update & Frame Orchestrator
// ==========================================

void DisplayManager::update(PetStateManager& petMgr, float tiltBiasX, uint32_t nowMillis) {
    if (nowMillis - _lastRenderTime < 33) {
        return;
    }
    _lastRenderTime = nowMillis;

    const PetState& state = petMgr.getState();

    if (nowMillis - _lastWalkTime >= 130) {
        _lastWalkTime = nowMillis;
        _walkFrameIndex = (_walkFrameIndex + 1) % 4;
    }

    if (nowMillis - _lastFrameTime >= 120) {
        _lastFrameTime = nowMillis;
        _frameIndex = (_frameIndex + 1) % 2;
    }

    // Sky cycle timer: every 3 minutes
    if (nowMillis - _lastSkyCycleTime >= 180000) {
        _lastSkyCycleTime = nowMillis;
        toggleSkyTime();
    }

    // Upgrade 5: Weather cycle timer: every 2 minutes
    if (nowMillis - _lastWeatherChangeTime >= 120000) {
        _lastWeatherChangeTime = nowMillis;
        _currentWeather = (WeatherType)((_currentWeather + 1) % WEATHER_COUNT);
    }

    if (_animState == ANIM_IDLE && _activeThoughtType == 0 && (nowMillis - _lastThoughtTime >= 8000)) {
        _lastThoughtTime = nowMillis;
        _thoughtStartTime = nowMillis;
        if (state.hunger < 2) _activeThoughtType = 1;
        else if (state.discipline > 50) _activeThoughtType = 4;
        else _activeThoughtType = 2 + (rand() % 4);
    }

    float baseScale = (state.stage == STAGE_EGG) ? 1.25f : (1.25f + (state.stage - 1) * 0.15f);
    int halfW = (int)(16.0f * baseScale);
    int minX = halfW + 4;
    int maxX = SCREEN_WIDTH - halfW - 4;

    if (_animState == ANIM_IDLE && !state.isSleeping && state.stage != STAGE_EGG && !_isTurning && !_battleActive && !_miniGameActive) {
        float netVelocity = _petVx + tiltBiasX;
        _petX += netVelocity;

        if (_petX < minX && !_facingRight) {
            _petX = minX;
            _petVx = fabsf(_petVx);
            _isTurning = true;
            _turnStartTime = nowMillis;
            _turnTargetFacingRight = true;
        } else if (_petX > maxX && _facingRight) {
            _petX = maxX;
            _petVx = -fabsf(_petVx);
            _isTurning = true;
            _turnStartTime = nowMillis;
            _turnTargetFacingRight = false;
        } else {
            if (netVelocity > 0.15f && !_facingRight) {
                _isTurning = true;
                _turnStartTime = nowMillis;
                _turnTargetFacingRight = true;
            } else if (netVelocity < -0.15f && _facingRight) {
                _isTurning = true;
                _turnStartTime = nowMillis;
                _turnTargetFacingRight = false;
            }
        }
    } else if (state.stage == STAGE_EGG) {
        _petX = SCREEN_WIDTH / 2.0f;
    }

    if (_isJumping) {
        switch (_jumpPhase) {
            case JUMP_PHASE_CROUCH_PRE:
                _petY = 0.0f;
                if (nowMillis - _jumpPhaseStartTime >= 100) {
                    _jumpPhase = JUMP_PHASE_ASCENT;
                    _jumpVy = -6.8f;
                }
                break;

            case JUMP_PHASE_ASCENT:
                _petY += _jumpVy;
                _jumpVy += 0.42f;
                if (_jumpVy >= -0.5f) {
                    _jumpPhase = JUMP_PHASE_APEX_DESCENT;
                }
                break;

            case JUMP_PHASE_APEX_DESCENT:
                _petY += _jumpVy;
                _jumpVy += 0.42f;
                if (_petY >= 0.0f) {
                    _petY = 0.0f;
                    _jumpPhase = JUMP_PHASE_CROUCH_LAND;
                    _jumpPhaseStartTime = nowMillis;
                }
                break;

            case JUMP_PHASE_CROUCH_LAND:
                _petY = 0.0f;
                if (nowMillis - _jumpPhaseStartTime >= 110) {
                    _jumpPhase = JUMP_PHASE_NONE;
                    _isJumping = false;
                    setAnimState(ANIM_IDLE);
                }
                break;

            default:
                break;
        }
    }

    if (state.isSleeping && _animState != ANIM_SLEEP) {
        setAnimState(ANIM_SLEEP);
    } else if (!state.isSleeping && _animState == ANIM_SLEEP) {
        setAnimState(ANIM_IDLE);
    }

    drawBackground();
    drawGround();
    drawPoops(state.poops);
    drawPet(petMgr, nowMillis);

    if (_miniGameActive) {
        drawMiniGame(nowMillis, tiltBiasX, petMgr);
    } else if (_battleActive) {
        drawBattle(nowMillis, petMgr);
    } else if (_isCleaningGeyser) {
        drawCleanGeyser(nowMillis, petMgr);
    } else if (_isEvolvingSeq) {
        drawEvolutionSequence(nowMillis);
    }

    drawHud(petMgr);

    if (_showStats) {
        drawStats(petMgr);
    }

    if (_toastText[0] && (int32_t)(_toastUntil - nowMillis) > 0) {
        int tw = (int)strlen(_toastText) * 6 + 12;
        int tx = (SCREEN_WIDTH - tw) / 2;
        _canvas.fillRoundRect(tx, 168, tw, 18, 5, 0x0000);
        _canvas.drawRoundRect(tx, 168, tw, 18, 5, 0xFFE0);
        _canvas.setTextColor(0xFFFF, 0x0000);
        _canvas.setTextSize(1);
        _canvas.setCursor(tx + 6, 173);
        _canvas.print(_toastText);
    }

    pushFrameToDisplay(_canvas);
}

void DisplayManager::drawSpriteRGB565(int x, int y, const SpriteInfo& sprite, bool flipX) {
    if (!sprite.data || sprite.width <= 0 || sprite.height <= 0) return;

    if (!flipX) {
        _canvas.pushImage(x, y, sprite.width, sprite.height, sprite.data, SPRITE_CHROMA_KEY);
    } else {
        int w = sprite.width;
        int h = sprite.height;
        for (int row = 0; row < h; ++row) {
            for (int col = 0; col < w; ++col) {
                uint16_t color = sprite.data[row * w + col];
                if (color != SPRITE_CHROMA_KEY) {
                    _canvas.drawPixel(x + (w - 1 - col), y + row, color);
                }
            }
        }
    }
}

void DisplayManager::drawBitmapScaled(int x, int y, const uint8_t* bitmap, int w, int h, int scale, uint16_t color, bool flipX) {
    int bytesPerRow = (w + 7) / 8;
    for (int row = 0; row < h; ++row) {
        for (int col = 0; col < w; ++col) {
            int byteIndex = row * bytesPerRow + (col / 8);
            uint8_t byteVal = pgm_read_byte(&bitmap[byteIndex]);
            bool isPixelOn = (byteVal & (0x80 >> (col % 8))) != 0;

            if (isPixelOn) {
                int drawCol = flipX ? (w - 1 - col) : col;
                if (scale == 1) {
                    _canvas.drawPixel(x + drawCol, y + row, color);
                } else {
                    _canvas.fillRect(x + drawCol * scale, y + row * scale, scale, scale, color);
                }
            }
        }
    }
}

void DisplayManager::showUpdateScreen(const char* title, const char* status, int progress, int total) {
    _canvas.fillScreen(COLOR_BLACK);
    _canvas.setTextColor(0xFDE0, COLOR_BLACK);
    _canvas.setTextSize(1);

    int titleLen = (int)strlen(title);
    int titleX = max(4, (SCREEN_WIDTH - titleLen * 6) / 2);
    _canvas.setCursor(titleX, 70);
    _canvas.print(title);

    if (status && strlen(status) > 0) {
        _canvas.setTextColor(0x07FF, COLOR_BLACK);
        int statusLen = (int)strlen(status);
        int statusX = max(4, (SCREEN_WIDTH - statusLen * 6) / 2);
        _canvas.setCursor(statusX, 90);
        _canvas.print(status);
    }

    int pct = (total > 0) ? constrain((progress * 100) / total, 0, 100) : 0;
    _canvas.setTextColor(0xFFFF, COLOR_BLACK);
    _canvas.setCursor(55, 115);
    _canvas.printf("%d%%", pct);

    _canvas.drawRoundRect(14, 135, SCREEN_WIDTH - 28, 14, 4, 0x5248);
    int barW = ((SCREEN_WIDTH - 32) * pct) / 100;
    if (barW > 0) {
        _canvas.fillRoundRect(16, 137, barW, 10, 3, 0x07FF);
    }

    pushFrameToDisplay(_canvas);
}

void DisplayManager::showOtaProgress(unsigned int progress, unsigned int total) {
    showUpdateScreen("POWERING UP DIGI-CORE...", "UPDATING FIRMWARE", progress, total);
}
