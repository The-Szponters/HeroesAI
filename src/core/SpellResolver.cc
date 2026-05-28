/**
 * @file SpellResolver.cc
 * @brief Implementation of spell validation and effect application.
 * @author Łukasz Szydlik
 */
#include "SpellResolver.h"

#include "../models/Buff.h"
#include "../models/SpellRegistry.h"

namespace core {

using models::Buff;
using models::BuffFactory;
using models::Hero;
using models::Spell;
using models::SpellAlignment;
using models::SpellId;
using models::SpellRegistry;
using models::Unit;

SpellResolver::SpellResolver( GameManager& model ) : model_( model ) {}

bool SpellResolver::isValidTarget( SpellId id,
                                          const Unit& caster_unit,
                                          const Unit& target ) const {
    if ( target.getCount( ) <= 0 ) {
        return false;
    }

    const Spell& spell = SpellRegistry::bySpellId( id );
    const bool target_is_ally = model_.areAllies( caster_unit, target );
    if ( spell.alignment_ == SpellAlignment::POSITIVE && ! target_is_ally ) {
        return false;
    }
    if ( spell.alignment_ == SpellAlignment::NEGATIVE && target_is_ally ) {
        return false;
    }
    return true;
}

SpellCastResult SpellResolver::tryCast( SpellId id,
                                                Hero& caster,
                                                Unit& target ) {
    SpellCastResult result;

    if ( caster.hasCastThisRound( ) ) {
        result.message = "Hero already cast a spell this round.";
        return result;
    }

    const Spell& spell = SpellRegistry::bySpellId( id );
    if ( caster.getCurrentMana( ) < spell.manaCost_ ) {
        result.message = "Not enough mana.";
        return result;
    }

    if ( target.getCount( ) <= 0 ) {
        result.message = "Target is gone.";
        return result;
    }

    const int power = caster.getPower( );
    const int duration = SpellRegistry::durationFor( power );

    switch ( id ) {
    case SpellId::MAGIC_ARROW:
    case SpellId::LIGHTNING_BOLT:
    case SpellId::ICE_BOLT: {
        const int damage = SpellRegistry::damageFor( id, power );
        target.takeDamage( damage );
        // Spell damage also breaks Blind (spec: any damage dispels it).
        if ( target.getCount( ) > 0 && target.hasBuff( models::BuffType::BLIND ) ) {
            target.removeBuff( models::BuffType::BLIND );
            target.setNextRetaliationHalfAttack( true );
        }
        // Mirror the melee/range death-cleanup path so dead targets
        // free their hex(es) instead of lingering as ghost blockers.
        model_.notifyUnitMaybeDied( target );
        result.message = spell.name_ + " deals " + std::to_string( damage ) + " damage.";
        break;
    }
    case SpellId::CURE: {
        target.removeNegativeBuffs( );
        const int heal_amount = SpellRegistry::healAmountFor( power );
        target.heal( heal_amount );
        result.message = "Cure heals " + std::to_string( heal_amount ) + " HP.";
        break;
    }
    case SpellId::CURSE:
        target.applyBuff( BuffFactory::createCurseBuff( duration ) );
        result.message = "Curse applied.";
        break;
    case SpellId::BLOODLUST:
        target.applyBuff( BuffFactory::createBloodlustBuff( duration ) );
        result.message = "Bloodlust applied.";
        break;
    case SpellId::BLIND:
        target.applyBuff( BuffFactory::createBlindBuff( duration ) );
        result.message = "Blind applied.";
        break;
    case SpellId::HASTE:
        target.applyBuff( BuffFactory::createHasteBuff( duration ) );
        result.message = "Haste applied.";
        break;
    case SpellId::BLESS:
        target.applyBuff( BuffFactory::createBlessBuff( duration ) );
        result.message = "Bless applied.";
        break;
    case SpellId::STONE_SKIN:
        target.applyBuff( BuffFactory::createStoneSkinBuff( duration ) );
        result.message = "Stone Skin applied.";
        break;
    case SpellId::SLOW:
        target.applyBuff( BuffFactory::createSlowBuff( duration ) );
        result.message = "Slow applied.";
        break;
    case SpellId::SHIELD:
        target.applyBuff( BuffFactory::createShieldBuff( duration ) );
        result.message = "Shield applied.";
        break;
    case SpellId::DISRUPTING_RAY:
        target.applyBuff( BuffFactory::createDisruptingRayBuff( ) );
        result.message = "Disrupting Ray applied.";
        break;
    }

    caster.spendMana( spell.manaCost_ );
    caster.markCastThisRound( );
    result.success = true;
    return result;
}

} // namespace core
