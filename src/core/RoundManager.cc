/**
 * @file RoundManager.cc
 * @brief Implementation of round scheduling and the wait/initiative logic.
 * @author Dominik Śledziewski
 */
#include <algorithm>

#include "RoundManager.h"

namespace core {

using models::Unit;

namespace {
// A unit with speed 0 (e.g. blinded) is treated as if it weren't in
// the round at all -- can't activate, doesn't show up in the queue UI.
bool canAct( const Unit* u ) {
    return u != nullptr && u->getCount( ) > 0 && u->getSpeed( ) > 0;
}

template <typename Queue>
void discardSkippedFromTop( Queue& q ) {
    while ( ! q.empty( ) ) {
        Unit* top = q.top( );
        if ( canAct( top ) ) {
            break;
        }
        q.pop( );
    }
}
} // namespace

void RoundManager::startRound( ) {
    while ( ! unactivatedUnits_.empty( ) ) {
        unactivatedUnits_.pop( );
}
    while ( ! waitedUnits_.empty( ) ) {
        waitedUnits_.pop( );
}

    for ( auto* unit : allUnits_ ) {
        if ( unit != nullptr && unit->getCount( ) > 0 ) {
            unactivatedUnits_.push( unit );
        }
    }
}

Unit* RoundManager::getCurrentUnit( ) {
    discardSkippedFromTop( unactivatedUnits_ );
    discardSkippedFromTop( waitedUnits_ );

    if ( ! unactivatedUnits_.empty( ) ) {
        return unactivatedUnits_.top( );
    } else if ( ! waitedUnits_.empty( ) ) {
        return waitedUnits_.top( );
    }

    if ( ! allUnits_.empty( ) ) {
        startRound( );
        if ( ! unactivatedUnits_.empty( ) ) {
            return unactivatedUnits_.top( );
        }
    }
    return nullptr;
}

void RoundManager::endCurrentUnitTurn( ) {
    discardSkippedFromTop( unactivatedUnits_ );
    discardSkippedFromTop( waitedUnits_ );

    if ( ! unactivatedUnits_.empty( ) ) {
        unactivatedUnits_.pop( );
    } else if ( ! waitedUnits_.empty( ) ) {
        waitedUnits_.pop( );
    }
}

void RoundManager::waitCurrentUnit( ) {
    discardSkippedFromTop( unactivatedUnits_ );

    if ( ! unactivatedUnits_.empty( ) ) {
        Unit* u = unactivatedUnits_.top( );
        unactivatedUnits_.pop( );
        if ( u != nullptr && u->getCount( ) > 0 ) {
            waitedUnits_.push( u );
        }
    }
}

std::vector<Unit*> RoundManager::getUnitsLeftInRound( ) const {
    std::vector<Unit*> left;
    auto temp_unactivated = unactivatedUnits_;
    auto temp_waited = waitedUnits_;

    while ( ! temp_unactivated.empty( ) ) {
        Unit* unit = temp_unactivated.top( );
        if ( canAct( unit ) ) {
            left.push_back( unit );
        }
        temp_unactivated.pop( );
    }

    while ( ! temp_waited.empty( ) ) {
        Unit* unit = temp_waited.top( );
        if ( canAct( unit ) ) {
            left.push_back( unit );
        }
        temp_waited.pop( );
    }

    return left;
}

std::vector<Unit*> RoundManager::getUnitQueueInRound( ) const {
    std::vector<Unit*> result;

    auto temp_unactivated = unactivatedUnits_;
    while ( ! temp_unactivated.empty( ) ) {
        Unit* u = temp_unactivated.top( );
        temp_unactivated.pop( );
        if ( canAct( u ) ) {
            result.push_back( u );
}
    }

    auto temp_waited = waitedUnits_;
    while ( ! temp_waited.empty( ) ) {
        Unit* u = temp_waited.top( );
        temp_waited.pop( );
        if ( canAct( u ) ) {
            result.push_back( u );
}
    }

    return result;
}

} // namespace core
