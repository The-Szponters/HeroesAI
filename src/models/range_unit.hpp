#pragma once
#include "unit.hpp"

class RangeUnit : public Unit
{
public:
    RangeUnit() = default;
    RangeUnit(std::string name, int tier, int attack, int defense, int health, int damage_min, int damage_max, int speed, int count, int shoots)
        : Unit(name, tier, attack, defense, health, damage_min, damage_max, speed, count), shoots(shoots) {}
    ~RangeUnit() override = default;

    int get_shoots() const { return shoots; }

private:
    int shoots = 1;
};