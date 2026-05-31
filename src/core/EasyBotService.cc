/**
 * @file EasyBotService.cc
 * @brief Implementation of the fixed-priority heuristic bot.
 * @author Łukasz Szydlik
 */
#include "EasyBotService.h"

#include <algorithm>
#include <array>
#include <climits>
#include <cmath>
#include <vector>

#include "../models/Unit.h"
#include "GameManager.h"

namespace core {

namespace {

int hexDistance( int aq, int ar, int bq, int br ) {
    const int as = -aq - ar;
    const int bs = -bq - br;
    return std::max( { std::abs( aq - bq ), std::abs( ar - br ), std::abs( as - bs ) } );
}

// Smallest hex distance from (q, r) to any living enemy of @p actor;
// INT_MAX when no enemy remains.
int nearestEnemyDistance( GameManager& model, const models::Unit& actor, int q, int r ) {
    int best = INT_MAX;
    for ( models::Unit* u : model.getAllUnits( ) ) {
        if ( u == nullptr || u->getCount( ) <= 0 ) {
            continue;
        }
        if ( ! model.areEnemies( actor, *u ) ) {
            continue;
        }
        best = std::min( best, hexDistance( q, r, u->getQ( ), u->getR( ) ) );
    }
    return best;
}

} // namespace

EasyBotService::EasyBotService( GameManager& model, ActionGenerator& generator )
    : model_( model ), generator_( generator ) {}

std::optional<ActionCommand> EasyBotService::decideAction( models::Unit& active_unit ) {
    const std::vector<ActionCommand> actions = generator_.generate( active_unit );
    if ( actions.empty( ) ) {
        return std::nullopt;
    }

    // Most preferred first. The first category with any legal action wins.
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
        if ( matches.empty( ) ) {
            continue;
        }

        if ( wanted == ActionType::MOVE ) {
            // Move toward the nearest enemy: keep the destination(s) with
            // the smallest distance to any enemy, then break ties randomly.
            int best_distance = INT_MAX;
            std::vector<const ActionCommand*> closest;
            for ( const ActionCommand* move : matches ) {
                const int d =
                    nearestEnemyDistance( model_, active_unit, move->destQ_, move->destR_ );
                if ( d < best_distance ) {
                    best_distance = d;
                    closest.clear( );
                    closest.push_back( move );
                } else if ( d == best_distance ) {
                    closest.push_back( move );
                }
            }
            if ( closest.empty( ) ) {
                closest = matches; // no enemies left -- any move is fine
            }
            std::uniform_int_distribution<std::size_t> dist( 0, closest.size( ) - 1 );
            return *closest[dist( rng_ )];
        }

        std::uniform_int_distribution<std::size_t> dist( 0, matches.size( ) - 1 );
        return *matches[dist( rng_ )];
    }

    // Unreachable in practice (DEFEND is always generated for a live
    // unit), but stay safe rather than returning an empty optional.
    return actions.front( );
}

} // namespace core
