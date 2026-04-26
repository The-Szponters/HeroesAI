#pragma once
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

    // Round queue management

    // Returns a pointer so it can return nullptr if the battle is over
    Unit* get_current_unit(); 

    // Must be pointers because std::vector<Unit&> is illegal
    std::vector<Unit*> get_units_left_in_round() const;
    std::vector<Unit*> get_unit_queue_in_round() const;

    // Action availability queries

    std::vector<Hex*> get_available_destinations(const Unit& unit) const;
    std::vector<std::pair<Unit*, Hex*>> get_available_attacks(const Unit& unit) const;
    // Path chain (start..dest inclusive) the View can walk for per-hop facing.
    std::vector<const Hex*> find_path(const Unit& unit, const Hex& dest_hex) const;
    // Round bookkeeping for the lookahead turn queue.
    int get_round_number() const { return round_number; }
    std::vector<Unit*> peek_next_round_order() const;

    Board& get_board() { return board; }
    const Board& get_board() const { return board; }
        bool are_allies(const Unit& first, const Unit& second) const;
        bool are_enemies(const Unit& first, const Unit& second) const;
    bool can_attack(const Unit& attacker, const Hex& target_hex) const;
    bool can_move(const Unit& unit, const Hex& dest_hex) const;
    void next_turn();

    // Actions

    void move(Unit& unit, Hex& dest_hex);
    void attack(Unit& attacker, Unit& defender, Hex& attack_from_hex);
    void wait(Unit& unit);
    void defend(Unit& unit);

    const Hero& get_blue_hero() const { return blue_hero; }
    const Hero& get_red_hero() const { return red_hero; }

private:
    void remove_dead_unit(Unit& unit);
        bool hero_contains_unit(const Hero& hero, const Unit& unit) const;

    Hero blue_hero;
    Hero red_hero;
    std::vector<Unit*> all_units_in_battle;
    int round_number = 1;

    // Board and units
    ActionManager action_manager;
    RoundManager round_manager;
    Board board;
};
