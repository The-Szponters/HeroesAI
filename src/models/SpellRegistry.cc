/**
 * @file SpellRegistry.cc
 * @brief Implementation of the static spell catalogue.
 * @author Łukasz Szydlik
 */
#include "SpellRegistry.h"

#include <stdexcept>

namespace models {

namespace {

// Atlas geometry mirrors the creatures-portraits pattern: 58x58 cells
// with a 5 px gap. 4 cols × 4 rows fits the 13 spells with a few empty
// trailing slots -- the picker UI just ignores them.
constexpr int K_CELL_W = 58;
constexpr int K_CELL_H = 58;
constexpr int K_CELL_STRIDE = 63;
constexpr int K_ATLAS_ORIGIN_X = 8;
constexpr int K_ATLAS_ORIGIN_Y = 8;

PortraitRect iconAt( int col, int row ) {
    PortraitRect r;
    r.x_ = K_ATLAS_ORIGIN_X + col * K_CELL_STRIDE;
    r.y_ = K_ATLAS_ORIGIN_Y + row * K_CELL_STRIDE;
    r.w_ = K_CELL_W;
    r.h_ = K_CELL_H;
    return r;
}

Spell make( SpellId id,
                std::string name,
                int level,
                SpellSchool school,
                SpellAlignment alignment,
                int mana_cost,
                bool instant_effect,
                std::string animation_asset,
                int icon_col,
                int icon_row ) {
    Spell s;
    s.id_ = id;
    s.name_ = std::move( name );
    s.level_ = level;
    s.school_ = school;
    s.alignment_ = alignment;
    s.manaCost_ = mana_cost;
    s.instantEffect_ = instant_effect;
    s.animationAsset_ = std::move( animation_asset );
    s.iconRect_ = iconAt( icon_col, icon_row );
    return s;
}

const std::vector<Spell>& buildCatalogue( ) {
    static const std::vector<Spell> catalogue = {
        // Damage spells (instant).
        make( SpellId::MAGIC_ARROW,    "Magic Arrow",    1, SpellSchool::AIR,
              SpellAlignment::NEGATIVE,  5, true,  "",            0, 0 ),
        make( SpellId::LIGHTNING_BOLT, "Lightning Bolt", 2, SpellSchool::AIR,
              SpellAlignment::NEGATIVE, 10, true,  "",            1, 0 ),
        make( SpellId::ICE_BOLT,       "Ice Bolt",       2, SpellSchool::WATER,
              SpellAlignment::NEGATIVE,  8, true,  "",            2, 0 ),
        // Cure (instant).
        make( SpellId::CURE,           "Cure",           1, SpellSchool::WATER,
              SpellAlignment::POSITIVE,  6, true,  "",            3, 0 ),
        // Buffs / debuffs (duration = power).
        make( SpellId::CURSE,          "Curse",          1, SpellSchool::FIRE,
              SpellAlignment::NEGATIVE,  6, false, "",            0, 1 ),
        make( SpellId::BLOODLUST,      "Bloodlust",      1, SpellSchool::FIRE,
              SpellAlignment::POSITIVE,  5, false, "",            1, 1 ),
        make( SpellId::BLIND,          "Blind",          2, SpellSchool::FIRE,
              SpellAlignment::NEGATIVE, 10, false, "",            2, 1 ),
        make( SpellId::HASTE,          "Haste",          1, SpellSchool::AIR,
              SpellAlignment::POSITIVE,  6, false, "",            3, 1 ),
        make( SpellId::BLESS,          "Bless",          1, SpellSchool::WATER,
              SpellAlignment::POSITIVE,  5, false, "",            0, 2 ),
        make( SpellId::STONE_SKIN,     "Stone Skin",     1, SpellSchool::EARTH,
              SpellAlignment::POSITIVE,  5, false, "",            1, 2 ),
        make( SpellId::SLOW,           "Slow",           1, SpellSchool::EARTH,
              SpellAlignment::NEGATIVE,  6, false, "",            2, 2 ),
        make( SpellId::SHIELD,         "Shield",         1, SpellSchool::EARTH,
              SpellAlignment::POSITIVE,  5, false, "",            3, 2 ),
        // Infinite-duration debuff (stackable).
        make( SpellId::DISRUPTING_RAY, "Disrupting Ray", 2, SpellSchool::AIR,
              SpellAlignment::NEGATIVE, 10, false, "",            0, 3 )
    };
    return catalogue;
}

} // namespace

const std::vector<Spell>& SpellRegistry::all( ) {
    return buildCatalogue( );
}

const Spell& SpellRegistry::bySpellId( SpellId id ) {
    for ( const Spell& s : all( ) ) {
        if ( s.id_ == id ) {
            return s;
        }
    }
    throw std::out_of_range( "SpellRegistry: unknown spell id" );
}

int SpellRegistry::damageFor( SpellId id, int spell_power ) {
    switch ( id ) {
    case SpellId::MAGIC_ARROW:    return spell_power * 10 + 10;
    case SpellId::LIGHTNING_BOLT: return spell_power * 25 + 10;
    case SpellId::ICE_BOLT:       return spell_power * 20 + 10;
    default:                       return 0;
    }
}

int SpellRegistry::healAmountFor( int spell_power ) {
    return spell_power * 5 + 10;
}

int SpellRegistry::durationFor( int spell_power ) {
    return spell_power;
}

} // namespace models
