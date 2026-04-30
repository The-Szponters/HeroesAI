/**
 * @file Hero.h
 * @brief Hero entity that owns an army and provides primary stats.
 */
#pragma once
#include "Army.h"
#include <string>
#include <utility>

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
        : name( std::move( name ) ),
          attack( attack ),
          defense( defense ),
          power( power ),
          knowledge( knowledge ) {}

    ~Hero( ) = default;

    const std::string& get_name( ) const { return name; }
    int get_attack( ) const { return attack; }
    int get_defense( ) const { return defense; }
    int get_power( ) const { return power; }
    int get_knowledge( ) const { return knowledge; }

    Army& get_army( ) { return army; }
    const Army& get_army( ) const { return army; }

private:
    std::string name;
    int attack = 0;
    int defense = 0;
    int power = 0;
    int knowledge = 0;

    Army army;
};

} // namespace models