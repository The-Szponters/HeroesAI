#include "GameManager.hpp"
#include <algorithm>

Unit* GameManager::get_current_unit() {
    return round_manager.get_current_unit();
}

std::vector<Unit*> GameManager::get_units_left_in_round() const {
    return round_manager.get_units_left_in_round();
}

std::vector<Unit*> GameManager::get_unit_queue_in_round() const {
    return round_manager.get_unit_queue_in_round(all_units_in_battle);
}

std::vector<Hex*> GameManager::get_available_destinations(const Unit& unit) const {
    return action_manager.get_available_destinations(unit, board);
}

std::vector<std::pair<Unit*, Hex*>> GameManager::get_available_attacks(const Unit& unit) const {
    return action_manager.get_available_attacks(unit, board);
}

void GameManager::move(Unit& unit, Hex& dest_hex) {
    action_manager.move(unit, dest_hex, board);
    round_manager.end_current_unit_turn();
}

void GameManager::attack(Unit& attacker, Unit& defender, Hex& attack_from_hex) {
    action_manager.attack(attacker, defender, attack_from_hex, board);
    round_manager.end_current_unit_turn();
}

void GameManager::wait(Unit& unit) {
    round_manager.wait_current_unit();
}

void GameManager::defend(Unit& unit) {
    action_manager.defend(unit);
    round_manager.end_current_unit_turn();
}
