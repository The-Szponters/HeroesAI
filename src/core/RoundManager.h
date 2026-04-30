/**
 * @file RoundManager.h
 * @brief Per-round turn ordering for all units in a battle.
 * @author Dominik Śledziewski
 */
#pragma once
#include <queue>
#include <vector>

#include "Unit.h"

namespace core {

/**
 * @brief Schedules whose turn it is in the current battle round.
 *
 * Faster units act first; units that wait are deferred to act in
 * reverse-speed order at the end of the round, matching classic
 * HoMM3 initiative rules.
 */
class RoundManager {
public:
    RoundManager( const std::vector<models::Unit*>& all_units ) : allUnits_( all_units ) {}

    void startRound( );
    models::Unit* getCurrentUnit( );
    void endCurrentUnitTurn( );
    void waitCurrentUnit( );

    std::vector<models::Unit*> getUnitsLeftInRound( ) const;
    std::vector<models::Unit*> getUnitQueueInRound( ) const;

private:
    /**
     * @brief Comparator for the max-heap of unactivated units (fastest first).
     */
    struct MaxHeapComparator {
        bool operator( )( const models::Unit* a, const models::Unit* b ) const {
            return a->getSpeed( ) < b->getSpeed( );
        }
    };

    /**
     * @brief Comparator for the min-heap of waited units (slowest first).
     */
    struct MinHeapComparator {
        bool operator( )( const models::Unit* a, const models::Unit* b ) const {
            return a->getSpeed( ) > b->getSpeed( );
        }
    };

    const std::vector<models::Unit*>& allUnits_;

    std::priority_queue<models::Unit*, std::vector<models::Unit*>, MaxHeapComparator>
        unactivatedUnits_;
    std::priority_queue<models::Unit*, std::vector<models::Unit*>, MinHeapComparator> waitedUnits_;

    models::Unit* currentUnit_ = nullptr;
};

} // namespace core
