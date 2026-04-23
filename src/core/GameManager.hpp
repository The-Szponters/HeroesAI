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

    // Actions

    void move(Unit& unit, Hex& dest_hex);
    void attack(Unit& attacker, Unit& defender, Hex& attack_from_hex);
    void wait(Unit& unit);
    void defend(Unit& unit);

    const Hero& get_blue_hero() const { return blue_hero; }
    const Hero& get_red_hero() const { return red_hero; }

private:
    void remove_dead_unit(Unit& unit);

    Hero blue_hero;
    Hero red_hero;
    std::vector<Unit*> all_units_in_battle;

    // Board and units
    ActionManager action_manager;
    RoundManager round_manager;
    Board board;
};
