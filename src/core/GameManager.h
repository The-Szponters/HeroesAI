/**
 * @file GameManager.h
 * @brief High-level battle controller -- the model the presenter talks to.
 */
#pragma once
#include "../models/Hero.h"
#include "ActionManager.h"
#include "Board.h"
#include "Hex.h"
#include "RoundManager.h"
#include "Unit.h"
#include <unordered_set>
#include <vector>

namespace core {

/**
 * @brief Aggregates board, heroes, round scheduling and action resolution
 *        into a single facade representing one ongoing battle.
 *
 * The presenter queries this class for the current unit, legal moves
 * and attacks, then forwards player commands (move, attack, wait,
 * defend). It also rolls morale and tracks dead units.
 */
class GameManager {
public:
    GameManager( ) : roundManager_( allUnitsInBattle_ ) {}
    GameManager( const models::Hero& blue_hero, const models::Hero& red_hero );

    models::Unit* getCurrentUnit( );

    std::vector<models::Unit*> getUnitsLeftInRound( ) const;
    std::vector<models::Unit*> getUnitQueueInRound( ) const;

    std::vector<models::Hex*> getAvailableDestinations( const models::Unit& unit ) const;
    std::vector<std::pair<models::Unit*, models::Hex*>>
    getAvailableAttacks( const models::Unit& unit ) const;

    std::vector<const models::Hex*> findPath( const models::Unit& unit,
                                               const models::Hex& dest_hex ) const;

    int getRoundNumber( ) const { return roundNumber_; }
    std::vector<models::Unit*> peekNextRoundOrder( ) const;

    models::Board& getBoard( ) { return board_; }
    const models::Board& getBoard( ) const { return board_; }
    bool areAllies( const models::Unit& first, const models::Unit& second ) const;
    bool areEnemies( const models::Unit& first, const models::Unit& second ) const;
    bool canAttack( const models::Unit& attacker, const models::Hex& target_hex ) const;
    bool canMove( const models::Unit& unit, const models::Hex& dest_hex ) const;

    bool willShoot( const models::Unit& attacker, const models::Unit& defender ) const;

    bool activeUnitHasMoraleBonus( ) const { return moraleTriggeredThisTurn_; }

    void nextTurn( );

    void move( models::Unit& unit, models::Hex& dest_hex );
    void attack( models::Unit& attacker, models::Unit& defender, models::Hex& attack_from_hex );
    void wait( models::Unit& unit );
    void defend( models::Unit& unit );

    const models::Hero& getBlueHero( ) const { return blueHero_; }
    const models::Hero& getRedHero( ) const { return redHero_; }

private:
    void removeDeadUnit( models::Unit& unit );
    bool heroContainsUnit( const models::Hero& hero, const models::Unit& unit ) const;

    void consumeTurnOrBurnMoraleBonus( );

    void rollMoraleForActive( models::Unit* unit );

    models::Hero blueHero_;
    models::Hero redHero_;
    std::vector<models::Unit*> allUnitsInBattle_;
    int roundNumber_ = 1;

    models::Unit* lastMoraleRolledUnit_ = nullptr;
    bool moraleTriggeredThisTurn_ = false;
    int moraleRoundTracked_ = 1;
    std::unordered_set<models::Unit*> moraleRolledUnitsThisRound_;

    ActionManager actionManager_;
    RoundManager roundManager_;
    models::Board board_;
};

} // namespace core
