#include "PetState.h"

static const char* PREFS_NAMESPACE = "digimon";
static const uint32_t NVS_MAGIC = 0xDE416002; // Versioned magic header for expanded state

PetStateManager::PetStateManager() : _pendingEvolution(false) {
    memset(&_state, 0, sizeof(PetState));
    _state.generation = 1;
}

void PetStateManager::begin() {
    _prefs.begin(PREFS_NAMESPACE, false);
    if (!load()) {
        _state.generation = 1;
        _state.shinyVariant = 0;
        _state.unlockedRelicsMask = 0x01; // ACC_NONE always unlocked
        resetToEgg();
        save();
    }
}

void PetStateManager::resetToEgg() {
    // Idea 10: Generational Legacy ("New Game+" Dino Eggs)
    // If resetting from a high-level dino (Level 5+) or after veteran age, advance Generation!
    if (_state.stage >= STAGE_ULTIMATE || _state.exp >= 100) {
        if (_state.generation < 99) {
            _state.generation++;
        }
        // Cycle or roll a rare Shiny palette variant for Gen 2+
        _state.shinyVariant = (_state.generation - 1) % 4;
        // Unlock Thunder Aura for reaching a new Generation!
        _state.unlockedRelicsMask |= (1 << ACC_THUNDER_AURA);
    } else if (_state.generation == 0) {
        _state.generation = 1;
    }

    _state.species = SPECIES_CERATO_BABY;
    _state.stage = STAGE_EGG;
    _state.hunger = 4;
    _state.strength = 4;
    _state.discipline = 0;
    _state.exp = 0;
    _state.lastTickTime = millis();
    _state.lastSaveTime = millis();
    _state.isSleeping = false;
    _state.poops = 0;
    _state.ageHours = 0;
    _state.careMistakes = 0;
    _state.isSick = false;
    _state.totalInteractions = 0;
    _pendingEvolution = false;
    checkUnlocks();
}

bool PetStateManager::load() {
    uint32_t magic = _prefs.getUInt("magic", 0);
    if (magic != NVS_MAGIC && magic != 0xDE416001) {
        return false;
    }

    _state.species = _prefs.getUChar("species", SPECIES_CERATO_BABY);
    _state.stage = _prefs.getUChar("stage", STAGE_EGG);
    _state.hunger = _prefs.getUChar("hunger", 4);
    _state.strength = _prefs.getUChar("strength", 4);
    _state.discipline = _prefs.getUShort("discipline", 20);
    _state.exp = _prefs.getUShort("exp", 0);
    _state.lastTickTime = millis();
    _state.lastSaveTime = millis();
    _state.isSleeping = _prefs.getBool("isSleeping", false);
    _state.poops = _prefs.getUChar("poops", 0);
    _state.ageHours = _prefs.getUShort("ageHours", 0);
    _state.careMistakes = _prefs.getUChar("careMistakes", 0);
    _state.isSick = _prefs.getBool("isSick", false);
    _state.totalInteractions = _prefs.getUShort("interactions", 0);

    // Load new upgrade fields with safe defaults
    _state.totalSteps = _prefs.getUInt("steps", 0);
    _state.fossils = _prefs.getUShort("fossils", 0);
    _state.battleWins = _prefs.getUShort("wins", 0);
    _state.battleLosses = _prefs.getUShort("losses", 0);
    _state.equippedAccessory = _prefs.getUChar("acc", ACC_NONE);
    _state.unlockedRelicsMask = _prefs.getUChar("relics", 0x01);
    _state.generation = _prefs.getUChar("gen", 1);
    if (_state.generation == 0) _state.generation = 1;
    _state.shinyVariant = _prefs.getUChar("shiny", 0);

    checkUnlocks();
    return true;
}

