#pragma once
#include <vector>
#include "hex.hpp"
#include "unit.hpp"
#include "board.hpp"

class ActionManager {
public:
    ActionManager() = default;

    std::vector<Hex*> get_available_destinations(const Unit& unit, const Board& board) const;
    std::vector<std::pair<Unit*, Hex*>> get_available_attacks(const Unit& unit, const Board& board) const;

    // Returns the shortest hex chain from `unit`'s current position to
    // `dest_hex`, inclusive of both endpoints.  Empty if unreachable.
    // The chain is what the View walks for per-segment facing updates.
    std::vector<const Hex*> find_path(const Unit& unit, const Hex& dest_hex, const Board& board) const;

    void move(Unit& unit, Hex& dest_hex, Board& board);
    bool attack(Unit& attacker, Unit& defender, Hex& attack_from_hex, Board& board);
    void defend(Unit& unit);
    int calculate_damage(const Unit& attacker, const Unit& defender) const;
};
