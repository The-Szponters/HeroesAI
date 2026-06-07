/**
 * @file RandomBotService.h
 * @brief A bot that picks a uniformly-random legal action.
 * @author Lukasz Szydlik
 */
#pragma once

#include <optional>
#include <random>

#include "ActionGenerator.h"
#include "IBot.h"

namespace core {

/**
 * @brief Baseline AI: chooses uniformly at random among all legal actions.
 *
 * Validates the action pipeline end-to-end and provides a sparring
 * partner before the Minimax strategy lands. It implements IBot, so the
 * presenter that drives it is agnostic to the concrete strategy.
 */
class RandomBotService : public IBot {
public:
    explicit RandomBotService( ActionGenerator& generator );

    std::optional<ActionCommand> decideAction( models::Unit& active_unit ) override;

private:
    ActionGenerator& generator_;
    std::mt19937 rng_{ std::random_device{ }( ) };
};

} // namespace core
