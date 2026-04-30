/**
 * @file Hero.h
 * @brief Hero entity that owns an army and provides primary stats.
 * @author Łukasz Szydlik
 */
#pragma once
#include <string>
#include <utility>

#include "Army.h"

namespace models {

/**
 * @brief A battle commander.
 *
 * Holds the four primary stats (attack, defense, power, knowledge)
 * and the Army of unit stacks the hero brings into combat.
 */
class Hero {
public:
    Hero( ) = default;

    Hero( std::string name, int attack, int defense, int power, int knowledge )
        : name_( std::move( name ) ),
          attack_( attack ),
          defense_( defense ),
          power_( power ),
          knowledge_( knowledge ) {}

    ~Hero( ) = default;

    const std::string& getName( ) const { return name_; }
    int getAttack( ) const { return attack_; }
    int getDefense( ) const { return defense_; }
    int getPower( ) const { return power_; }
    int getKnowledge( ) const { return knowledge_; }

    Army& getArmy( ) { return army_; }
    const Army& getArmy( ) const { return army_; }

private:
    std::string name_;
    int attack_ = 0;
    int defense_ = 0;
    int power_ = 0;
    int knowledge_ = 0;

    Army army_;
};

} // namespace models