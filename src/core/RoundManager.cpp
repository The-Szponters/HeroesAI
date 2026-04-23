#include "RoundManager.hpp"
#include <algorithm>

namespace {
template <typename Queue>
void discard_dead_from_top(Queue& q) {
    while (!q.empty()) {
        Unit* top = q.top();
        if (top != nullptr && top->get_count() > 0) {
            break;
        }
        q.pop();
    }
}
}

void RoundManager::start_round() {
    while (!unactivated_units.empty()) unactivated_units.pop();
    while (!waited_units.empty()) waited_units.pop();

    for (auto* unit : all_units) {
        if (unit != nullptr && unit->get_count() > 0) {
            unactivated_units.push(unit);
        }
    }
}

Unit* RoundManager::get_current_unit() {
    discard_dead_from_top(unactivated_units);
    discard_dead_from_top(waited_units);

    if (!unactivated_units.empty()) {
        return unactivated_units.top();
    } else if (!waited_units.empty()) {
        return waited_units.top();
    }
    
    // Auto start new round if both are empty and we have units
    if (!all_units.empty()) {
        start_round();
        if (!unactivated_units.empty()) {
            return unactivated_units.top();
        }
    }
    return nullptr;
}

void RoundManager::end_current_unit_turn() {
    discard_dead_from_top(unactivated_units);
    discard_dead_from_top(waited_units);

    if (!unactivated_units.empty()) {
        unactivated_units.pop();
    } else if (!waited_units.empty()) {
        waited_units.pop();
    }
}

void RoundManager::wait_current_unit() {
    discard_dead_from_top(unactivated_units);

    if (!unactivated_units.empty()) {
        Unit* u = unactivated_units.top();
        unactivated_units.pop();
        if (u != nullptr && u->get_count() > 0) {
            waited_units.push(u);
        }
    }
}

std::vector<Unit*> RoundManager::get_units_left_in_round() const {
    std::vector<Unit*> left;
    auto temp_unactivated = unactivated_units;
    auto temp_waited = waited_units;
    
    while (!temp_unactivated.empty()) {
        Unit* unit = temp_unactivated.top();
        if (unit != nullptr && unit->get_count() > 0) {
            left.push_back(unit);
        }
        temp_unactivated.pop();
    }
    
    while (!temp_waited.empty()) {
        Unit* unit = temp_waited.top();
        if (unit != nullptr && unit->get_count() > 0) {
            left.push_back(unit);
        }
        temp_waited.pop();
    }
    
    return left;
}

std::vector<Unit*> RoundManager::get_unit_queue_in_round() const {
    std::vector<Unit*> sorted_units;
    sorted_units.reserve(all_units.size());
    for (Unit* unit : all_units) {
        if (unit != nullptr && unit->get_count() > 0) {
            sorted_units.push_back(unit);
        }
    }

    std::sort(sorted_units.begin(), sorted_units.end(), [](const Unit* a, const Unit* b) {
        return a->get_speed() > b->get_speed();
    });
    return sorted_units;
}
