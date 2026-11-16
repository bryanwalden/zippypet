#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include "Config.h"

// Core persistent Pet State structure
struct PetState {
    uint8_t species;          // 0=Botamon, 1=Koromon, 2=Agumon, 3=Greymon
    uint8_t stage;            // 0=Egg, 1=Baby, 2=Rookie, 3=Champion
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
