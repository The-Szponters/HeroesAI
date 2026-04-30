/**
 * @file UnitId.h
 * @brief Strongly-typed identifiers for every recruitable unit kind.
 *
 * Used by the unit factory to look up data-driven unit definitions
 * (stats, ranged ammo, animation files) loaded from JSON at startup.
 * @author Dominik Śledziewski
 */
#pragma once

namespace models {

/**
 * @brief Enumerates every concrete unit kind known to the factory.
 *
 * The order of the enumerators is also used as a dense index into the
 * unit-name lookup table inside UnitFactory, so do not reorder these
 * entries without updating the corresponding name table.
 */
enum class UnitID {
    PIKEMAN,
    HALBERDIER,
    ARCHER,
    MARKSMAN,
    GRIFFIN,
    ROYAL_GRIFFIN,
    SWORDSMAN,
    CRUSADER,
    MONK,
    ZEALOT,
    CAVALIER,
    CHAMPION,
    ANGEL,
    ARCHANGEL,

    IMP,
    FAMILIAR,
    GOG,
    MAGOG,
    HELL_HOUND,
    CERBERUS,
    DEMON,
    HORNED_DEMON,
    PIT_FIEND,
    PIT_LORD,
    EFREET,
    EFREET_SULTAN,
    DEVIL,
    ARCH_DEVIL,

    SKELETON,
    SKELETON_WARRIOR,
    WALKING_DEAD,
    ZOMBIE,
    WIGHT,
    WRAITH,
    VAMPIRE,
    VAMPIRE_LORD,
    LICH,
    POWER_LICH,
    BLACK_KNIGHT,
    DREAD_KNIGHT,
    BONE_DRAGON,
    GHOST_DRAGON
};

} // namespace models
