#pragma once
#include "unit.hpp"

class RangeUnit : public Unit
{
public:
    RangeUnit() = default;
    RangeUnit(std::string name, int tier, int attack, int defense, int health, int damage, int speed, int size, int shoots)
        : Unit(name, tier, attack, defense, health, damage, speed, size), shoots(shoots) {}
    ~RangeUnit() override = default;

    int get_shoots() const { return shoots; }

private:
    int shoots = 1;
};