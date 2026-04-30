/**
 * @file GameManager.h
 * @brief High-level battle controller -- the model the presenter talks to.
 */
#pragma once
#include <unordered_set>
#include <vector>
#include "Hex.h"
#include "Unit.h"
#include "Board.h"
#include "../models/Hero.h"
#include "ActionManager.h"
#include "RoundManager.h"

namespace core {

/**
 * @brief Aggregates board, heroes, round scheduling and action resolution
 *        into a single facade representing one ongoing battle.
 *
 * The presenter queries this class for the current unit, legal moves
 * and attacks, then forwards player commands (move, attack, wait,
 * defend). It also rolls morale and tracks dead units.
 */
class GameManager {
public:
    GameManager() : round_manager(all_units_in_battle ){}
    GameManager(models::Hero blue_hero, models::Hero red_hero );

    models::Unit* get_current_unit( );

    std::vector<models::Unit*> get_units_left_in_round() const;
    std::vector<models::Unit*> get_unit_queue_in_round() const;

    std::vector<models::Hex*> get_available_destinations(const models::Unit& unit) const;
    std::vector<std::pair<models::Unit*, models::Hex*>> get_available_attacks(const models::Unit& unit) const;

    std::vector<const models::Hex*> find_path(const models::Unit& unit, const models::Hex& dest_hex) const;

    int get_round_number() const { return round_number; }
    std::vector<models::Unit*> peek_next_round_order() const;

    models::Board& get_board( ){ return board; }
    const models::Board& get_board() const { return board; }
    bool are_allies(const models::Unit& first, const models::Unit& second) const;
    bool are_enemies(const models::Unit& first, const models::Unit& second) const;
    bool can_attack(const models::Unit& attacker, const models::Hex& target_hex) const;
    bool can_move(const models::Unit& unit, const models::Hex& dest_hex) const;

    bool will_shoot(const models::Unit& attacker, const models::Unit& defender) const;

    bool active_unit_has_morale_bonus() const { return morale_triggered_this_turn; }

    void next_turn( );

    void move(models::Unit& unit, models::Hex& dest_hex );
    void attack(models::Unit& attacker, models::Unit& defender, models::Hex& attack_from_hex );
    void wait(models::Unit& unit );
    void defend(models::Unit& unit );

    const models::Hero& get_blue_hero() const { return blue_hero; }
    const models::Hero& get_red_hero() const { return red_hero; }

private:
    void remove_dead_unit(models::Unit& unit );
    bool hero_contains_unit(const models::Hero& hero, const models::Unit& unit) const;

    void consume_turn_or_burn_morale_bonus( );

    void roll_morale_for_active(models::Unit* unit );

    models::Hero blue_hero;
    models::Hero red_hero;
    std::vector<models::Unit*> all_units_in_battle;
    int round_number = 1;

    models::Unit* last_morale_rolled_unit = nullptr;
    bool  morale_triggered_this_turn = false;
    int   morale_round_tracked = 1;
    std::unordered_set<models::Unit*> morale_rolled_units_this_round;

    ActionManager action_manager;
    RoundManager round_manager;
    models::Board board;
};

}  // namespace core
