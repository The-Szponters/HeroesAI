#pragma once
#include <functional>
#include <vector>
#include "hex.hpp"
#include "unit.hpp"
#include "board.hpp"

class ActionManager {
public:
    ActionManager() = default;

    using EnemyPredicate = std::function<bool(const Unit&)>;

    std::vector<Hex*> get_available_destinations(const Unit& unit, const Board& board) const;
    std::vector<std::pair<Unit*, Hex*>> get_available_attacks(const Unit& unit, const Board& board) const;

    std::vector<const Hex*> find_path(const Unit& unit, const Hex& dest_hex, const Board& board) const;

    void move(Unit& unit, Hex& dest_hex, Board& board);

    bool attack(Unit& attacker, Unit& defender, Hex& attack_from_hex, Board& board);

    bool shoot(Unit& attacker, Unit& defender, Board& board);

    bool can_shoot(const Unit& attacker, const Unit& defender,
                   const EnemyPredicate& is_enemy, const Board& board) const;

    bool is_blocked_by_adjacent_enemy(const Unit& unit,
                                      const EnemyPredicate& is_enemy,
                                      const Board& board) const;

    static int hex_distance(const Unit& a, const Unit& b);

    void defend(Unit& unit);
    int calculate_damage(const Unit& attacker, const Unit& defender) const;
};
