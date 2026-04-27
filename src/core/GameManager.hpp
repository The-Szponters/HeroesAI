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
    // True iff `attacker`'s next attack against `defender` will resolve as a
    // ranged shot (vs a melee swing).  Presenter uses this to choose between
    // the projectile-based and melee-based visual event chains.
    bool will_shoot(const Unit& attacker, const Unit& defender) const;

    // Morale state for the *currently active* unit.  Rolled once per real
    // turn (re-asserted by get_current_unit() each time a different unit
    // becomes active) and burned by the unit's first action, so the bonus
    // delivers exactly one extra action — never two.
    bool active_unit_has_morale_bonus() const { return morale_triggered_this_turn; }

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

    // Advance the round queue UNLESS the active unit's morale just gave it a
    // free extra action — in which case clear the flag and stay on the same
    // unit.  Called from move/attack/defend; wait() always advances and
    // burns any pending bonus since the unit declined to use it.
    void consume_turn_or_burn_morale_bonus();

    // Roll a 10% chance (HoMM3's +2 morale tier) for `unit`; sets
    // `morale_triggered_this_turn` and is a no-op if `unit` already had a
    // roll this turn.
    void roll_morale_for_active(Unit* unit);

    Hero blue_hero;
    Hero red_hero;
    std::vector<Unit*> all_units_in_battle;
    int round_number = 1;

    // Morale state — every army is hardcoded to +2 morale for now (Phase 5
    // spec).  `last_morale_rolled_unit` deduplicates rolls when the
    // presenter re-queries the active unit several times before any action.
    Unit* last_morale_rolled_unit = nullptr;
    bool morale_triggered_this_turn = false;

    // Board and units
    ActionManager action_manager;
    RoundManager round_manager;
    Board board;
};
