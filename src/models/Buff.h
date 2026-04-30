/**
 * @file Buff.h
 * @brief Temporary stat-modifying effects applied to units.
 */
#pragma once
#include <functional>

namespace models {

/**
 * @brief Identifies the kind of buff (so duplicates can be merged).
 */
enum class BuffType { DEFEND, SLOW, BLIND };

/**
 * @brief Value object describing a temporary stat modifier.
 */
struct Buff {
    BuffType type;
    int duration;

    std::function<int( int )> modify_attack = []( int x ) { return x; };
    std::function<int( int )> modify_defense = []( int x ) { return x; };
    std::function<int( int )> modify_speed = []( int x ) { return x; };
    std::function<int( int )> modify_damage_min = []( int x ) { return x; };
    std::function<int( int )> modify_damage_max = []( int x ) { return x; };
};

/**
 * @brief Factory of canonical, ready-to-apply Buff instances.
 *
 * Centralises buff parameters (duration, modifier lambdas) so that
 * gameplay code never constructs Buff structs directly.
 */
class BuffFactory {
public:
    static Buff create_defend_buff( ) {
        Buff b;
        b.type = BuffType::DEFEND;
        b.duration = 1;
        b.modify_defense = []( int def ) { return def + 5; };
        return b;
    }

    static Buff create_slow_buff( ) {
        Buff b;
        b.type = BuffType::SLOW;
        b.duration = 3;
        b.modify_speed = []( int spd ) { return spd / 2; };
        return b;
    }

    static Buff create_blind_buff( ) {
        Buff b;
        b.type = BuffType::BLIND;
        b.duration = 3;
        b.modify_speed = []( int spd ) { return 0; };
        return b;
    }
};

} // namespace models
