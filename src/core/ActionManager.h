/**
 * @file ActionManager.h
 * @brief Movement, attack and defend rules executed against the board.
 */
#pragma once
#include <functional>
#include <vector>
#include "Hex.h"
#include "Unit.h"
#include "Board.h"

namespace core {

/**
 * @brief Pure-logic resolver for a single unit's action.
 *
 * Computes legal destinations, melee/range attack targets, performs
 * pathfinding, and applies the resulting board / unit mutations.
 * Stateless on its own; all state lives on the Board and Unit args.
 */
class ActionManager {
public:
    ActionManager() = default;

    using EnemyPredicate = std::function<bool(const models::Unit&)>;

    std::vector<models::Hex*> get_available_destinations(const models::Unit& unit,
                                                         const models::Board& board) const;
    std::vector<std::pair<models::Unit*, models::Hex*>> get_available_attacks(const models::Unit& unit,
                                                                              const models::Board& board) const;

    std::vector<const models::Hex*> find_path(const models::Unit& unit,
                                              const models::Hex& dest_hex,
                                              const models::Board& board) const;

    void move(models::Unit& unit, models::Hex& dest_hex, models::Board& board );

    bool attack(models::Unit& attacker, models::Unit& defender,
                models::Hex& attack_from_hex, models::Board& board );

    bool shoot(models::Unit& attacker, models::Unit& defender, models::Board& board );

    bool can_shoot(const models::Unit& attacker, const models::Unit& defender,
                   const EnemyPredicate& is_enemy, const models::Board& board) const;

    bool is_blocked_by_adjacent_enemy(const models::Unit& unit,
                                      const EnemyPredicate& is_enemy,
                                      const models::Board& board) const;

    static int hex_distance(const models::Unit& a, const models::Unit& b );

    void defend(models::Unit& unit );
    int calculate_damage(const models::Unit& attacker, const models::Unit& defender) const;
};

}  // namespace core
