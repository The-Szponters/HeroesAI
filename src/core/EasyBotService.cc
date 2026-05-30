/**
 * @file EasyBotService.cc
 * @brief Implementation of the fixed-priority heuristic bot.
 * @author Łukasz Szydlik
 */
#include "EasyBotService.h"

#include <array>
#include <vector>

namespace core {

EasyBotService::EasyBotService( ActionGenerator& generator ) : generator_( generator ) {}

std::optional<ActionCommand> EasyBotService::decideAction( models::Unit& active_unit ) {
    const std::vector<ActionCommand> actions = generator_.generate( active_unit );
    if ( actions.empty( ) ) {
        return std::nullopt;
    }

    // Most preferred first. The first category with any legal action
    // wins; the concrete action within it is chosen at random.
    static constexpr std::array<ActionType, 6> K_PRIORITY = {
        ActionType::CAST_SPELL,
        ActionType::RANGED_ATTACK,
        ActionType::MELEE_ATTACK,
        ActionType::WAIT,
        ActionType::MOVE,
        ActionType::DEFEND
    };

    for ( const ActionType wanted : K_PRIORITY ) {
        std::vector<const ActionCommand*> matches;
        for ( const ActionCommand& action : actions ) {
            if ( action.type_ == wanted ) {
                matches.push_back( &action );
            }
        }
        if ( ! matches.empty( ) ) {
            std::uniform_int_distribution<std::size_t> dist( 0, matches.size( ) - 1 );
            return *matches[dist( rng_ )];
        }
    }

    // Unreachable in practice (DEFEND is always generated for a live
    // unit), but stay safe rather than returning an empty optional.
    return actions.front( );
}

} // namespace core
