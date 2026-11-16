#include "PetState.h"

static const char* PREFS_NAMESPACE = "digimon";
static const uint32_t NVS_MAGIC = 0xDE416001; // Versioned magic header

PetStateManager::PetStateManager() : _pendingEvolution(false) {
    memset(&_state, 0, sizeof(PetState));
}

void PetStateManager::begin() {
    _prefs.begin(PREFS_NAMESPACE, false);
    if (!load()) {
        resetToEgg();
        save();
    }
}

void PetStateManager::resetToEgg() {
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
}

bool PetStateManager::load() {
    uint32_t magic = _prefs.getUInt("magic", 0);
    if (magic != NVS_MAGIC) {
        return false;
    }

    _state.species = _prefs.getUChar("species", SPECIES_CERATO_BABY);
    _state.stage = _prefs.getUChar("stage", STAGE_EGG);
    _state.hunger = _prefs.getUChar("hunger", 4);
    _state.strength = _prefs.getUChar("strength", 4);
    _state.discipline = _prefs.getUShort("discipline", 20);
    _state.exp = _prefs.getUShort("exp", 0);
    _state.lastTickTime = millis(); // Refresh relative tick tracker
    _state.lastSaveTime = millis();
    _state.isSleeping = _prefs.getBool("isSleeping", false);
    _state.poops = _prefs.getUChar("poops", 0);
    _state.ageHours = _prefs.getUShort("ageHours", 0);
    _state.careMistakes = _prefs.getUChar("careMistakes", 0);
    _state.isSick = _prefs.getBool("isSick", false);
    _state.totalInteractions = _prefs.getUShort("interactions", 0);

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

    _state.lastSaveTime = millis();
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
        // Natural hunger decay
        if (_state.hunger > 0) {
            _state.hunger--;
        } else {
            _state.careMistakes++;
        }

        // Strength decay
        if (_state.strength > 0 && (_state.ageHours % 2 == 0)) {
            _state.strength--;
        }

        // Poop generation
        if (_state.hunger >= 2 && _state.poops < 4 && (_state.ageHours % 2 == 0)) {
            _state.poops++;
        }

        // Sickness check from prolonged mess or starvation
        if (_state.poops >= 3 || _state.hunger == 0) {
            if (random(0, 10) < 4) {
                _state.isSick = true;
            }
        }
    } else {
        // Sleeping pets recover energy slightly
        if (_state.strength < 4 && (_state.ageHours % 3 == 0)) {
            _state.strength++;
        }
    }

    // Natural experience growth from surviving hours
    _state.exp += 2;

    // Check if ready for evolution
    evaluateEvolution();
    return true;
}

bool PetStateManager::feed(bool isVitamin) {
    if (_state.isSleeping) return false;

    bool acted = false;
    if (isVitamin) {
        if (_state.strength < 4) {
            _state.strength++;
            _state.exp += 3;
            acted = true;
        }
    } else {
        if (_state.hunger < 4) {
            _state.hunger++;
            _state.exp += 2;
            acted = true;
        }
    }

    if (acted) {
        _state.totalInteractions++;
        evaluateEvolution();
    }
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
    _state.exp += 8;
    _state.discipline = min(150, _state.discipline + 2);
    _state.totalInteractions++;

    evaluateEvolution();
    return true;
}

bool PetStateManager::clean() {
    if (_state.poops > 0) {
        _state.poops = 0;
        _state.discipline = min(150, _state.discipline + 3);
        _state.exp += 2;
        _state.totalInteractions++;
        return true;
    }
    return false;
}

void PetStateManager::rewardDiscipline(uint16_t amount) {
    _state.discipline = min((uint16_t)250, (uint16_t)(_state.discipline + amount));
    _state.exp += amount * 2;
    _state.totalInteractions++;
    evaluateEvolution();
}

void PetStateManager::setSleep(bool sleep) {
    _state.isSleeping = sleep;
    save(); // Save immediately before sleep
}

void PetStateManager::cure() {
    if (_state.isSick) {
        _state.isSick = false;
        _state.discipline = min(150, _state.discipline + 5);
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
