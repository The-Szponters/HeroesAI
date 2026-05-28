/**
 * @file Buff.h
 * @brief Temporary stat-modifying effects applied to units.
 * @author Dominik Śledziewski
 */
#pragma once
#include <cmath>
#include <functional>

namespace models {

/**
 * @brief Identifies the kind of buff (so duplicates can be merged).
 */
enum class BuffType {
    DEFEND,
    SLOW,
    BLIND,
    CURSE,
    BLOODLUST,
    HASTE,
    BLESS,
    STONE_SKIN,
    SHIELD,
    DISRUPTING_RAY
};

/**
 * @brief Buff alignment used by Cure to selectively dispel negative effects.
 */
enum class BuffAlignment {
    POSITIVE,
    NEGATIVE
};

/**
 * @brief Value object describing a temporary stat modifier.
 *
 * `duration_` is measured in rounds. The sentinel value `-1` means
 * the buff lasts until the end of the battle (used by Disrupting Ray).
 * `stackable_ == true` lets `Unit::applyBuff` push duplicates instead
 * of replacing the existing entry of the same type.
 *
 * Bless / Curse can't be expressed by independent min / max lambdas
 * (each lambda only sees its own stat), so they use the post-pass
 * `forceMaxDamage_` / `forceMinDamage_` flags evaluated by
 * `Unit::recalculateStats` after every per-stat lambda has run.
 */
struct Buff {
    BuffType type_;
    int duration_ = 0;
    bool stackable_ = false;
    BuffAlignment alignment_ = BuffAlignment::POSITIVE;

    std::function<int( int )> modifyAttack_ = []( int x ) { return x; };
    std::function<int( int )> modifyDefense_ = []( int x ) { return x; };
    std::function<int( int )> modifySpeed_ = []( int x ) { return x; };
    std::function<int( int )> modifyDamageMin_ = []( int x ) { return x; };
    std::function<int( int )> modifyDamageMax_ = []( int x ) { return x; };

    // Multiplier applied to incoming melee damage (Shield). Defaults to
    // identity so existing buffs leave melee damage untouched.
    std::function<float( float )> modifyIncomingMeleeMult_ = []( float x ) { return x; };

    // Post-pass damage clamps -- Bless squashes min up to max, Curse
    // squashes max down to min, evaluated after all per-stat lambdas.
    bool forceMaxDamage_ = false;
    bool forceMinDamage_ = false;
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
        b.alignment_ = BuffAlignment::POSITIVE;
        b.modifyDefense_ = []( int def ) { return def + 5; };
        return b;
    }

    static Buff createSlowBuff( int duration = 3 ) {
        Buff b;
        b.type_ = BuffType::SLOW;
        b.duration_ = duration;
        b.alignment_ = BuffAlignment::NEGATIVE;
        // Speed * 0.75, rounded up (spec: 5 -> 4).
        b.modifySpeed_ = []( int spd ) {
            return static_cast<int>( std::ceil( static_cast<float>( spd ) * 0.75f ) );
        };
        return b;
    }

    static Buff createBlindBuff( int duration = 3 ) {
        Buff b;
        b.type_ = BuffType::BLIND;
        b.duration_ = duration;
        b.alignment_ = BuffAlignment::NEGATIVE;
        // Speed -> 0 keeps the unit out of the turn queue (RoundManager
        // skips speed-0 actors). Attack stat stays unchanged; the 50%
        // counter-attack penalty is applied to the DAMAGE in
        // ActionManager via the nextRetaliationHalfAttack_ flag.
        b.modifySpeed_ = []( int ) { return 0; };
        return b;
    }

    static Buff createCurseBuff( int duration ) {
        Buff b;
        b.type_ = BuffType::CURSE;
        b.duration_ = duration;
        b.alignment_ = BuffAlignment::NEGATIVE;
        b.forceMinDamage_ = true;
        return b;
    }

    static Buff createBloodlustBuff( int duration ) {
        Buff b;
        b.type_ = BuffType::BLOODLUST;
        b.duration_ = duration;
        b.alignment_ = BuffAlignment::POSITIVE;
        b.modifyAttack_ = []( int atk ) { return atk + 3; };
        return b;
    }

    static Buff createHasteBuff( int duration ) {
        Buff b;
        b.type_ = BuffType::HASTE;
        b.duration_ = duration;
        b.alignment_ = BuffAlignment::POSITIVE;
        b.modifySpeed_ = []( int spd ) { return spd + 3; };
        return b;
    }

    static Buff createBlessBuff( int duration ) {
        Buff b;
        b.type_ = BuffType::BLESS;
        b.duration_ = duration;
        b.alignment_ = BuffAlignment::POSITIVE;
        b.forceMaxDamage_ = true;
        return b;
    }

    static Buff createStoneSkinBuff( int duration ) {
        Buff b;
        b.type_ = BuffType::STONE_SKIN;
        b.duration_ = duration;
        b.alignment_ = BuffAlignment::POSITIVE;
        b.modifyDefense_ = []( int def ) { return def + 3; };
        return b;
    }

    static Buff createShieldBuff( int duration ) {
        Buff b;
        b.type_ = BuffType::SHIELD;
        b.duration_ = duration;
        b.alignment_ = BuffAlignment::POSITIVE;
        b.modifyIncomingMeleeMult_ = []( float m ) { return m * 0.85f; };
        return b;
    }

    static Buff createDisruptingRayBuff( ) {
        Buff b;
        b.type_ = BuffType::DISRUPTING_RAY;
        b.duration_ = -1;            // infinite (until end of battle)
        b.stackable_ = true;         // each cast adds another -3
        b.alignment_ = BuffAlignment::NEGATIVE;
        b.modifyDefense_ = []( int def ) { return def - 3; };
        return b;
    }
};

} // namespace models
