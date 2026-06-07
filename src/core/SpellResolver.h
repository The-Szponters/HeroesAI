/**
 * @file SpellResolver.h
 * @brief Executes hero spells against target units.
 * @author Lukasz Szydlik
 */
#pragma once

#include <string>

#include "../models/Hero.h"
#include "../models/Spell.h"
#include "../models/Unit.h"
#include "GameManager.h"

namespace core {

/**
 * @brief Outcome of a single cast attempt.
 *
 * `success` is false when the cast was rejected (insufficient mana,
 * already cast this round, invalid target). The `message` is a short
 * human-readable string suitable for the HUD.
 */
struct SpellCastResult {
    bool success = false;
    std::string message;
};

/**
 * @brief Validates and applies spell effects on target units.
 *
 * Stateless aside from the model reference. The presenter constructs
 * one per BattlePresenter, passes the active hero + target unit, and
 * shows the resulting `message` to the player. On success the resolver
 * decrements mana and marks the hero as having cast this round so the
 * spellbook button greys out for the rest of the round.
 */
class SpellResolver {
public:
    explicit SpellResolver( GameManager& model );

    SpellCastResult tryCast( models::SpellId id,
                                  models::Hero& caster,
                                  models::Unit& target );

    bool isValidTarget( models::SpellId id,
                              const models::Unit& caster_unit,
                              const models::Unit& target ) const;

private:
    GameManager& model_;
};

} // namespace core
