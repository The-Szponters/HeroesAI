/**
 * @file RoundManager.cc
 * @brief Implementation of round scheduling and the wait/initiative logic.
 */
#include "RoundManager.h"
#include <algorithm>

namespace core {

using models::Unit;

namespace {
template <typename Queue>
void discardDeadFromTop( Queue& q ) {
    while ( ! q.empty( ) ) {
        Unit* top = q.top( );
        if ( top != nullptr && top->getCount( ) > 0 ) {
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
    discardDeadFromTop( unactivatedUnits_ );
    discardDeadFromTop( waitedUnits_ );

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
    discardDeadFromTop( unactivatedUnits_ );
    discardDeadFromTop( waitedUnits_ );

    if ( ! unactivatedUnits_.empty( ) ) {
        unactivatedUnits_.pop( );
    } else if ( ! waitedUnits_.empty( ) ) {
        waitedUnits_.pop( );
    }
}

void RoundManager::waitCurrentUnit( ) {
    discardDeadFromTop( unactivatedUnits_ );

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
        if ( unit != nullptr && unit->getCount( ) > 0 ) {
            left.push_back( unit );
        }
        temp_unactivated.pop( );
    }

    while ( ! temp_waited.empty( ) ) {
        Unit* unit = temp_waited.top( );
        if ( unit != nullptr && unit->getCount( ) > 0 ) {
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
        if ( u != nullptr && u->getCount( ) > 0 ) {
            result.push_back( u );
}
    }

    auto temp_waited = waitedUnits_;
    while ( ! temp_waited.empty( ) ) {
        Unit* u = temp_waited.top( );
        temp_waited.pop( );
        if ( u != nullptr && u->getCount( ) > 0 ) {
            result.push_back( u );
}
    }

    return result;
}

} // namespace core
