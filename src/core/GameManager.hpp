#pragma once
#include <unordered_set>
#include <vector>
#include "hex.hpp"
#include "unit.hpp"
#include "board.hpp"
#include "../models/hero.hpp"
#include "ActionManager.hpp"
#include "RoundManager.hpp"

class GameManager {
public:
    GameManager() : round_manager(all_units_in_battle) {}
    GameManager(Hero blue_hero, Hero red_hero);

    Unit* get_current_unit(); 

    std::vector<Unit*> get_units_left_in_round() const;
    std::vector<Unit*> get_unit_queue_in_round() const;

    std::vector<Hex*> get_available_destinations(const Unit& unit) const;
    std::vector<std::pair<Unit*, Hex*>> get_available_attacks(const Unit& unit) const;

    std::vector<const Hex*> find_path(const Unit& unit, const Hex& dest_hex) const;

    int get_round_number() const { return round_number; }
    std::vector<Unit*> peek_next_round_order() const;

    Board& get_board() { return board; }
    const Board& get_board() const { return board; }
        bool are_allies(const Unit& first, const Unit& second) const;
        bool are_enemies(const Unit& first, const Unit& second) const;
    bool can_attack(const Unit& attacker, const Hex& target_hex) const;
    bool can_move(const Unit& unit, const Hex& dest_hex) const;

    bool will_shoot(const Unit& attacker, const Unit& defender) const;

    bool active_unit_has_morale_bonus() const { return morale_triggered_this_turn; }

    void next_turn();

    void move(Unit& unit, Hex& dest_hex);
    void attack(Unit& attacker, Unit& defender, Hex& attack_from_hex);
    void wait(Unit& unit);
    void defend(Unit& unit);

    const Hero& get_blue_hero() const { return blue_hero; }
    const Hero& get_red_hero() const { return red_hero; }

private:
    void remove_dead_unit(Unit& unit);
        bool hero_contains_unit(const Hero& hero, const Unit& unit) const;

    void consume_turn_or_burn_morale_bonus();

    void roll_morale_for_active(Unit* unit);

    Hero blue_hero;
    Hero red_hero;
    std::vector<Unit*> all_units_in_battle;
    int round_number = 1;

    Unit* last_morale_rolled_unit = nullptr;
    bool  morale_triggered_this_turn = false;
    int   morale_round_tracked = 1;
    std::unordered_set<Unit*> morale_rolled_units_this_round;

    ActionManager action_manager;
    RoundManager round_manager;
    Board board;
};
