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

    void move(Unit& unit, Hex& dest_hex, Board& board);
    void attack(Unit& attacker, Unit& defender, Hex& attack_from_hex, Board& board);
    void defend(Unit& unit);
};
