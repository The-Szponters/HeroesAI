/**
 * @file Spell.h
 * @brief Data structures describing a castable spell.
 * @author Dominik Sledziewski
 */
#pragma once

#include <string>

#include "Unit.h"   // for models::PortraitRect

namespace models {

/**
 * @brief Identifier for every implemented spell.
 *
 * Used as a dense index into SpellRegistry; the order is preserved
 * across the codebase (spellbook UI sorts on it).
 */
enum class SpellId {
    MAGIC_ARROW,
    LIGHTNING_BOLT,
    ICE_BOLT,
    CURE,
    CURSE,
    BLOODLUST,
    BLIND,
    HASTE,
    BLESS,
    STONE_SKIN,
    SLOW,
    SHIELD,
    DISRUPTING_RAY
};

/**
 * @brief Magic school a spell belongs to.
 */
enum class SpellSchool {
    FIRE,
    WATER,
    EARTH,
    AIR
};

/**
 * @brief Spell alignment -- decides which side the spell may target
 *        and (for Cure) which buffs count as "negative".
 */
enum class SpellAlignment {
    POSITIVE,
    NEGATIVE
};

/**
 * @brief Immutable metadata describing one spell.
 *
 * The `iconRect_` is a sub-rectangle inside the shared spell-icon
 * atlas PNG (`assets/ui/spellbook/spell_icons.png`); `animationAsset_`
 * names the HoMM3 .def file played on the target hex when the spell
 * is cast (an empty string falls back to a coloured flash).
 */
struct Spell {
    SpellId id_ = SpellId::MAGIC_ARROW;
    std::string name_;
    int level_ = 1;
    SpellSchool school_ = SpellSchool::AIR;
    SpellAlignment alignment_ = SpellAlignment::NEGATIVE;
    int manaCost_ = 0;
    bool instantEffect_ = true;
    std::string animationAsset_;
    PortraitRect iconRect_;        // PNG atlas fallback (kept for compat)
    int defIconFrame_ = -1;        // preferred -- frame index in spells_icons.def
    std::string description_;      // shown when right-clicking the spell in the spellbook
};

} // namespace models
