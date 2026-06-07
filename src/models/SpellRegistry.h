/**
 * @file SpellRegistry.h
 * @brief Static catalogue of every implemented spell.
 * @author Lukasz Szydlik
 */
#pragma once

#include <vector>

#include "Spell.h"

namespace models {

/**
 * @brief Read-only catalogue of all spells available to heroes.
 *
 * The data is hard-coded (spec-driven) and accessed via static
 * helpers. Damage / heal / duration formulas live here too so the
 * presenter and resolver share one source of truth.
 */
class SpellRegistry {
public:
    static const std::vector<Spell>& all( );
    static const Spell& bySpellId( SpellId id );

    // (Spell Power * 10) + 10 etc. -- see HeroesAI spec.
    static int damageFor( SpellId id, int spell_power );
    static int healAmountFor( int spell_power );
    static int durationFor( int spell_power );
};

} // namespace models
