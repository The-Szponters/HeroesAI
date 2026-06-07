/**
 * @file IBot.h
 * @brief Strategy interface for an AI that picks a battle action.
 * @author Lukasz Szydlik
 */
#pragma once

#include <optional>

#include "../models/Unit.h"
#include "ActionCommand.h"

namespace core {

/**
 * @brief Pluggable decision strategy for the currently active unit.
 *
 * Implementations choose one ActionCommand for @p active_unit (and its
 * owning hero). RandomBotService picks uniformly at random today; a
 * future MinimaxBotService will implement the same contract using
 * lookahead + evaluation, so the presenter that drives the bot never
 * needs to change.
 */
class IBot {
public:
    virtual ~IBot( ) = default;

    /**
     * @brief Chooses an action for @p active_unit.
     * @param active_unit The unit whose turn it currently is.
     * @return The chosen action, or std::nullopt when no legal action exists.
     */
    virtual std::optional<ActionCommand> decideAction( models::Unit& active_unit ) = 0;

    /**
     * @brief Whether decideAction is heavy enough to run off the UI thread.
     *
     * The presenter runs such strategies on a worker thread so the window
     * keeps rendering while they think. Cheap strategies (random, easy)
     * return false and are evaluated synchronously.
     */
    virtual bool wantsAsync( ) const { return false; }
};

} // namespace core
