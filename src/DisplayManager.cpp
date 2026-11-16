#include "DisplayManager.h"
#include <math.h>

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
      _lastThoughtTime(0),
      _thoughtStartTime(0),
      _activeThoughtType(0),
      _miniGameActive(false),
      _miniGameStartTime(0),
      _meteorX(67.0f),
      _meteorY(0.0f),
      _meteorVy(2.5f),
      _miniGameScore(0),
      _miniGameRound(0),
      _miniGameStruck(false),
      _miniGameStrikeTime(0),
      _isCleaningGeyser(false),
      _cleanStartTime(0),
      _isEvolvingSeq(false),
      _evoStartTime(0),
      _evoTargetStage(3) {}

// Pushes the finished 135x240 _canvas to M5.Display. _canvas itself is never
// rotated or scaled with negative values.
//  - M5StickS3 (real hardware): 135x240 portrait panel, drawn at (0,0).
//  - Wokwi CoreS3 (320x240): centered at x = (width - 135) / 2, y = 0.
static void pushFrameToDisplay(M5Canvas& canvas) {
#if defined(WOKWI_SIM)
    int x = (M5.Display.width() - SCREEN_WIDTH) / 2;
    if (x < 0) x = 0;
  #if WOKWI_SOFT_FLIP
    // Rotate the whole finished frame by 180 deg (angle only; zoom stays +1.0)
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
    // M5.begin(cfg) has already completed in main.cpp setup() before this runs,
    // so this is the one and only place the display rotation is set.
#if defined(WOKWI_SIM)
    M5.Display.setRotation(WOKWI_ROTATION);   // 0-3 only, never mirrored (4-7)
#else
    M5.Display.setRotation(SCREEN_ROTATION);  // 0 = upright portrait on M5StickS3
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

    // Small 32x32 sprite canvas
    _spriteCanvas.setColorDepth(16);
    _spriteCanvas.setPsram(false);
    _spriteCanvas.createSprite(SPRITE_WIDTH, SPRITE_HEIGHT);
    _spriteCanvas.setSwapBytes(true); // Crucial: ensure uint16_t RGB565 pixel arrays match native endianness
    _spriteCanvas.setPivot(SPRITE_WIDTH / 2, SPRITE_HEIGHT - 1); // Center-bottom ground pivot (16, 31)
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

void DisplayManager::startMiniGame() {
    _miniGameActive = true;
    _miniGameStartTime = millis();
    _miniGameRound = 1;
    _miniGameScore = 0;
    _meteorX = 25.0f + (rand() % 85);
    _meteorY = -15.0f;
    _meteorVy = 2.4f;
    _miniGameStruck = false;
}

void DisplayManager::handleMiniGameInput() {
    if (!_miniGameActive) return;
    uint32_t now = millis();

    // Strike zone: meteor between y=140 and y=182
    if (_meteorY >= 135.0f && _meteorY <= 186.0f && !_miniGameStruck) {
        _miniGameStruck = true;
        _miniGameStrikeTime = now;
        _miniGameScore++;
        triggerRoar(); // Pet strikes with mighty roar!
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
}

// ==========================================
// Full-Color Living Environment Rendering
// ==========================================

void DisplayManager::drawBackground() {
    uint32_t now = millis();

    // Feature 1: Day/Night Sky Gradients & Celestial Features
    switch (_currentSky) {
        case SKY_DAY: {
            // Bright Prehistoric Golden Day
            for (int y = 0; y < 186; ++y) {
                float t = (float)y / 185.0f;
                uint8_t r = (uint8_t)(30  + t * 210);
                uint8_t g = (uint8_t)(130 + t * 90);
                uint8_t b = (uint8_t)(230 - t * 140);
                uint16_t col = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
                _canvas.drawFastHLine(0, y, SCREEN_WIDTH, col);
            }
            // Sun glow
            _canvas.fillCircle(105, 38, 14, 0xFFE0);
            _canvas.fillCircle(105, 38, 18, 0xFD80);
            // Fluffy clouds
            _canvas.fillRoundRect(15, 45, 45, 10, 5, 0xFFFF);
            _canvas.fillRoundRect(25, 40, 25, 12, 6, 0xFFFF);
            break;
        }
        case SKY_NIGHT: {
            // Deep Obsidian / Indigo Midnight
            for (int y = 0; y < 186; ++y) {
                float t = (float)y / 185.0f;
                uint8_t r = (uint8_t)(10 + t * 40);
                uint8_t g = (uint8_t)(8  + t * 15);
                uint8_t b = (uint8_t)(35 + t * 25);
                uint16_t col = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
                _canvas.drawFastHLine(0, y, SCREEN_WIDTH, col);
            }
            // Glowing crescent moon
            _canvas.fillCircle(24, 30, 9, 0xFFFF);
            _canvas.fillCircle(27, 28, 8, 0x18C3); // shadow cutout for crescent

            // Animated twinkling diamond stars
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
            // Rose-Gold Sunrise
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
            // Iconic Prehistoric Volcanic Twilight-Sunset
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
    _canvas.fillTriangle(26, 114, 32, 108, 38, 114, 0xFA20); // Molten peak crest

    // Volcano silhouette (right peak)
    _canvas.fillTriangle(65, 186, 108, 122, 135, 186, 0x28C5);
    _canvas.fillTriangle(104, 126, 108, 122, 112, 126, 0xFCE0);

    // Drifting volcanic smoke puffs
    int smokeOffset = (now / 150) % 36;
    _canvas.fillCircle(32 - (smokeOffset / 3), 104 - smokeOffset, 4 + (smokeOffset / 6), 0x51E7);
    _canvas.fillCircle(108 - (smokeOffset / 4), 118 - smokeOffset, 3 + (smokeOffset / 7), 0x51E7);

    // Dynamic fire embers drifting upward
    for (int i = 0; i < 6; ++i) {
        int ex = (int)(20 + (i * 21) + sinf((now + i * 400) * 0.003f) * 12);
        int ey = 180 - ((int)((now / 35) + i * 30) % 170);
        _canvas.drawPixel(ex, ey, 0xFDE0); // Radiant ember gold
        _canvas.drawPixel(ex, ey + 1, 0xFA20); // Amber tail
    }
}

void DisplayManager::drawGround() {
    // Volcanic Bedrock Crust (y = 186 to 240)
    for (int y = 186; y < SCREEN_HEIGHT; ++y) {
        float gt = (float)(y - 186) / 53.0f;
        uint8_t r = (uint8_t)(45 - gt * 25);
        uint8_t g = (uint8_t)(38 - gt * 22);
        uint8_t b = (uint8_t)(35 - gt * 20);
        uint16_t col = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
        _canvas.drawFastHLine(0, y, SCREEN_WIDTH, col);
    }

    // Bedrock rim line
    _canvas.drawFastHLine(0, 186, SCREEN_WIDTH, 0x5248);

    // Glowing magma fissures on the ground
    _canvas.drawLine(15, 192, 38, 196, 0xFA20);
    _canvas.drawLine(38, 196, 62, 193, 0xFDE0);
    _canvas.drawLine(82, 195, 105, 198, 0xFA20);
    _canvas.drawLine(105, 198, 124, 194, 0xFDE0);

    // Textured rocky highlights
    _canvas.drawPixel(45, 189, 0x7BCF);
    _canvas.drawPixel(88, 190, 0x7BCF);
    _canvas.drawPixel(112, 188, 0x7BCF);
}

// ==========================================
// Bottom Dock HUD & Menu with Clear Descriptions
// ==========================================

void DisplayManager::drawHud(const PetStateManager& petMgr) {
    // Dock is located at the bottom of the screen: y = 212..239
    int dockY = 212;
    int dockH = 26;

    // Control Deck frosted pill background
    _canvas.fillRoundRect(4, dockY, SCREEN_WIDTH - 8, dockH, 6, 0x10A2);
    _canvas.drawRoundRect(4, dockY, SCREEN_WIDTH - 8, dockH, 6, 0x3186);

    // 4 Big, vibrant self-explanatory icons
    // 0: Feed (14), 1: Train (46), 2: Clean (78), 3: Stats (110)
    drawBitmapScaled(14,  dockY + 5, icon_feed,  16, 16, 1, 0xF9C6); // Coral Red Meat
    drawBitmapScaled(46,  dockY + 5, icon_train, 16, 16, 1, 0xFDE0); // Radiant Gold Target
    drawBitmapScaled(78,  dockY + 5, icon_clean, 16, 16, 1, 0x07FF); // Cyan Geyser/Bath
    drawBitmapScaled(110, dockY + 5, icon_stats, 16, 16, 1, 0xC81F); // Purple Heart/Meter

    // Highlight box around chosen icon: bright + pulsing when the menu is OPEN,
    // dim single outline when CLOSED (so the open/closed state is obvious).
    int iconX = 14 + ((int)_selectedMenu * 32) - 3;
    if (_menuOpen) {
        bool pulse = ((millis() / 250) % 2) == 0;
        _canvas.drawRoundRect(iconX, dockY + 2, 22, 22, 4, 0xFFFF);
        _canvas.drawRoundRect(iconX - 1, dockY + 1, 24, 24, 5, pulse ? 0xFD80 : 0xFFE0);
        _canvas.drawRoundRect(iconX - 2, dockY, 26, 26, 6, pulse ? 0xFFE0 : 0xFD80);
    } else {
        _canvas.drawRoundRect(iconX, dockY + 2, 22, 22, 4, 0x4208);
    }

    // Banner text: shows what the two buttons do RIGHT NOW
    const char* labelText = "B:MENU  A:POKE";
    uint16_t badgeColor = 0x7BEF;
    if (_menuOpen) {
        badgeColor = 0xFFFF;
        switch (_selectedMenu) {
            case MENU_FEED:  labelText = "B:NEXT  A:FEED";  badgeColor = 0xF9C6; break;
            case MENU_TRAIN: labelText = "B:NEXT  A:TRAIN"; badgeColor = 0xFDE0; break;
            case MENU_CLEAN: labelText = "B:NEXT  A:CLEAN"; badgeColor = 0x07FF; break;
            case MENU_STATS: labelText = "B:NEXT  A:STATS"; badgeColor = 0xC81F; break;
            default: break;
        }
    }

    // Action banner badge
    _canvas.fillRoundRect(4, 192, SCREEN_WIDTH - 8, 17, 4, 0x18C3);
    _canvas.drawRoundRect(4, 192, SCREEN_WIDTH - 8, 17, 4, badgeColor);
    _canvas.fillCircle(11, 200, 3, badgeColor);

    _canvas.setTextColor(0xFFFF, 0x18C3);
    _canvas.setTextSize(1);
    _canvas.setCursor(17, 197);
    _canvas.print(labelText);

    // Alert indicator on the top-right if pet needs urgent attention
    if (petMgr.needsAlert() && ((millis() / 400) % 2 == 0)) {
        drawBitmapScaled(SCREEN_WIDTH - 22, 8, icon_alert, 16, 16, 1, COLOR_ACCENT_RED);
    }
}

void DisplayManager::drawMenu() {
    // Menu highlight is rendered directly inside drawHud
}

void DisplayManager::drawStats(const PetStateManager& petMgr) {
    const PetState& state = petMgr.getState();

    // Sleek frosted modal card centered on the screen (y = 25 to 185)
    _canvas.fillRoundRect(8, 25, SCREEN_WIDTH - 16, 160, 8, 0x18C3);
    _canvas.drawRoundRect(8, 25, SCREEN_WIDTH - 16, 160, 8, 0xFDE0);
    _canvas.drawFastHLine(12, 46, SCREEN_WIDTH - 24, 0x39E8);

    _canvas.setTextColor(0xFDE0, 0x18C3);
    _canvas.setTextSize(1);
    _canvas.setCursor(18, 32);
    _canvas.printf("LV.%d %s", state.stage, petMgr.getSpeciesName());

    // Age & Weight
    _canvas.setTextColor(0xFFFF, 0x18C3);
    _canvas.setCursor(16, 54);
    _canvas.printf("Age: %d hrs", state.ageHours);
    _canvas.setCursor(16, 68);
    _canvas.printf("Weight: %d kg", 10 + state.stage * 12);

    // Hunger Hearts
    _canvas.setCursor(16, 84);
    _canvas.print("Hunger: ");
    for (int i = 0; i < 4; ++i) {
        const uint8_t* heartIcon = (i < state.hunger) ? sprite_heart_full : sprite_heart_empty;
        drawBitmapScaled(62 + i * 16, 81, heartIcon, 16, 16, 1, 0xF9C6);
    }

    // Strength Hearts
    _canvas.setCursor(16, 102);
    _canvas.print("Strength: ");
    for (int i = 0; i < 4; ++i) {
        const uint8_t* heartIcon = (i < state.strength) ? sprite_heart_full : sprite_heart_empty;
        drawBitmapScaled(62 + i * 16, 99, heartIcon, 16, 16, 1, 0xFDE0);
    }

    // Discipline Progress Bar
    _canvas.setCursor(16, 120);
    _canvas.printf("Disc: %d%%", state.discipline);
    _canvas.drawRect(16, 132, SCREEN_WIDTH - 32, 8, 0x39E8);
    int barW = ((SCREEN_WIDTH - 36) * constrain(state.discipline, 0, 100)) / 100;
    if (barW > 0) {
        _canvas.fillRect(18, 134, barW, 4, 0x07FF);
    }

    // Dismiss & Reset Hint
    _canvas.setTextColor(0x9CD3, 0x18C3);
    _canvas.setCursor(14, 152);
    _canvas.print("A/B:Exit  Hold B:Egg");
}

// ==========================================
// Pet Drawing: Programmatic 32x32 pushRotateZoom()
// ==========================================

void DisplayManager::drawPet(const PetStateManager& petMgr, uint32_t nowMillis) {
    const PetState& state = petMgr.getState();
    uint32_t elapsed = nowMillis - _animStartTime;

    uint8_t stage = state.stage;
    uint8_t frameIdx = 0;

    // Base scale calculation: ~1.25x for stage 1 up to ~2.15x for stage 7
    float baseScale = (stage == STAGE_EGG) ? 1.25f : (1.25f + (stage - 1) * 0.15f);
    float zoomX = baseScale;
    float zoomY = baseScale;
    float angle = 0.0f;
    float groundY = 186.0f;

    if (stage == STAGE_EGG) {
        // Egg wobbling sine-wave animation
        float eggPhase = (float)nowMillis * 0.005f;
        float eggWobble = sinf(eggPhase);
        frameIdx = (fabsf(eggWobble) > 0.65f) ? 1 : 0;
        if (fabsf(eggWobble) > 0.65f) {
            angle = eggWobble * 15.0f;
            zoomY = baseScale * (1.0f + 0.08f * fabsf(eggWobble));
            zoomX = baseScale * (1.0f - 0.08f * fabsf(eggWobble));
        }
    } else {
        // Active pet animation states driven by programmatic modifiers
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
                        // Anticipation squash before leaping
                        frameIdx = 0;
                        zoomY = baseScale * 0.68f;
                        zoomX = baseScale * 1.32f;
                        angle = 0.0f;
                        break;
                    case JUMP_PHASE_ASCENT:
                        // Powerful upward stretch and forward tilt
                        frameIdx = min((uint8_t)1, (uint8_t)(getStageFrameCount(stage) - 1));
                        zoomY = baseScale * 1.35f;
                        zoomX = baseScale * 0.85f;
                        angle = _facingRight ? -12.0f : 12.0f;
                        break;
                    case JUMP_PHASE_APEX_DESCENT:
                        // Apex hang / tuck
                        frameIdx = 0;
                        zoomY = baseScale * 1.05f;
                        zoomX = baseScale * 0.95f;
                        angle = _facingRight ? 6.0f : -6.0f;
                        break;
                    case JUMP_PHASE_CROUCH_LAND:
                        // Impact compression squash on landing
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
                    // Programmatic waddling cycle
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
                    // Programmatic idle breathing cycle
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

    // Load current 32x32 frame into small sprite canvas
    const uint16_t* frameData = getStageFrame(stage, frameIdx);

    // Determine whether to flip horizontally in software
    // Pet naturally faces right in logical coordinate system; when facing left, flip horizontally
    bool flipHorizontal = !_facingRight;

    // Handle turning transition squash
    float finalZoomX = zoomX;
    if (_isTurning) {
        uint32_t turnElapsed = nowMillis - _turnStartTime;
        if (turnElapsed < 120) {
            float t = (float)turnElapsed / 120.0f;
            // Squash width towards 0 at midpoint, then expand in new direction
            float turnFactor = fabsf(cosf(t * 3.14159f));
            finalZoomX = zoomX * max(0.15f, turnFactor);
            // Flip facing direction halfway through turn
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

    // Software row-flip if facing left to keep zoomX strictly positive
    if (flipHorizontal) {
        uint16_t flippedFrame[SPRITE_WIDTH * SPRITE_HEIGHT];
        for (int y = 0; y < SPRITE_HEIGHT; ++y) {
            int rowOffset = y * SPRITE_WIDTH;
            for (int x = 0; x < SPRITE_WIDTH; ++x) {
                flippedFrame[rowOffset + x] = frameData[rowOffset + (SPRITE_WIDTH - 1 - x)];
            }
        }
        _spriteCanvas.pushImage(0, 0, SPRITE_WIDTH, SPRITE_HEIGHT, flippedFrame);
    } else {
        _spriteCanvas.pushImage(0, 0, SPRITE_WIDTH, SPRITE_HEIGHT, frameData);
    }

    // Strictly positive zoomX in pushRotateZoom() prevents coordinate inversion artifacts
    _spriteCanvas.pushRotateZoom(&_canvas, _petX, groundY, angle, finalZoomX, zoomY, SPRITE_CHROMA_KEY);

    // Feature 2: Thought & Emotion Bubbles over pet's head
    float headOffsetH = 30.0f * zoomY;
    drawThoughtBubble((int)_petX, (int)(groundY - headOffsetH), nowMillis);

    // Overlays
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

// ==========================================
// Feature 2: Thought & Emotion Bubbles
// ==========================================

void DisplayManager::drawThoughtBubble(int petHeadX, int petHeadY, uint32_t nowMillis) {
    if (_animState != ANIM_IDLE || _activeThoughtType == 0) return;
    uint32_t elapsed = nowMillis - _thoughtStartTime;
    if (elapsed > 2800) {
        _activeThoughtType = 0;
        return;
    }

    int bx = constrain(petHeadX - 10, 10, SCREEN_WIDTH - 30);
    int by = petHeadY - 26;

    // Small trailing cloud bubbles
    _canvas.fillCircle(petHeadX - 2, petHeadY - 5, 2, 0xFFFF);
    _canvas.fillCircle(petHeadX + 2, petHeadY - 12, 3, 0xFFFF);

    // Main cloud bubble
    _canvas.fillRoundRect(bx, by, 26, 18, 5, 0xFFFF);
    _canvas.drawRoundRect(bx, by, 26, 18, 5, 0x18C3);

    // Inner icon
    if (_activeThoughtType == 1) {
        // Meat drumstick
        _canvas.fillCircle(bx + 8, by + 8, 4, 0xF9C6);
        _canvas.drawLine(bx + 11, by + 8, bx + 18, by + 12, 0x18C3);
    } else if (_activeThoughtType == 2) {
        // Musical note
        _canvas.fillCircle(bx + 9, by + 11, 2, 0x07FF);
        _canvas.fillCircle(bx + 16, by + 9, 2, 0x07FF);
        _canvas.drawFastVLine(bx + 11, by + 4, 8, 0x07FF);
        _canvas.drawFastVLine(bx + 18, by + 2, 8, 0x07FF);
        _canvas.drawFastHLine(bx + 11, by + 3, 8, 0x07FF);
    } else if (_activeThoughtType == 3) {
        // Zzz
        _canvas.setTextColor(0x07FF, 0xFFFF);
        _canvas.setCursor(bx + 8, by + 5);
        _canvas.print("Zz");
    } else if (_activeThoughtType == 4) {
        // Heart
        drawBitmapScaled(bx + 5, by + 2, sprite_heart_full, 16, 16, 1, 0xF9C6);
    } else {
        // Lightning bolt
        _canvas.drawLine(bx + 14, by + 3, bx + 9, by + 9, 0xFDE0);
        _canvas.drawLine(bx + 9, by + 9, bx + 16, by + 9, 0xFDE0);
        _canvas.drawLine(bx + 16, by + 9, bx + 11, by + 15, 0xFDE0);
    }
}

// ==========================================
// Feature 3: Meteor Strike Training Mini-Game
// ==========================================

void DisplayManager::drawMiniGame(uint32_t nowMillis, PetStateManager& petMgr) {
    if (!_miniGameActive) return;

    // Mini-game HUD banner at top
    _canvas.fillRoundRect(10, 8, SCREEN_WIDTH - 20, 20, 5, 0x18C3);
    _canvas.drawRoundRect(10, 8, SCREEN_WIDTH - 20, 20, 5, 0xFDE0);
    _canvas.setTextColor(0xFDE0, 0x18C3);
    _canvas.setCursor(18, 14);
    _canvas.printf("STRIKE! Round %d/3 [Score: %d]", _miniGameRound, _miniGameScore);

    // Target strike line at y = 160
    _canvas.drawFastHLine(10, 160, SCREEN_WIDTH - 20, 0xF81F);
    _canvas.drawFastHLine(10, 161, SCREEN_WIDTH - 20, 0xFA20);

    // Meteor falling physics
    if (!_miniGameStruck) {
        _meteorY += _meteorVy;

        // Meteor glowing fireball
        int mx = (int)_meteorX;
        int my = (int)_meteorY;
        _canvas.fillCircle(mx, my, 7, 0xFDE0);
        _canvas.fillCircle(mx, my, 4, 0xFFFF);
        // Fire smoke tail
        _canvas.fillCircle(mx + 2, my - 6, 5, 0xFA20);
        _canvas.fillCircle(mx + 4, my - 12, 4, 0xD082);
        _canvas.fillCircle(mx + 5, my - 18, 2, 0x51E7);

        // Check ground impact / missed
        if (_meteorY > 186.0f) {
            _meteorY = -15.0f;
            _meteorX = 25.0f + (rand() % 85);
            _miniGameRound++;
            if (_miniGameRound > 3) {
                // Game over
                _miniGameActive = false;
                petMgr.getStateMutable().discipline += (_miniGameScore * 5);
                triggerSecretStars();
            }
        }
    } else {
        // Shattered explosion particles!
        uint32_t strikeElapsed = nowMillis - _miniGameStrikeTime;
        int mx = (int)_meteorX;
        int my = (int)_meteorY;

        _canvas.fillCircle(mx, my, 12 - (strikeElapsed / 30), 0xFFFF);
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
                petMgr.getStateMutable().discipline += (_miniGameScore * 6);
                triggerSecretStars();
            }
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
        // Cascading torrent of cyan & white water sheets
        for (int x = 8; x < SCREEN_WIDTH - 8; x += 10) {
            int waveY = (int)((elapsed * 0.35f) + (sinf(x * 0.2f) * 12.0f));
            _canvas.drawFastVLine(x, 0, min(186, waveY), 0x07FF);
            _canvas.drawFastVLine(x + 2, 0, min(186, waveY), 0xFFFF);
            _canvas.drawFastVLine(x + 4, 0, min(186, waveY), 0x05BF);
        }

        // Sparkling soap bubbles around pet
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
        // Pulsing concentric golden energy aura rings around the pet
        int cx = (int)_petX + 30;
        int cy = 145;
        int ringR = (int)((elapsed / 25) % 65);

        _canvas.drawCircle(cx, cy, ringR, 0xFDE0);
        _canvas.drawCircle(cx, cy, max(1, ringR - 4), 0xFFFF);
        _canvas.drawCircle(cx, cy, max(1, ringR - 8), 0xFA20);

        // Flashing lightning screen flash
        if ((elapsed / 80) % 2 == 0) {
            _canvas.drawRect(2, 2, SCREEN_WIDTH - 4, SCREEN_HEIGHT - 4, 0xFFFF);
        }

        // Announcement card
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
        // Draw cute smiling Bandai-style poop pile
        _canvas.fillRoundRect(px, py + 4, 14, 6, 2, 0x8A40); // bottom swirl
        _canvas.fillRoundRect(px + 2, py + 1, 10, 5, 2, 0x9260); // mid swirl
        _canvas.fillCircle(px + 7, py + 1, 3, 0xA280); // top tip
        // Eyes
        _canvas.drawPixel(px + 5, py + 3, 0xFFFF);
        _canvas.drawPixel(px + 9, py + 3, 0xFFFF);
        // Stink lines
        if ((millis() / 300 + i) % 2 == 0) {
            _canvas.drawLine(px + 4, py - 4, px + 2, py - 8, 0x39E8);
            _canvas.drawLine(px + 10, py - 4, px + 12, py - 8, 0x39E8);
        }
    }
}

void DisplayManager::drawEatingEffect(uint32_t elapsed) {
    // Chomping particles near mouth
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

    // Expanding sonic / energy shockwave pulses traveling outward
    int waveDist = (int)(elapsed * 0.16f) % 45;
    int wx = mx + (_facingRight ? waveDist : -waveDist);

    _canvas.drawCircle(wx, my, 6 + (waveDist / 4), 0xFFFF);
    _canvas.drawCircle(wx, my, 8 + (waveDist / 3), 0xFDE0);
    _canvas.drawCircle(wx, my, 12 + (waveDist / 2), 0xFA20);
}

void DisplayManager::drawZzzEffect(uint32_t elapsed) {
    (void)elapsed;
    // Zzz indicator rendered directly on sleep sprites
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
    // 30 FPS frame pacing to eliminate flicker
    if (nowMillis - _lastRenderTime < 33) {
        return;
    }
    _lastRenderTime = nowMillis;

    const PetState& state = petMgr.getState();

    // Pacing timers
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

    // Thought bubble generator: every 8 seconds in IDLE
    if (_animState == ANIM_IDLE && _activeThoughtType == 0 && (nowMillis - _lastThoughtTime >= 8000)) {
        _lastThoughtTime = nowMillis;
        _thoughtStartTime = nowMillis;
        if (state.hunger < 2) _activeThoughtType = 1; // meat
        else if (state.discipline > 50) _activeThoughtType = 4; // heart
        else _activeThoughtType = 2 + (rand() % 4);
    }

    // Determine pet boundary constraints based on scale
    float baseScale = (state.stage == STAGE_EGG) ? 1.25f : (1.25f + (state.stage - 1) * 0.15f);
    int halfW = (int)(16.0f * baseScale);
    int minX = halfW + 4;
    int maxX = SCREEN_WIDTH - halfW - 4;

    // Turn transition & Movement kinematics
    if (_animState == ANIM_IDLE && !state.isSleeping && state.stage != STAGE_EGG && !_isTurning) {
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
    }

    // Full-Motion Jump State Machine
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

    // Sleep state synchronization
    if (state.isSleeping && _animState != ANIM_SLEEP) {
        setAnimState(ANIM_SLEEP);
    } else if (!state.isSleeping && _animState == ANIM_SLEEP) {
        setAnimState(ANIM_IDLE);
    }

    // Render Living Scene
    drawBackground();
    drawGround();
    drawPoops(state.poops);
    drawPet(petMgr, nowMillis);

    // Feature Overlays
    if (_miniGameActive) {
        drawMiniGame(nowMillis, petMgr);
    } else if (_isCleaningGeyser) {
        drawCleanGeyser(nowMillis, petMgr);
    } else if (_isEvolvingSeq) {
        drawEvolutionSequence(nowMillis);
    }

    // Bottom Dock HUD and Menu
    drawHud(petMgr);

    // Stats Card Overlay
    if (_showStats) {
        drawStats(petMgr);
    }

    // Short feedback toast (e.g. "NOT HUNGRY")
    if (_toastText[0] && (int32_t)(_toastUntil - nowMillis) > 0) {
        int tw = (int)strlen(_toastText) * 6 + 12;
        int tx = (SCREEN_WIDTH - tw) / 2;
        _canvas.fillRoundRect(tx, 150, tw, 18, 5, 0x0000);
        _canvas.drawRoundRect(tx, 150, tw, 18, 5, 0xFFE0);
        _canvas.setTextColor(0xFFFF, 0x0000);
        _canvas.setTextSize(1);
        _canvas.setCursor(tx + 6, 155);
        _canvas.print(_toastText);
    }

    // Push completed 135x240 double-buffered frame to display at 30 FPS
    pushFrameToDisplay(_canvas);
}

// ==========================================
// Sprite & Bitmap Drawing Primitives
// ==========================================

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

    // Centered Title
    int titleLen = (int)strlen(title);
    int titleX = max(4, (SCREEN_WIDTH - titleLen * 6) / 2);
    _canvas.setCursor(titleX, 70);
    _canvas.print(title);

    // Status subtitle
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

    // Progress Bar
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
