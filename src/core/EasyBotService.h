/**
 * @file EasyBotService.h
 * @brief A bot that picks actions by a fixed category priority.
 * @author Lukasz Szydlik
 */
#pragma once

#include <optional>
#include <random>

#include "ActionGenerator.h"
#include "IBot.h"

namespace core {

class GameManager;

/**
 * @brief Heuristic AI with a fixed action-category preference.
 *
 * Each turn it casts a (random) spell whenever it can, otherwise it
 * prefers, in order: ranged attack, melee attack, wait, move, and
 * finally defend. Attacks and spells pick a random target within the
 * category; a move always heads toward the nearest enemy. Implements
 * IBot, so it is interchangeable with the other strategies.
 */
class EasyBotService : public IBot {
public:
    EasyBotService( GameManager& model, ActionGenerator& generator );

    std::optional<ActionCommand> decideAction( models::Unit& active_unit ) override;

private:
    GameManager& model_;
    ActionGenerator& generator_;
    std::mt19937 rng_{ std::random_device{ }( ) };
};

} // namespace core
