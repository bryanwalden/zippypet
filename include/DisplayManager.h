#pragma once

#include <Arduino.h>
#include <M5Unified.h>
#include "Config.h"
#include "PetState.h"

enum JumpPhase : uint8_t {
    JUMP_PHASE_NONE = 0,
    JUMP_PHASE_CROUCH_PRE,
    JUMP_PHASE_ASCENT,
    JUMP_PHASE_APEX_DESCENT,
    JUMP_PHASE_CROUCH_LAND
};

enum SkyTime : uint8_t {
    SKY_DAWN = 0,
    SKY_DAY,
    SKY_SUNSET,
    SKY_NIGHT,
    SKY_COUNT
};

// Upgrade 5: Dynamic Prehistoric Weather & Volcanic Eruption Events
enum WeatherType : uint8_t {
    WEATHER_CLEAR = 0,
    WEATHER_ASH_FALL,
    WEATHER_ACID_RAIN,
    WEATHER_ERUPTION,
    WEATHER_COUNT
};

// Upgrade 6: Second Mini-Game ("Tilt Lava Dodge")
enum MiniGameMode : uint8_t {
    MINIGAME_METEOR_STRIKE = 0,
    MINIGAME_TILT_LAVA_DODGE
};

class DisplayManager {
public:
    DisplayManager();

    void begin();
    void setBrightness(uint8_t brightness);
    uint8_t getBrightness() const { return _brightness; }

    // State & Animation setters
    void setAnimState(AnimState state);
    AnimState getAnimState() const { return _animState; }
    void triggerRoar();
    void triggerJump();
    void triggerSecretStars();
    void triggerEat(bool isPill);
    void triggerCleanGeyser();
    void triggerEvolutionSequence(uint8_t targetStage);

    // Mini-Games (Idea 6: alternates Meteor Strike & Tilt Lava Dodge)
    void startMiniGame();
    bool isMiniGameActive() const { return _miniGameActive; }
    void handleMiniGameInput();

    // Upgrade 3: Wireless ESP-NOW / IR PvP & Rival Dino Battle
    void startBattle(uint8_t currentStage);
    bool isBattleActive() const { return _battleActive; }
    void handleBattleInput();

    // Atmosphere & Weather (Idea 5)
    void toggleSkyTime();
    SkyTime getSkyTime() const { return _currentSky; }
    void cycleWeather();
    WeatherType getWeather() const { return _currentWeather; }

    // Menu HUD Control
    void setMenuOpen(bool open);
    bool isMenuOpen() const { return _menuOpen; }
    void cycleMenuItem();
    MenuItem getSelectedMenuItem() const { return _selectedMenu; }
    void toggleStatsOverlay();
    bool isStatsOpen() const { return _showStats; }
    void closeAllOverlays();
    void showToast(const char* text, uint32_t durationMs = 1600);

    // Render loop
    void update(PetStateManager& petMgr, float tiltBiasX, uint32_t nowMillis);

    // OTA screen rendering
    void showUpdateScreen(const char* title, const char* status, int progress, int total);
    void showOtaProgress(unsigned int progress, unsigned int total);

private:
    char     _toastText[24] = {0};
    uint32_t _toastUntil = 0;
    uint32_t _menuLastActivity = 0;
    M5Canvas _canvas;
    M5Canvas _spriteCanvas;
    uint8_t _brightness;
    AnimState _animState;
    uint32_t _animStartTime;
    uint32_t _lastFrameTime;
    uint8_t _frameIndex;
    uint32_t _lastWalkTime;
    uint8_t _walkFrameIndex;
    uint32_t _lastRenderTime;

    // Movement & Position
    float _petX;
    float _petY;
    float _petVx;
    bool _facingRight;

    // Turn transition state
    bool _isTurning;
    uint32_t _turnStartTime;
    bool _turnTargetFacingRight;

    // Jump Physics & State Machine
    JumpPhase _jumpPhase;
    uint32_t _jumpPhaseStartTime;
    float _jumpVy;
    bool _isJumping;

    // Menu state
    bool _menuOpen;
    MenuItem _selectedMenu;
    bool _showStats;

    // Feature 1 & Upgrade 5: Day/Night Cycle + Prehistoric Weather
    SkyTime _currentSky;
    uint32_t _lastSkyCycleTime;
    WeatherType _currentWeather;
    uint32_t _lastWeatherChangeTime;

    // Feature 2: Thought & Emotion Bubbles
    uint32_t _lastThoughtTime;
    uint32_t _thoughtStartTime;
    uint8_t _activeThoughtType;

    // Feature 3 & Upgrade 6: Dual Training Mini-Games (Meteor Strike + Tilt Lava Dodge)
    bool _miniGameActive;
    MiniGameMode _miniGameMode;
    uint8_t _nextMiniGameToggle;
    uint32_t _miniGameStartTime;
    float _meteorX;
    float _meteorY;
    float _meteorVy;
    float _bonusMeatX;
    float _bonusMeatY;
    int8_t _miniGameScore;
    int8_t _miniGameRound;
    bool _miniGameStruck;
    uint32_t _miniGameStrikeTime;

    // Upgrade 3: Wireless ESP-NOW / Rival Dino Battle State
    bool _battleActive;
    uint32_t _battleStartTime;
    int16_t _playerHp;
    int16_t _rivalHp;
    uint8_t _battleCharge;
    uint8_t _rivalStage;
    uint32_t _lastRivalAttackTime;
    bool _peerDiscovered;

    // Feature 4: Geyser Cleaning Animation
    bool _isCleaningGeyser;
    uint32_t _cleanStartTime;

    // Feature 5: Evolution Sequence
    bool _isEvolvingSeq;
    uint32_t _evoStartTime;
    uint8_t _evoTargetStage;

    // Sub-draw routines
    void drawBackground();
    void drawWeather(uint32_t nowMillis);
    void drawGround();
    void drawHud(const PetStateManager& petMgr);
    void drawMenu();
    void drawStats(const PetStateManager& petMgr);
    void drawPet(const PetStateManager& petMgr, uint32_t nowMillis);
    void drawAccessory(uint8_t acc, int headX, int headY, int centerX, int centerY, uint32_t nowMillis);
    void drawPoops(uint8_t count);
    void drawThoughtBubble(int petHeadX, int petHeadY, uint32_t nowMillis);
    void drawMiniGame(uint32_t nowMillis, float tiltBiasX, PetStateManager& petMgr);
    void drawBattle(uint32_t nowMillis, PetStateManager& petMgr);
    void drawCleanGeyser(uint32_t nowMillis, PetStateManager& petMgr);
    void drawEvolutionSequence(uint32_t nowMillis);
    void drawEatingEffect(uint32_t elapsed);
    void drawRoarEffect(uint32_t elapsed, const PetState& state);
    void drawZzzEffect(uint32_t elapsed);
    void drawStarsEffect(uint32_t elapsed);

    void drawBitmapScaled(int x, int y, const uint8_t* bitmap, int w, int h, int scale, uint16_t color, bool flipX = false);
    void drawSpriteRGB565(int x, int y, const SpriteInfo& sprite, bool flipX = false);
};
