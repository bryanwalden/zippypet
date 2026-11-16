#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include "Config.h"

// Core persistent Pet State structure
struct PetState {
    uint8_t species;          // 0=Baby .. 6=Mecha Cerato
    uint8_t stage;            // 0=Egg, 1..7=Levels 1..7
    uint8_t hunger;           // 0 to 4 (4 is completely full)
    uint8_t strength;         // 0 to 4 (4 is max energy/hearts)
    uint16_t discipline;      // Behavior/parent reward points (0-100+)
    uint16_t exp;             // Growth experience points
    uint32_t lastTickTime;    // Timestamp (millis or epoch) for hourly decay
    uint32_t lastSaveTime;    // Timestamp of last NVS commit
    bool isSleeping;          // Current sleep state

    // Supplemental status tracking
    uint8_t poops;            // Poop count on screen (0 to 4)
    uint16_t ageHours;        // Age in hours
    uint8_t careMistakes;     // Missed hunger calls
    bool isSick;              // Sickness condition from poop/neglect
    uint16_t totalInteractions;

    // Upgrade 1: Pedometer Step Counter & Fossils
    uint32_t totalSteps;      // Lifetime / current pet steps walked
    uint16_t fossils;         // Prehistoric fossil shards found while walking

    // Upgrade 3: Wireless ESP-NOW / IR Battle Record
    uint16_t battleWins;      // Total PvP / Sparring battle wins
    uint16_t battleLosses;    // Total battle losses

    // Upgrade 7: Unlockable Relics & Accessories
    uint8_t equippedAccessory;// PetAccessory enum (0=None, 1=Crown, 2=Visor, 3=Chef, 4=Aura)
    uint8_t unlockedRelicsMask;// Bitmask of unlocked accessories (bit 0..4)

    // Upgrade 10: Generational Legacy ("New Game+" Dino Eggs)
    uint8_t generation;       // Generation number (1 = Gen 1, 2 = Gen 2, ...)
    uint8_t shinyVariant;     // 0=Normal, 1=Albino/Frost, 2=Obsidian/Shadow, 3=Golden Solar
};

class PetStateManager {
public:
    PetStateManager();

    void begin();
    bool load();
    void save();
    void resetToEgg();

    // Game loop logic
    bool tick(uint32_t nowMillis);
    bool checkAutoSave(uint32_t nowMillis);

    // Gameplay interactions
    bool feed(bool isVitamin = false);
    bool train();
    bool clean();
    void rewardDiscipline(uint16_t amount);
    void setSleep(bool sleep);
    void cure();

    // Upgrade 1: Pedometer step reward handler (returns true if milestone loot found)
    bool recordStep();

    // Upgrade 3: Battle win/loss handler
    void recordBattleResult(bool won);

    // Upgrade 7: Accessory cycling & unlock evaluation
    void checkUnlocks();
    void cycleAccessory();
    const char* getAccessoryName() const;

    // Upgrade 10: Generational EXP bonus helper
    void addExpWithBonus(uint16_t baseExp);

    // Evolution and checks
    bool evaluateEvolution();
    bool needsAlert() const;
    const char* getSpeciesName() const;
    const char* getStageName() const;

    // Direct access to state
    const PetState& getState() const { return _state; }
    PetState& getStateMutable() { return _state; }

    bool hasPendingEvolution() const { return _pendingEvolution; }
    void clearPendingEvolution() { _pendingEvolution = false; }

private:
    PetState _state;
    Preferences _prefs;
    bool _pendingEvolution;
};
