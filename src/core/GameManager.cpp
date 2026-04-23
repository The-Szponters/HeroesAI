#include "GameManager.hpp"
#include <algorithm>

GameManager::GameManager(Hero blue_hero, Hero red_hero)
    : blue_hero(std::move(blue_hero)), red_hero(std::move(red_hero)), round_manager(all_units_in_battle) {
    
    // Add units from blue hero
    for (const auto& unit_ptr : this->blue_hero.get_army().get_units()) {
        if (unit_ptr) {
            all_units_in_battle.push_back(unit_ptr.get());
            try {
                board.get_hex(unit_ptr->get_q(), unit_ptr->get_r(), unit_ptr->get_s()).set_unit(unit_ptr);
            } catch (std::out_of_range&) {}
        }
    }

    // Add units from red hero
    for (const auto& unit_ptr : this->red_hero.get_army().get_units()) {
        if (unit_ptr) {
            all_units_in_battle.push_back(unit_ptr.get());
            try {
                board.get_hex(unit_ptr->get_q(), unit_ptr->get_r(), unit_ptr->get_s()).set_unit(unit_ptr);
            } catch (std::out_of_range&) {}
        }
    }
    
    round_manager.start_round();
}

Unit* GameManager::get_current_unit() {
    return round_manager.get_current_unit();
}

std::vector<Unit*> GameManager::get_units_left_in_round() const {
    return round_manager.get_units_left_in_round();
}

std::vector<Unit*> GameManager::get_unit_queue_in_round() const {
    return round_manager.get_unit_queue_in_round();
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
    if (action_manager.attack(attacker, defender, attack_from_hex, board)) {
        // Attack returned true -> defender is dead
        remove_dead_unit(defender);
    }
    round_manager.end_current_unit_turn();
}

void GameManager::wait(Unit& unit) {
    round_manager.wait_current_unit();
}

void GameManager::defend(Unit& unit) {
    action_manager.defend(unit);
    round_manager.end_current_unit_turn();
}

void GameManager::remove_dead_unit(Unit& unit) {
    auto it = std::find(all_units_in_battle.begin(), all_units_in_battle.end(), &unit);
    if (it != all_units_in_battle.end()) {
        all_units_in_battle.erase(it);
    }
}
