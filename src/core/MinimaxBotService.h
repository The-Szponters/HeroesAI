/**
 * @file MinimaxBotService.h
 * @brief Depth-limited minimax (alpha-beta) battle AI.
 * @author Łukasz Szydlik
 */
#pragma once

#include <optional>
#include <vector>

#include "ActionCommand.h"
#include "IBot.h"

namespace core {

class GameManager;
class ActionGenerator;
class SpellResolver;

/**
 * @brief Minimax search with alpha-beta pruning over unit activations.
 *
 * Each search node corresponds to one unit's activation; whether it is a
 * maximizing or minimizing node is decided by the acting unit's side
 * (turns are initiative-ordered, not strictly alternating). The search
 * runs on deep clones of the live GameManager (see GameManager::clone),
 * applying the real move/attack/spell rules so it never diverges from
 * actual game behaviour. A material-based evaluation (army value
 * differential plus small positional / mana terms) scores leaf states.
 *
 * Implements IBot, so it is interchangeable with the simpler strategies.
 */
class MinimaxBotService : public IBot {
public:
    MinimaxBotService( GameManager& model, int depth );

    std::optional<ActionCommand> decideAction( models::Unit& active_unit ) override;

    // The alpha-beta search is expensive -- run it off the UI thread.
    bool wantsAsync( ) const override { return true; }

private:
    double alphaBeta( GameManager& state, int depth, double alpha, double beta, int our_side );
    double evaluate( GameManager& state, int our_side ) const;

    // Builds a branching-limited action set for @p actor on @p state.
    std::vector<ActionCommand>
    generateAndPrune( GameManager& state, ActionGenerator& generator, models::Unit& actor ) const;

    // Re-points an action's target unit from one state to the equivalent
    // unit (same board position) in @p destination.
    ActionCommand translate( const ActionCommand& source, GameManager& destination ) const;

    // Applies an action to a state using the real game rules. Returns
    // false if the action could not be applied (treated as skipped).
    bool applyAction( GameManager& state, SpellResolver& resolver,
                          const ActionCommand& command ) const;

    GameManager& model_;
    int depth_;
    long nodeBudget_ = 0;
};

} // namespace core