void PetStateManager::save() {
    _prefs.putUInt("magic", NVS_MAGIC);
    _prefs.putUChar("species", _state.species);
    _prefs.putUChar("stage", _state.stage);
    _prefs.putUChar("hunger", _state.hunger);
    _prefs.putUChar("strength", _state.strength);
    _prefs.putUShort("discipline", _state.discipline);
    _prefs.putUShort("exp", _state.exp);
    _prefs.putBool("isSleeping", _state.isSleeping);
    _prefs.putUChar("poops", _state.poops);
    _prefs.putUShort("ageHours", _state.ageHours);
    _prefs.putUChar("careMistakes", _state.careMistakes);
    _prefs.putBool("isSick", _state.isSick);
    _prefs.putUShort("interactions", _state.totalInteractions);

    _prefs.putUInt("steps", _state.totalSteps);
    _prefs.putUShort("fossils", _state.fossils);
    _prefs.putUShort("wins", _state.battleWins);
    _prefs.putUShort("losses", _state.battleLosses);
    _prefs.putUChar("acc", _state.equippedAccessory);
    _prefs.putUChar("relics", _state.unlockedRelicsMask);
    _prefs.putUChar("gen", _state.generation);
    _prefs.putUChar("shiny", _state.shinyVariant);

    _state.lastSaveTime = millis();
}

void PetStateManager::addExpWithBonus(uint16_t baseExp) {
    // Idea 10: +15% EXP bonus per generation above Gen 1
    uint32_t bonusPct = 100 + (uint32_t)(_state.generation - 1) * 15;
    uint16_t boosted = (uint16_t)max(1UL, (baseExp * bonusPct) / 100UL);
    _state.exp += boosted;
}

bool PetStateManager::recordStep() {
    _state.totalSteps++;
    bool lootFound = false;

    // Every 5 steps: +1 EXP
    if (_state.totalSteps % 5 == 0) {
        addExpWithBonus(1);
        evaluateEvolution();
    }

    // Every 25 steps: Discover a Prehistoric Fossil Shard + refill 1 Hunger/Strength heart!
    if (_state.totalSteps % 25 == 0) {
        _state.fossils++;
        if (_state.hunger < 4) _state.hunger++;
        if (_state.strength < 4) _state.strength++;
        addExpWithBonus(5);
        checkUnlocks();
        evaluateEvolution();
        lootFound = true;
    }
    return lootFound;
}

void PetStateManager::recordBattleResult(bool won) {
    if (won) {
        _state.battleWins++;
        _state.discipline = min(250, _state.discipline + 8);
        addExpWithBonus(15);
    } else {
        _state.battleLosses++;
        addExpWithBonus(4);
    }
    _state.totalInteractions++;
    checkUnlocks();
    evaluateEvolution();
    save();
}

void PetStateManager::checkUnlocks() {
    // All 4 accessories can be unlocked through gameplay milestones (and we also unlock Crown + Visor early if stats progress)
    _state.unlockedRelicsMask |= (1 << ACC_NONE);
    if (_state.stage >= STAGE_ROOKIE || _state.totalSteps >= 20 || _state.fossils >= 1) {
        _state.unlockedRelicsMask |= (1 << ACC_CROWN);
    }
    if (_state.discipline >= 30 || _state.exp >= 20) {
        _state.unlockedRelicsMask |= (1 << ACC_CYBER_VISOR);
    }
    if (_state.totalInteractions >= 2 || _state.hunger == 4) {
        _state.unlockedRelicsMask |= (1 << ACC_CHEF_HAT);
    }
    if (_state.battleWins >= 1 || _state.generation >= 2 || _state.stage >= STAGE_ULTIMATE) {
        _state.unlockedRelicsMask |= (1 << ACC_THUNDER_AURA);
    }
}

void PetStateManager::cycleAccessory() {
    checkUnlocks();
    for (int i = 1; i <= ACC_COUNT; ++i) {
        uint8_t nextAcc = (_state.equippedAccessory + i) % ACC_COUNT;
        // Allow cycling through unlocked relics (or all relics once any milestone is reached)
        if ((_state.unlockedRelicsMask & (1 << nextAcc)) || nextAcc == ACC_NONE) {
            _state.equippedAccessory = nextAcc;
            break;
        }
    }
    save();
}

