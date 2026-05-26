/**
 * @file Hero.h
 * @brief Hero entity that owns an army and provides primary stats.
 * @author Łukasz Szydlik
 */
#pragma once
#include <algorithm>
#include <string>
#include <utility>

#include "Army.h"

namespace models {

/**
 * @brief A battle commander.
 *
 * Holds the four primary stats (attack, defense, power, knowledge),
 * a mana pool (max = knowledge * 10) used to cast spells, the
 * per-round cast flag (limit of one cast per round), and the Army of
 * unit stacks the hero brings into combat.
 */
class Hero {
public:
    Hero( ) : maxMana_( knowledge_ * 10 ), currentMana_( knowledge_ * 10 ) {}

    Hero( std::string name, int attack, int defense, int power, int knowledge )
        : name_( std::move( name ) ),
          attack_( attack ),
          defense_( defense ),
          power_( power ),
          knowledge_( knowledge ),
          maxMana_( knowledge * 10 ),
          currentMana_( knowledge * 10 ) {}

    ~Hero( ) = default;

    const std::string& getName( ) const { return name_; }
    int getAttack( ) const { return attack_; }
    int getDefense( ) const { return defense_; }
    int getPower( ) const { return power_; }
    int getKnowledge( ) const { return knowledge_; }

    int getMaxMana( ) const { return maxMana_; }
    int getCurrentMana( ) const { return currentMana_; }
    bool hasCastThisRound( ) const { return hasCastThisRound_; }

    void spendMana( int amount ) {
        currentMana_ = std::max( 0, currentMana_ - amount );
    }
    void markCastThisRound( ) { hasCastThisRound_ = true; }
    void resetCastFlagForNewRound( ) { hasCastThisRound_ = false; }

    Army& getArmy( ) { return army_; }
    const Army& getArmy( ) const { return army_; }

private:
    std::string name_;
    int attack_ = 0;
    int defense_ = 0;
    int power_ = 0;
    int knowledge_ = 0;

    int maxMana_ = 0;
    int currentMana_ = 0;
    bool hasCastThisRound_ = false;

    Army army_;
};

} // namespace models
