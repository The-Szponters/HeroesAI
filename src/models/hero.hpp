#pragma once
#include <string>
#include <utility>
#include "army.hpp"

class Hero {
public:
    Hero() = default;
    
    Hero(std::string name, int attack, int defense, int power, int knowledge)
        : name(std::move(name)), attack(attack), defense(defense), power(power), knowledge(knowledge) {}
    
    ~Hero() = default;

    const std::string& get_name() const { return name; }
    int get_attack() const { return attack; }
    int get_defense() const { return defense; }
    int get_power() const { return power; }
    int get_knowledge() const { return knowledge; }

    Army& get_army() { return army; } 

private:
    std::string name;
    int attack = 0;
    int defense = 0;
    int power = 0;
    int knowledge = 0;
    
    Army army;
};