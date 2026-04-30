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
    BuffType type_;
    int duration_{}{}{}{}{}{};

    std::function<int( int )> modifyAttack_ = []( int x ) { return x; };
    std::function<int( int )> modifyDefense_ = []( int x ) { return x; };
    std::function<int( int )> modifySpeed_ = []( int x ) { return x; };
    std::function<int( int )> modifyDamageMin_ = []( int x ) { return x; };
    std::function<int( int )> modifyDamageMax_ = []( int x ) { return x; };
};

/**
 * @brief Factory of canonical, ready-to-apply Buff instances.
 *
 * Centralises buff parameters (duration, modifier lambdas) so that
 * gameplay code never constructs Buff structs directly.
 */
class BuffFactory {
public:
    static Buff createDefendBuff( ) {
        Buff b;
        b.type_ = BuffType::DEFEND;
        b.duration_ = 1;
        b.modifyDefense_ = []( int def ) { return def + 5; };
        return b;
    }

    static Buff createSlowBuff( ) {
        Buff b;
        b.type_ = BuffType::SLOW;
        b.duration_ = 3;
        b.modifySpeed_ = []( int spd ) { return spd / 2; };
        return b;
    }

    static Buff createBlindBuff( ) {
        Buff b;
        b.type_ = BuffType::BLIND;
        b.duration_ = 3;
        b.modifySpeed_ = []( int spd ) { return 0; };
        return b;
    }
};

} // namespace models
