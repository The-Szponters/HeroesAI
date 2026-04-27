#pragma once
#include <functional>
#include <vector>
#include "hex.hpp"
#include "unit.hpp"
#include "board.hpp"

class ActionManager {
public:
    ActionManager() = default;

    // Predicate the higher-level GameManager hands down so ActionManager can
    // distinguish friend from foe without owning a hero registry.  Used for
    // ranged-blocking checks (only ENEMY adjacency disables shooting).
    using EnemyPredicate = std::function<bool(const Unit&)>;

    std::vector<Hex*> get_available_destinations(const Unit& unit, const Board& board) const;
    std::vector<std::pair<Unit*, Hex*>> get_available_attacks(const Unit& unit, const Board& board) const;

    // Returns the shortest hex chain from `unit`'s current position to
    // `dest_hex`, inclusive of both endpoints.  Empty if unreachable.
    // The chain is what the View walks for per-segment facing updates.
    std::vector<const Hex*> find_path(const Unit& unit, const Hex& dest_hex, const Board& board) const;

    void move(Unit& unit, Hex& dest_hex, Board& board);

    // Melee attack (handles charge-into-melee for ranged units too).  Returns
    // true iff the defender died from the initial strike.  When `attacker`
    // is_ranged() the damage is halved per HoMM3 melee-penalty.
    bool attack(Unit& attacker, Unit& defender, Hex& attack_from_hex, Board& board);

    // Ranged attack — no movement, no retaliation, ammo decremented.  Damage
    // is halved when cube-distance(attacker, defender) > 10.  Caller must
    // have verified can_shoot() first.  Returns true iff the defender died.
    bool shoot(Unit& attacker, Unit& defender, Board& board);

    // True iff `attacker` can shoot `defender` right now: is_ranged, ammo > 0,
    // not currently blocked by an adjacent enemy, and not already adjacent
    // (HoMM3 forbids shooting a unit you're already in melee with — the
    // attempt becomes a 50%-damage melee instead, handled by attack()).
    bool can_shoot(const Unit& attacker, const Unit& defender,
                   const EnemyPredicate& is_enemy, const Board& board) const;

    bool is_blocked_by_adjacent_enemy(const Unit& unit,
                                      const EnemyPredicate& is_enemy,
                                      const Board& board) const;

    // Cube-distance between any two units' nearest body hexes.
    static int hex_distance(const Unit& a, const Unit& b);

    void defend(Unit& unit);
    int calculate_damage(const Unit& attacker, const Unit& defender) const;
};
