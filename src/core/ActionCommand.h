/**
 * @file ActionCommand.h
 * @brief Discrete, evaluable representation of a single battle action.
 * @author Łukasz Szydlik
 */
#pragma once

#include "../models/Spell.h"
#include "../models/Unit.h"

namespace core {

/**
 * @brief The kind of action an ActionCommand describes.
 */
enum class ActionType {
    MOVE,
    MELEE_ATTACK,
    RANGED_ATTACK,
    WAIT,
    DEFEND,
    CAST_SPELL
};

/**
 * @brief One concrete, ready-to-execute battle action.
 *
 * Actions are produced as plain value objects so they can be enumerated,
 * evaluated and executed independently of who chose them (a human click,
 * the RandomBotService, or a future MinimaxBotService). Only the fields
 * relevant to @ref type_ are meaningful:
 *  - MOVE / MELEE_ATTACK use @ref destQ_ / @ref destR_ as the approach
 *    head hex (for MELEE_ATTACK in place this equals the unit's current
 *    head).
 *  - MELEE_ATTACK / RANGED_ATTACK / CAST_SPELL use @ref target_.
 *  - CAST_SPELL uses @ref spellId_.
 */
struct ActionCommand {
    ActionType type_ = ActionType::WAIT;

    int destQ_ = 0;
    int destR_ = 0;

    models::Unit* target_ = nullptr;

    models::SpellId spellId_ = models::SpellId::MAGIC_ARROW;
};

} // namespace core
