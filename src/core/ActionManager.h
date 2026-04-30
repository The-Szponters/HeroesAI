/**
 * @file ActionManager.h
 * @brief Movement, attack and defend rules executed against the board.
 * @author Łukasz Szydlik
 */
#pragma once
#include <functional>
#include <vector>

#include "Board.h"
#include "Hex.h"
#include "Unit.h"

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
    ActionManager( ) = default;

    using EnemyPredicate = std::function<bool( const models::Unit& )>;

    /**
     * @brief Computes all hex destinations reachable by @p unit this turn.
     * @param unit  The moving unit.
     * @param board Current battlefield state.
     * @return Pointers to reachable Hex objects (excluding the unit's current hex).
     */
    std::vector<models::Hex*> getAvailableDestinations( const models::Unit& unit,
                                                          const models::Board& board ) const;

    /**
     * @brief Returns all enemy units that @p unit can melee-attack from its current position.
     * @param unit  The attacking unit.
     * @param board Current battlefield state.
     * @return Pairs of (target unit pointer, hex pointer) for each adjacent attackable enemy.
     */
    std::vector<std::pair<models::Unit*, models::Hex*>>
    getAvailableAttacks( const models::Unit& unit, const models::Board& board ) const;

    /**
     * @brief Finds the shortest walkable path from @p unit to @p dest_hex via BFS.
     * @param unit     The unit to move.
     * @param dest_hex Target destination hex.
     * @param board    Current battlefield state.
     * @return Ordered list of hex pointers from start to destination, or empty if unreachable.
     */
    std::vector<const models::Hex*> findPath( const models::Unit& unit,
                                               const models::Hex& dest_hex,
                                               const models::Board& board ) const;

    /**
     * @brief Moves @p unit to @p dest_hex, updating both the unit's position and the board.
     * @param unit     Unit to move.
     * @param dest_hex Destination hex.
     * @param board    Battlefield to mutate.
     */
    void move( models::Unit& unit, models::Hex& dest_hex, models::Board& board );

    /**
     * @brief Executes a melee attack, including optional repositioning and retaliation.
     * @param attacker        Attacking unit.
     * @param defender        Defending unit.
     * @param attack_from_hex Hex the attacker stands on while striking.
     * @param board           Battlefield to mutate.
     * @return true if the defender was destroyed.
     */
    bool attack( models::Unit& attacker,
                 models::Unit& defender,
                 models::Hex& attack_from_hex,
                 models::Board& board );

    /**
     * @brief Executes a ranged attack, consuming one ammo and dealing projectile damage.
     * @param attacker Unit performing the ranged attack.
     * @param defender Unit receiving the damage.
     * @param board    Current battlefield state.
     * @return true if the defender was destroyed.
     */
    bool shoot( models::Unit& attacker, models::Unit& defender, models::Board& board );

    /**
     * @brief Checks whether @p attacker is able to fire a ranged shot at @p defender.
     * @param attacker  The potential shooter.
     * @param defender  The intended target.
     * @param is_enemy  Predicate identifying enemy units (used to detect blocking enemies).
     * @param board     Current battlefield state.
     * @return true if the shot is legal.
     */
    bool canShoot( const models::Unit& attacker,
                    const models::Unit& defender,
                    const EnemyPredicate& is_enemy,
                    const models::Board& board ) const;

    /**
     * @brief Determines whether adjacent enemy units prevent @p unit from shooting.
     * @param unit     The unit to check.
     * @param is_enemy Predicate identifying enemy units.
     * @param board    Current battlefield state.
     * @return true if at least one adjacent enemy is present.
     */
    bool isBlockedByAdjacentEnemy( const models::Unit& unit,
                                       const EnemyPredicate& is_enemy,
                                       const models::Board& board ) const;

    /**
     * @brief Returns the minimum hex distance between any body hex of @p a and any of @p b.
     * @param a First unit.
     * @param b Second unit.
     * @return Cube-coordinate distance.
     */
    static int hexDistance( const models::Unit& a, const models::Unit& b );

    /**
     * @brief Applies a DEFEND buff to @p unit, raising its defense for one turn.
     * @param unit Unit that chose the defend action.
     */
    void defend( models::Unit& unit );

    /**
     * @brief Calculates the total damage @p attacker deals to @p defender.
     * @param attacker Attacking unit.
     * @param defender Defending unit.
     * @return Integer damage value after the attack/defense modifier is applied.
     */
    int calculateDamage( const models::Unit& attacker, const models::Unit& defender ) const;
};

} // namespace core