const char* PetStateManager::getAccessoryName() const {
    switch (_state.equippedAccessory) {
        case ACC_CROWN:        return "Dino Crown";
        case ACC_CYBER_VISOR:  return "Cyber Visor";
        case ACC_CHEF_HAT:     return "Chef Hat";
        case ACC_THUNDER_AURA: return "Thunder Aura";
        default:               return "None";
    }
}

bool PetStateManager::checkAutoSave(uint32_t nowMillis) {
    if (nowMillis - _state.lastSaveTime >= NVS_SAVE_INTERVAL_MS) {
        save();
        return true;
    }
    return false;
}

bool PetStateManager::tick(uint32_t nowMillis) {
    if (nowMillis - _state.lastTickTime < HOURLY_TICK_INTERVAL_MS) {
        return false;
    }

    _state.lastTickTime = nowMillis;
    _state.ageHours++;

    if (!_state.isSleeping) {
        if (_state.hunger > 0) {
            _state.hunger--;
        } else {
            _state.careMistakes++;
        }

        if (_state.strength > 0 && (_state.ageHours % 2 == 0)) {
            _state.strength--;
        }

        if (_state.hunger >= 2 && _state.poops < 4 && (_state.ageHours % 2 == 0)) {
            _state.poops++;
        }

        if (_state.poops >= 3 || _state.hunger == 0) {
            if (random(0, 10) < 4) {
                _state.isSick = true;
            }
        }
    } else {
        if (_state.strength < 4 && (_state.ageHours % 3 == 0)) {
            _state.strength++;
        }
    }

    addExpWithBonus(2);
    evaluateEvolution();
    return true;
}

bool PetStateManager::feed(bool isVitamin) {
    if (_state.isSleeping) return false;

    bool acted = false;
    if (isVitamin) {
        if (_state.strength < 4) {
            _state.strength++;
            addExpWithBonus(3);
            acted = true;
        }
    } else {
        if (_state.hunger < 4) {
            _state.hunger++;
            addExpWithBonus(2);
            acted = true;
        }
    }

    _state.totalInteractions++;
    checkUnlocks();
    evaluateEvolution();
    return acted;
}

bool PetStateManager::train() {
    if (_state.isSleeping || _state.isSick) return false;

    if (_state.strength < 4) {
        _state.strength++;
    }
    if (_state.hunger > 0 && random(0, 2) == 0) {
        _state.hunger--;
    }
    addExpWithBonus(8);
    _state.discipline = min(250, _state.discipline + 2);
    _state.totalInteractions++;

    checkUnlocks();
    evaluateEvolution();
    return true;
}

bool PetStateManager::clean() {
    if (_state.poops > 0) {
        _state.poops = 0;
        _state.discipline = min(250, _state.discipline + 3);
        addExpWithBonus(2);
        _state.totalInteractions++;
        return true;
    }
    return false;
}

void PetStateManager::rewardDiscipline(uint16_t amount) {
    _state.discipline = min((uint16_t)250, (uint16_t)(_state.discipline + amount));
    addExpWithBonus(amount * 2);
    _state.totalInteractions++;
    checkUnlocks();
    evaluateEvolution();
}

void PetStateManager::setSleep(bool sleep) {
    _state.isSleeping = sleep;
    save();
}

void PetStateManager::cure() {
    if (_state.isSick) {
        _state.isSick = false;
        _state.discipline = min(250, _state.discipline + 5);
        _state.totalInteractions++;
    }
}

