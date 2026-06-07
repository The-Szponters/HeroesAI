/**
 * @file ActionGenerator.h
 * @brief Enumerates every legal ActionCommand for the active unit + hero.
 * @author Lukasz Szydlik
 */
#pragma once

#include <vector>

#include "ActionCommand.h"

namespace core {

class GameManager;
class SpellResolver;

/**
 * @brief Produces the full set of mathematically valid actions.
 *
 * Pure core logic: it only queries the GameManager (destinations,
 * attacks, shooting legality, hero/mana state) and the SpellResolver
 * (spell target validity), then packages each possibility as a discrete
 * ActionCommand. Both the RandomBotService and a future MinimaxBotService
 * consume the same list, so the search/evaluation layer stays decoupled
 * from how actions are generated.
 */
class ActionGenerator {
public:
    ActionGenerator( GameManager& model, SpellResolver& spell_resolver );

    /**
     * @brief Lists all legal actions for @p active_unit and its hero.
     * @param active_unit The currently active unit.
     * @return Every legal ActionCommand (move, attack, wait, defend, cast).
     */
    std::vector<ActionCommand> generate( models::Unit& active_unit ) const;

private:
    void appendMoves( models::Unit& active_unit, std::vector<ActionCommand>& out ) const;
    void appendMeleeAttacks( models::Unit& active_unit, std::vector<ActionCommand>& out ) const;
    void appendRangedAttacks( models::Unit& active_unit, std::vector<ActionCommand>& out ) const;
    void appendWaitDefend( models::Unit& active_unit, std::vector<ActionCommand>& out ) const;
    void appendSpells( models::Unit& active_unit, std::vector<ActionCommand>& out ) const;

    GameManager& model_;
    SpellResolver& spellResolver_;
};

} // namespace core
