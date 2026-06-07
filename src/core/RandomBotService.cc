/**
 * @file RandomBotService.cc
 * @brief Implementation of the uniform-random action bot.
 * @author Lukasz Szydlik
 */
#include "RandomBotService.h"

namespace core {

RandomBotService::RandomBotService( ActionGenerator& generator ) : generator_( generator ) {}

std::optional<ActionCommand> RandomBotService::decideAction( models::Unit& active_unit ) {
    const std::vector<ActionCommand> actions = generator_.generate( active_unit );
    if ( actions.empty( ) ) {
        return std::nullopt;
    }
    std::uniform_int_distribution<std::size_t> dist( 0, actions.size( ) - 1 );
    return actions[dist( rng_ )];
}

} // namespace core