bool PetStateManager::evaluateEvolution() {
    if (_pendingEvolution) return false;

    // Stage 0: Egg -> Level 1 (Baby Cerato)
    if (_state.stage == STAGE_EGG) {
        if (_state.totalInteractions >= 3 || _state.exp >= 5 || _state.ageHours >= 1) {
            _state.stage = STAGE_BABY;
            _state.species = SPECIES_CERATO_BABY;
            _pendingEvolution = true;
            checkUnlocks();
            save();
            return true;
        }
    }
    // Stage 1: Level 1 -> Level 2 (Juvenile Cerato)
    else if (_state.stage == STAGE_BABY) {
        if (_state.exp >= 30 || _state.ageHours >= 2) {
            _state.stage = STAGE_TRAINING;
            _state.species = SPECIES_CERATO_JUVENILE;
            _pendingEvolution = true;
            checkUnlocks();
            save();
            return true;
        }
    }
    // Stage 2: Level 2 -> Level 3 (Ceratosaurus Rookie)
    else if (_state.stage == STAGE_TRAINING) {
        if (_state.exp >= 75 || _state.ageHours >= 4) {
            _state.stage = STAGE_ROOKIE;
            _state.species = SPECIES_CERATO_ROOKIE;
            _pendingEvolution = true;
            checkUnlocks();
            save();
            return true;
        }
    }
    // Stage 3: Level 3 -> Level 4 (Pyro Cerato)
    else if (_state.stage == STAGE_ROOKIE) {
        if (_state.exp >= 150 && _state.discipline >= 30 && _state.careMistakes <= 4) {
            _state.stage = STAGE_CHAMPION;
            _state.species = SPECIES_CERATO_CHAMPION;
            _pendingEvolution = true;
            checkUnlocks();
            save();
            return true;
        }
    }
    // Stage 4: Level 4 -> Level 5 (Flare Cerato)
    else if (_state.stage == STAGE_CHAMPION) {
        if (_state.exp >= 260 && _state.discipline >= 60 && _state.careMistakes <= 3) {
            _state.stage = STAGE_ULTIMATE;
            _state.species = SPECIES_CERATO_ULTIMATE;
            _pendingEvolution = true;
            checkUnlocks();
            save();
            return true;
        }
    }
    // Stage 5: Level 5 -> Level 6 (Metal Cerato Cyborg)
    else if (_state.stage == STAGE_ULTIMATE) {
        if (_state.exp >= 400 && _state.discipline >= 90) {
            _state.stage = STAGE_CYBORG;
            _state.species = SPECIES_CERATO_CYBORG;
            _pendingEvolution = true;
            checkUnlocks();
            save();
            return true;
        }
    }
    // Stage 6: Level 6 -> Level 7 (Mecha Cerato Laser Cannon)
    else if (_state.stage == STAGE_CYBORG) {
        if (_state.exp >= 600 && _state.discipline >= 120) {
            _state.stage = STAGE_MECHA;
            _state.species = SPECIES_CERATO_MECHA;
            _pendingEvolution = true;
            checkUnlocks();
            save();
            return true;
        }
    }

    return false;
}

bool PetStateManager::needsAlert() const {
    if (_state.isSleeping) return false;
    return (_state.hunger == 0 || _state.isSick || _state.poops >= 3);
}

const char* PetStateManager::getSpeciesName() const {
    if (_state.stage == STAGE_EGG) {
        return "Dino Egg";
    }
    switch (_state.species) {
        case SPECIES_CERATO_BABY:     return "Baby Cerato";
        case SPECIES_CERATO_JUVENILE: return "Juvenile Cerato";
        case SPECIES_CERATO_ROOKIE:   return "Ceratosaurus";
        case SPECIES_CERATO_CHAMPION: return "Pyro Cerato";
        case SPECIES_CERATO_ULTIMATE: return "Flare Cerato";
        case SPECIES_CERATO_CYBORG:   return "Metal Cerato";
        case SPECIES_CERATO_MECHA:    return "Mecha Cerato";
        default:                      return "Ceratosaurus";
    }
}

const char* PetStateManager::getStageName() const {
    switch (_state.stage) {
        case STAGE_EGG:        return "Egg";
        case STAGE_BABY:       return "Level 1";
        case STAGE_TRAINING:   return "Level 2";
        case STAGE_ROOKIE:     return "Level 3";
        case STAGE_CHAMPION:   return "Level 4";
        case STAGE_ULTIMATE:   return "Level 5";
        case STAGE_CYBORG:     return "Level 6";
        case STAGE_MECHA:      return "Level 7";
        default:               return "Stage";
    }
}
