/**
 * @file ActionGenerator.cc
 * @brief Implementation of legal-action enumeration.
 * @author Lukasz Szydlik
 */
#include "ActionGenerator.h"

#include <algorithm>
#include <array>
#include <utility>

#include "../models/Hero.h"
#include "../models/SpellRegistry.h"
#include "GameManager.h"
#include "SpellResolver.h"

namespace core {

using models::Hero;
using models::Hex;
using models::Spell;
using models::SpellRegistry;
using models::Unit;

namespace {

/**
 * @brief A unit's occupied hexes (head, plus tail for 2-hex units).
 *
 * The tail sits one column toward the unit's facing, matching how the
 * board placement and BattlePresenter::buildAttackOriginsForTarget
 * compute the second hex.
 */
std::array<std::pair<int, int>, 2> bodyHexes( int head_q, int head_r, int size, bool facing_left ) {
    std::array<std::pair<int, int>, 2> body{ { { head_q, head_r }, { head_q, head_r } } };
    if ( size == 2 ) {
        const int tail_dq = facing_left ? 1 : -1;
        body[1] = { head_q + tail_dq, head_r };
    }
    return body;
}

int hexCubeDistance( int aq, int ar, int bq, int br ) {
    const int as = -aq - ar;
    const int bs = -bq - br;
    return std::max( { std::abs( aq - bq ), std::abs( ar - br ), std::abs( as - bs ) } );
}

// True when any body hex of A is cube-distance-1 from any body hex of B.
bool bodiesAdjacent( const std::array<std::pair<int, int>, 2>& a,
                          int a_count,
                          const std::array<std::pair<int, int>, 2>& b,
                          int b_count ) {
    for ( int i = 0; i < a_count; ++i ) {
        for ( int j = 0; j < b_count; ++j ) {
            if ( hexCubeDistance( a[i].first, a[i].second, b[j].first, b[j].second ) == 1 ) {
                return true;
            }
        }
    }
    return false;
}

} // namespace

ActionGenerator::ActionGenerator( GameManager& model, SpellResolver& spell_resolver )
    : model_( model ), spellResolver_( spell_resolver ) {}

std::vector<ActionCommand> ActionGenerator::generate( Unit& active_unit ) const {
    std::vector<ActionCommand> actions;
    appendMoves( active_unit, actions );
    appendMeleeAttacks( active_unit, actions );
    appendRangedAttacks( active_unit, actions );
    appendWaitDefend( active_unit, actions );
    appendSpells( active_unit, actions );
    return actions;
}

void ActionGenerator::appendMoves( Unit& active_unit, std::vector<ActionCommand>& out ) const {
    for ( const Hex* dest : model_.getAvailableDestinations( active_unit ) ) {
        if ( dest == nullptr ) {
            continue;
        }
        ActionCommand cmd;
        cmd.type_ = ActionType::MOVE;
        cmd.destQ_ = dest->getQ( );
        cmd.destR_ = dest->getR( );
        out.push_back( cmd );
    }
}

void ActionGenerator::appendMeleeAttacks( Unit& active_unit,
                                                  std::vector<ActionCommand>& out ) const {
    // Attack in place: enemies already adjacent to the current position.
    for ( const auto& [target, hex] : model_.getAvailableAttacks( active_unit ) ) {
        if ( target == nullptr || target->getCount( ) <= 0 ) {
            continue;
        }
        ActionCommand cmd;
        cmd.type_ = ActionType::MELEE_ATTACK;
        cmd.destQ_ = active_unit.getQ( );
        cmd.destR_ = active_unit.getR( );
        cmd.target_ = target;
        out.push_back( cmd );
    }

    // Move & attack: every reachable hex from which an enemy becomes
    // adjacent. getAvailableDestinations excludes the current hex, so
    // this never duplicates the attack-in-place commands above.
    const std::vector<Hex*> destinations = model_.getAvailableDestinations( active_unit );
    const int attacker_size = active_unit.getSize( );
    const bool attacker_facing = active_unit.isFacingLeft( );

    for ( Unit* target : model_.getAllUnits( ) ) {
        if ( target == nullptr || target->getCount( ) <= 0 ) {
            continue;
        }
        if ( ! model_.areEnemies( active_unit, *target ) ) {
            continue;
        }
        const auto target_body =
            bodyHexes( target->getQ( ), target->getR( ), target->getSize( ), target->isFacingLeft( ) );
        const int target_count = ( target->getSize( ) == 2 ) ? 2 : 1;

        for ( const Hex* dest : destinations ) {
            if ( dest == nullptr ) {
                continue;
            }
            const auto attacker_body =
                bodyHexes( dest->getQ( ), dest->getR( ), attacker_size, attacker_facing );
            const int attacker_count = ( attacker_size == 2 ) ? 2 : 1;
            if ( ! bodiesAdjacent( attacker_body, attacker_count, target_body, target_count ) ) {
                continue;
            }
            ActionCommand cmd;
            cmd.type_ = ActionType::MELEE_ATTACK;
            cmd.destQ_ = dest->getQ( );
            cmd.destR_ = dest->getR( );
            cmd.target_ = target;
            out.push_back( cmd );
        }
    }
}

void ActionGenerator::appendRangedAttacks( Unit& active_unit,
                                                   std::vector<ActionCommand>& out ) const {
    if ( ! active_unit.isRanged( ) || active_unit.getAmmo( ) <= 0 ) {
        return;
    }
    for ( Unit* target : model_.getAllUnits( ) ) {
        if ( target == nullptr || target->getCount( ) <= 0 ) {
            continue;
        }
        if ( ! model_.areEnemies( active_unit, *target ) ) {
            continue;
        }
        if ( ! model_.willShoot( active_unit, *target ) ) {
            continue;
        }
        ActionCommand cmd;
        cmd.type_ = ActionType::RANGED_ATTACK;
        cmd.target_ = target;
        out.push_back( cmd );
    }
}

void ActionGenerator::appendWaitDefend( Unit& active_unit,
                                                std::vector<ActionCommand>& out ) const {
    (void) active_unit;
    if ( model_.canCurrentUnitWait( ) ) {
        ActionCommand wait_cmd;
        wait_cmd.type_ = ActionType::WAIT;
        out.push_back( wait_cmd );
    }
    ActionCommand defend_cmd;
    defend_cmd.type_ = ActionType::DEFEND;
    out.push_back( defend_cmd );
}

void ActionGenerator::appendSpells( Unit& active_unit, std::vector<ActionCommand>& out ) const {
    Hero* caster = model_.getCasterFor( active_unit );
    if ( caster == nullptr || caster->hasCastThisRound( ) ) {
        return;
    }
    const int mana = caster->getCurrentMana( );
    for ( const Spell& spell : SpellRegistry::all( ) ) {
        if ( mana < spell.manaCost_ ) {
            continue;
        }
        for ( Unit* target : model_.getAllUnits( ) ) {
            if ( target == nullptr || target->getCount( ) <= 0 ) {
                continue;
            }
            if ( ! spellResolver_.isValidTarget( spell.id_, active_unit, *target ) ) {
                continue;
            }
            ActionCommand cmd;
            cmd.type_ = ActionType::CAST_SPELL;
            cmd.spellId_ = spell.id_;
            cmd.target_ = target;
            out.push_back( cmd );
        }
    }
}

} // namespace core
