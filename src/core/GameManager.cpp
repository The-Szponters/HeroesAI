#include "GameManager.hpp"
#include <algorithm>

namespace {

// Tail offset of a 2-hex unit, identical to the convention used by
// ActionManager / SfmlBattleView (right army faces left → tail extends right).
std::tuple<int, int, int> tail_delta(const Unit& u) {
    if (u.is_facing_left()) return { 1, 0, -1};
    return {-1, 0,  1};
}

void place_unit_on_board(Board& board, const std::shared_ptr<Unit>& unit) {
    if (!unit) return;
    try {
        board.get_hex(unit->get_q(), unit->get_r(), unit->get_s()).set_unit(unit);
    } catch (std::out_of_range&) {}
    if (unit->get_size() == 2) {
        auto [dq, dr, ds] = tail_delta(*unit);
        try {
            board.get_hex(unit->get_q() + dq, unit->get_r() + dr, unit->get_s() + ds).set_unit(unit);
        } catch (std::out_of_range&) {}
    }
}

} // namespace

GameManager::GameManager(Hero blue_hero, Hero red_hero)
    : blue_hero(std::move(blue_hero)), red_hero(std::move(red_hero)), round_manager(all_units_in_battle) {

    for (const auto& unit_ptr : this->blue_hero.get_army().get_units()) {
        if (unit_ptr) {
            all_units_in_battle.push_back(unit_ptr.get());
            place_unit_on_board(board, unit_ptr);
        }
    }

    for (const auto& unit_ptr : this->red_hero.get_army().get_units()) {
        if (unit_ptr) {
            all_units_in_battle.push_back(unit_ptr.get());
            place_unit_on_board(board, unit_ptr);
        }
    }

    round_manager.start_round();
}

Unit* GameManager::get_current_unit() {
    // Detect a round rollover: if the queue was empty just before this call
    // (no remaining units in either heap) and a new unit appears now, the
    // RoundManager auto-restarted the round and we must bump the counter.
    const std::size_t before = round_manager.get_units_left_in_round().size();
    Unit* u = round_manager.get_current_unit();
    if (u != nullptr && before == 0) {
        ++round_number;
    }
    return u;
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

std::vector<const Hex*> GameManager::find_path(const Unit& unit, const Hex& dest_hex) const {
    return action_manager.find_path(unit, dest_hex, board);
}

std::vector<Unit*> GameManager::peek_next_round_order() const {
    // Snapshot what initiative order the *next* round would have if it started
    // right now: every alive unit, ordered by descending speed (ties broken by
    // pointer for stability).  Units that died this round are excluded.
    std::vector<Unit*> next;
    next.reserve(all_units_in_battle.size());
    for (Unit* u : all_units_in_battle) {
        if (u != nullptr && u->get_count() > 0) next.push_back(u);
    }
    std::sort(next.begin(), next.end(), [](const Unit* a, const Unit* b) {
        if (a->get_speed() != b->get_speed()) return a->get_speed() > b->get_speed();
        return a < b;
    });
    return next;
}

std::vector<std::pair<Unit*, Hex*>> GameManager::get_available_attacks(const Unit& unit) const {
    std::vector<std::pair<Unit*, Hex*>> filtered;
    for (const auto& [target, hex] : action_manager.get_available_attacks(unit, board)) {
        if (target == nullptr || hex == nullptr) {
            continue;
        }
        if (are_enemies(unit, *target)) {
            filtered.push_back({target, hex});
        }
    }
    return filtered;
}

bool GameManager::hero_contains_unit(const Hero& hero, const Unit& unit) const {
    for (const auto& candidate : hero.get_army().get_units()) {
        if (candidate && candidate.get() == &unit) {
            return true;
        }
    }
    return false;
}

bool GameManager::are_allies(const Unit& first, const Unit& second) const {
    const bool first_blue = hero_contains_unit(blue_hero, first);
    const bool second_blue = hero_contains_unit(blue_hero, second);
    if (first_blue && second_blue) {
        return true;
    }

    const bool first_red = hero_contains_unit(red_hero, first);
    const bool second_red = hero_contains_unit(red_hero, second);
    if (first_red && second_red) {
        return true;
    }

    return false;
}

bool GameManager::are_enemies(const Unit& first, const Unit& second) const {
    const bool first_known = hero_contains_unit(blue_hero, first) || hero_contains_unit(red_hero, first);
    const bool second_known = hero_contains_unit(blue_hero, second) || hero_contains_unit(red_hero, second);
    if (!first_known || !second_known) {
        // Preserve legacy behavior for units outside hero armies.
        return &first != &second;
    }
    return !are_allies(first, second);
}

bool GameManager::can_attack(const Unit& attacker, const Hex& target_hex) const {
    const auto attacks = get_available_attacks(attacker);
    for (const auto& [target, hex] : attacks) {
        if (target != nullptr && hex == &target_hex) {
            return true;
        }
    }
    return false;
}

bool GameManager::can_move(const Unit& unit, const Hex& dest_hex) const {
    const auto destinations = get_available_destinations(unit);
    for (const Hex* hex : destinations) {
        if (hex == &dest_hex) {
            return true;
        }
    }
    return false;
}

void GameManager::next_turn() {
    // Current action methods already update turn state as needed.
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
