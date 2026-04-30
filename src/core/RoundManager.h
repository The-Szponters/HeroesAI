/**
 * @file RoundManager.h
 * @brief Per-round turn ordering for all units in a battle.
 */
#pragma once
#include <vector>
#include <queue>
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
    RoundManager(const std::vector<models::Unit*>& all_units) : all_units(all_units ){}

    void start_round( );
    models::Unit* get_current_unit( );
    void end_current_unit_turn( );
    void wait_current_unit( );

    std::vector<models::Unit*> get_units_left_in_round() const;
    std::vector<models::Unit*> get_unit_queue_in_round() const;

private:
    /**
     * @brief Comparator for the max-heap of unactivated units (fastest first).
     */
    struct MaxHeapComparator {
        bool operator()(const models::Unit* a, const models::Unit* b) const {
            return a->get_speed() < b->get_speed( ); 
        }
    };

    /**
     * @brief Comparator for the min-heap of waited units (slowest first).
     */
    struct MinHeapComparator {
        bool operator()(const models::Unit* a, const models::Unit* b) const {
            return a->get_speed() > b->get_speed( ); 
        }
    };

    const std::vector<models::Unit*>& all_units;

    std::priority_queue<models::Unit*, std::vector<models::Unit*>, MaxHeapComparator> unactivated_units;
    std::priority_queue<models::Unit*, std::vector<models::Unit*>, MinHeapComparator> waited_units;

    models::Unit* current_unit = nullptr;
};

}  // namespace core
