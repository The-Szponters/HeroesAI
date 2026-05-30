/**
 * @file EasyBotService.h
 * @brief A bot that picks actions by a fixed category priority.
 * @author Łukasz Szydlik
 */
#pragma once

#include <optional>
#include <random>

#include "ActionGenerator.h"
#include "IBot.h"

namespace core {

/**
 * @brief Heuristic AI with a fixed action-category preference.
 *
 * Each turn it casts a (random) spell whenever it can, otherwise it
 * prefers, in order: ranged attack, melee attack, wait, move, and
 * finally defend. Within the chosen category the concrete action (which
 * spell, which target, which hex) is picked at random, so it stays
 * unpredictable while behaving more purposefully than the pure random
 * bot. Implements IBot, so it is interchangeable with the other
 * strategies.
 */
class EasyBotService : public IBot {
public:
    explicit EasyBotService( ActionGenerator& generator );

    std::optional<ActionCommand> decideAction( models::Unit& active_unit ) override;

private:
    ActionGenerator& generator_;
    std::mt19937 rng_{ std::random_device{ }( ) };
};

} // namespace core
