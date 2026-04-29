#pragma once
#include <vector>
#include <queue>
#include "unit.hpp"

class RoundManager {
public:
    RoundManager(const std::vector<Unit*>& all_units) : all_units(all_units) {}

    void start_round();
    Unit* get_current_unit();
    void end_current_unit_turn();
    void wait_current_unit();

    std::vector<Unit*> get_units_left_in_round() const;
    std::vector<Unit*> get_unit_queue_in_round() const;

private:
    struct MaxHeapComparator {
        bool operator()(const Unit* a, const Unit* b) const {
            return a->get_speed() < b->get_speed(); 
        }
    };

    struct MinHeapComparator {
        bool operator()(const Unit* a, const Unit* b) const {
            return a->get_speed() > b->get_speed(); 
        }
    };

    const std::vector<Unit*>& all_units;

    std::priority_queue<Unit*, std::vector<Unit*>, MaxHeapComparator> unactivated_units;
    std::priority_queue<Unit*, std::vector<Unit*>, MinHeapComparator> waited_units;

    Unit* current_unit = nullptr;
};
