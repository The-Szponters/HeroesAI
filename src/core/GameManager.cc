/**
 * @file GameManager.cc
 * @brief Implementation of the battle facade and morale rolls.
 * @author Dominik Śledziewski
 */
#include <algorithm>
#include <memory>
#include <random>
#include <tuple>
#include <unordered_map>

#include "GameManager.h"

namespace core {

using models::Board;
using models::Hero;
using models::Hex;
using models::Unit;

namespace {

std::tuple<int, int, int> tailDelta( const Unit& u ) {
    if ( u.isFacingLeft( ) ) {
        return { 1, 0, -1 };
}
    return { -1, 0, 1 };
}

void placeUnitOnBoard( Board& board, const std::shared_ptr<Unit>& unit ) {
    if ( ! unit ) {
        return;
}
    try {
        board.getHex( unit->getQ( ), unit->getR( ), unit->getS( ) ).setUnit( unit );
    } catch ( std::out_of_range& ) {}
    if ( unit->getSize( ) == 2 ) {
        auto [dq, dr, ds] = tailDelta( *unit );
        try {
            board.getHex( unit->getQ( ) + dq, unit->getR( ) + dr, unit->getS( ) + ds )
                .setUnit( unit );
        } catch ( std::out_of_range& ) {}
    }
}

// Deep-copies a hero: stats / mana / cast flag come from the value copy,
// while the army is rebuilt from polymorphic Unit clones so the copy
// shares no unit objects with the source. Every old->new unit pointer is
// recorded in @p remap so the board and round queues can be rewired.
models::Hero cloneHeroDeep( const models::Hero& src,
                                   std::unordered_map<const Unit*, Unit*>& remap ) {
    models::Hero dst = src; // copies stats, mana, cast flag, (shallow) army
    while ( ! dst.getArmy( ).getUnits( ).empty( ) ) {
        dst.getArmy( ).removeUnit( 0 );
    }
    for ( const std::shared_ptr<Unit>& orig : src.getArmy( ).getUnits( ) ) {
        if ( ! orig ) {
            continue;
        }
        std::shared_ptr<Unit> cloned = orig->clone( );
        remap[orig.get( )] = cloned.get( );
        dst.getArmy( ).addUnit( cloned );
    }
    return dst;
}

} // namespace

GameManager::GameManager( const Hero& blue_hero, const Hero& red_hero )
    : blueHero_( blue_hero ),
      redHero_( red_hero ),
      roundManager_( allUnitsInBattle_ ) {
    // Each hero's attack / defense skill is granted to all of its stacks as
    // a permanent flat bonus (HoMM3 rule), applied once at battle start.
    auto enlist = [this]( const models::Hero& hero ) {
        const int atk = hero.getAttack( );
        const int def = hero.getDefense( );
        for ( const auto& unit_ptr : hero.getArmy( ).getUnits( ) ) {
            if ( ! unit_ptr ) {
                continue;
            }
            if ( atk != 0 || def != 0 ) {
                unit_ptr->applyBuff( models::BuffFactory::createHeroBonusBuff( atk, def ) );
            }
            allUnitsInBattle_.push_back( unit_ptr.get( ) );
            placeUnitOnBoard( board_, unit_ptr );
        }
    };
    enlist( this->blueHero_ );
    enlist( this->redHero_ );

    roundManager_.startRound( );
}

Unit* GameManager::getCurrentUnit( ) {
    const std::size_t before = roundManager_.getUnitsLeftInRound( ).size( );
    Unit* u = roundManager_.getCurrentUnit( );
    if ( u != nullptr && before == 0 ) {
        advanceRound( );
    }

    if ( roundNumber_ != moraleRoundTracked_ ) {
        moraleRolledUnitsThisRound_.clear( );
        moraleRoundTracked_ = roundNumber_;
    }

    rollMoraleForActive( u );
    return u;
}

void GameManager::advanceRound( ) {
    ++roundNumber_;
    for ( Unit* u : allUnitsInBattle_ ) {
        if ( u != nullptr && u->getCount( ) > 0 ) {
            u->onTurnStart( );   // ticks all buff durations
        }
    }
    blueHero_.resetCastFlagForNewRound( );
    redHero_.resetCastFlagForNewRound( );
}

models::Hero* GameManager::getCasterFor( const models::Unit& unit ) {
    if ( heroContainsUnit( blueHero_, unit ) ) {
        return &blueHero_;
    }
    if ( heroContainsUnit( redHero_, unit ) ) {
        return &redHero_;
    }
    return nullptr;
}

void GameManager::rollMoraleForActive( Unit* unit ) {
    if ( ! moraleEnabled_ ) {
        moraleTriggeredThisTurn_ = false;
        lastMoraleRolledUnit_ = unit;
        return;
    }
    if ( unit == nullptr ) {
        lastMoraleRolledUnit_ = nullptr;
        moraleTriggeredThisTurn_ = false;
        return;
    }
    if ( unit == lastMoraleRolledUnit_ ) {
        return;
    }

    if ( moraleRolledUnitsThisRound_.count( unit ) > 0 ) {
        moraleTriggeredThisTurn_ = false;
        lastMoraleRolledUnit_ = unit;
        return;
    }

    static thread_local std::mt19937 rng{ std::random_device{ }( ) };
    std::uniform_int_distribution<int> dist( 0, 99 );
    moraleTriggeredThisTurn_ = ( dist( rng ) < 10 );
    lastMoraleRolledUnit_ = unit;
    moraleRolledUnitsThisRound_.insert( unit );
}

void GameManager::consumeTurnOrBurnMoraleBonus( ) {
    if ( moraleTriggeredThisTurn_ ) {
        moraleTriggeredThisTurn_ = false;
        return;
    }
    roundManager_.endCurrentUnitTurn( );
    lastMoraleRolledUnit_ = nullptr;
}

std::vector<Unit*> GameManager::getUnitsLeftInRound( ) const {
    return roundManager_.getUnitsLeftInRound( );
}

std::vector<Unit*> GameManager::getUnitQueueInRound( ) const {
    return roundManager_.getUnitQueueInRound( );
}

bool GameManager::canCurrentUnitWait( ) const {
    return roundManager_.currentUnitCanWait( );
}

std::vector<Hex*> GameManager::getAvailableDestinations( const Unit& unit ) const {
    return actionManager_.getAvailableDestinations( unit, board_ );
}

std::vector<const Hex*> GameManager::findPath( const Unit& unit, const Hex& dest_hex ) const {
    return actionManager_.findPath( unit, dest_hex, board_ );
}

std::vector<Unit*> GameManager::peekNextRoundOrder( ) const {
    std::vector<Unit*> next;
    next.reserve( allUnitsInBattle_.size( ) );
    for ( Unit* u : allUnitsInBattle_ ) {
        if ( u != nullptr && u->getCount( ) > 0 ) {
            next.push_back( u );
}
    }
    std::sort( next.begin( ), next.end( ), []( const Unit* a, const Unit* b ) {
        if ( a->getSpeed( ) != b->getSpeed( ) ) {
            return a->getSpeed( ) > b->getSpeed( );
}
        return a < b;
    } );
    return next;
}

std::vector<std::pair<Unit*, Hex*>> GameManager::getAvailableAttacks( const Unit& unit ) const {
    std::vector<std::pair<Unit*, Hex*>> filtered;
    for ( const auto& [target, hex] : actionManager_.getAvailableAttacks( unit, board_ ) ) {
        if ( target == nullptr || hex == nullptr ) {
            continue;
        }
        if ( areEnemies( unit, *target ) ) {
            filtered.push_back( { target, hex } );
        }
    }
    return filtered;
}

bool GameManager::heroContainsUnit( const Hero& hero, const Unit& unit ) const {
    for ( const auto& candidate : hero.getArmy( ).getUnits( ) ) {
        if ( candidate && candidate.get( ) == &unit ) {
            return true;
        }
    }
    return false;
}

bool GameManager::areAllies( const Unit& first, const Unit& second ) const {
    const bool first_blue = heroContainsUnit( blueHero_, first );
    const bool second_blue = heroContainsUnit( blueHero_, second );
    if ( first_blue && second_blue ) {
        return true;
    }

    const bool first_red = heroContainsUnit( redHero_, first );
    const bool second_red = heroContainsUnit( redHero_, second );
    if ( first_red && second_red ) {
        return true;
    }

    return false;
}

bool GameManager::areEnemies( const Unit& first, const Unit& second ) const {
    const bool first_known =
        heroContainsUnit( blueHero_, first ) || heroContainsUnit( redHero_, first );
    const bool second_known =
        heroContainsUnit( blueHero_, second ) || heroContainsUnit( redHero_, second );
    if ( ! first_known || ! second_known ) {
        return &first != &second;
    }
    return ! areAllies( first, second );
}

bool GameManager::canAttack( const Unit& attacker, const Hex& target_hex ) const {
    const auto attacks = getAvailableAttacks( attacker );
    Unit* clicked_target = nullptr;
    if ( target_hex.hasUnit( ) ) {
        clicked_target = target_hex.getUnit( ).get( );
    }

    for ( const auto& [target, hex] : attacks ) {
        if ( target == nullptr ) {
            continue;
        }

        if ( clicked_target != nullptr && target == clicked_target ) {
            return true;
        }
        if ( hex == &target_hex ) {
            return true;
        }
    }
    return false;
}

bool GameManager::canMove( const Unit& unit, const Hex& dest_hex ) const {
    const auto destinations = getAvailableDestinations( unit );
    for ( const Hex* hex : destinations ) {
        if ( hex == &dest_hex ) {
            return true;
        }
    }
    return false;
}

void GameManager::nextTurn( ) {}

void GameManager::move( Unit& unit, Hex& dest_hex ) {
    actionManager_.move( unit, dest_hex, board_ );
    consumeTurnOrBurnMoraleBonus( );
}

void GameManager::attack( Unit& attacker, Unit& defender, Hex& attack_from_hex ) {
    const auto enemy_predicate = [this, &attacker]( const Unit& other ) {
        return areEnemies( attacker, other );
    };

    bool defender_died = false;
    if ( actionManager_.canShoot( attacker, defender, enemy_predicate, board_ ) ) {
        defender_died = actionManager_.shoot( attacker, defender, board_ );
    } else {
        defender_died = actionManager_.attack( attacker, defender, attack_from_hex, board_ );
    }

    if ( defender_died ) {
        removeDeadUnit( defender );
    }
    consumeTurnOrBurnMoraleBonus( );
}

bool GameManager::willShoot( const Unit& attacker, const Unit& defender ) const {
    const auto enemy_predicate = [this, &attacker]( const Unit& other ) {
        return areEnemies( attacker, other );
    };
    return actionManager_.canShoot( attacker, defender, enemy_predicate, board_ );
}

void GameManager::wait( Unit& unit ) {
    moraleTriggeredThisTurn_ = false;
    lastMoraleRolledUnit_ = nullptr;
    roundManager_.waitCurrentUnit( );
}

void GameManager::defend( Unit& unit ) {
    actionManager_.defend( unit );
    consumeTurnOrBurnMoraleBonus( );
}

void GameManager::removeDeadUnit( Unit& unit ) {
    auto it = std::find( allUnitsInBattle_.begin( ), allUnitsInBattle_.end( ), &unit );
    if ( it != allUnitsInBattle_.end( ) ) {
        allUnitsInBattle_.erase( it );
    }
}

int GameManager::sideOfUnit( const Unit& unit ) const {
    if ( heroContainsUnit( blueHero_, unit ) ) {
        return 0;
    }
    if ( heroContainsUnit( redHero_, unit ) ) {
        return 1;
    }
    return -1;
}

std::unique_ptr<GameManager> GameManager::clone( ) const {
    // Default ctor leaves heroes/units empty and binds the new round
    // manager to the new (empty) allUnitsInBattle_ vector, which we then
    // populate in place.
    auto copy = std::make_unique<GameManager>( );

    std::unordered_map<const Unit*, Unit*> remap;
    copy->blueHero_ = cloneHeroDeep( blueHero_, remap );
    copy->redHero_ = cloneHeroDeep( redHero_, remap );

    // Rebuild the alive-units list in the same order, remapped to clones.
    copy->allUnitsInBattle_.clear( );
    copy->allUnitsInBattle_.reserve( allUnitsInBattle_.size( ) );
    for ( Unit* original : allUnitsInBattle_ ) {
        const auto it = remap.find( original );
        if ( it != remap.end( ) ) {
            copy->allUnitsInBattle_.push_back( it->second );
        }
    }

    // Place living units (corpses are irrelevant to search logic).
    for ( const std::shared_ptr<Unit>& u : copy->blueHero_.getArmy( ).getUnits( ) ) {
        if ( u && u->getCount( ) > 0 ) {
            placeUnitOnBoard( copy->board_, u );
        }
    }
    for ( const std::shared_ptr<Unit>& u : copy->redHero_.getArmy( ).getUnits( ) ) {
        if ( u && u->getCount( ) > 0 ) {
            placeUnitOnBoard( copy->board_, u );
        }
    }

    // Reproduce round number + the unactivated/waited scheduling.
    copy->roundNumber_ = roundNumber_;
    std::vector<Unit*> unactivated = roundManager_.snapshotUnactivated( );
    std::vector<Unit*> waited = roundManager_.snapshotWaited( );
    const auto remap_ptr = [&remap]( Unit* p ) -> Unit* {
        const auto it = remap.find( p );
        return it != remap.end( ) ? it->second : nullptr;
    };
    for ( Unit*& p : unactivated ) {
        p = remap_ptr( p );
    }
    for ( Unit*& p : waited ) {
        p = remap_ptr( p );
    }
    copy->roundManager_.restoreState( unactivated, waited );

    // Search must be deterministic: no random morale bonus turns.
    copy->moraleEnabled_ = false;
    copy->moraleTriggeredThisTurn_ = false;
    copy->lastMoraleRolledUnit_ = nullptr;
    copy->moraleRolledUnitsThisRound_.clear( );
    copy->moraleRoundTracked_ = roundNumber_;

    return copy;
}

void GameManager::notifyUnitMaybeDied( models::Unit& unit ) {
    if ( unit.getCount( ) > 0 ) {
        return;
    }
    try {
        models::Hex& head_hex = board_.getHex( unit.getQ( ), unit.getR( ), unit.getS( ) );
        head_hex.unitDied( );
        if ( unit.getSize( ) == 2 ) {
            const int tail_dq = unit.isFacingLeft( ) ? 1 : -1;
            try {
                models::Hex& tail_hex =
                    board_.getHex( unit.getQ( ) + tail_dq, unit.getR( ), unit.getS( ) - tail_dq );
                tail_hex.unitDied( );
            } catch ( const std::out_of_range& ) {}
        }
    } catch ( const std::out_of_range& ) {}
    removeDeadUnit( unit );
}

} // namespace core
