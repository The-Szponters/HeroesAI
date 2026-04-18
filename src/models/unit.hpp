#pragma once
#include <string>

class Unit
{
public:
    Unit() = default;
    Unit(std::string name, int tier, int attack, int defense, int health, int damage_min, int damage_max, int speed, int count)
        : name(std::move(name)), tier(tier), attack(attack), defense(defense), health(health), damage_min(damage_min), damage_max(damage_max), speed(speed), count(count), health_left(health) {}
    virtual ~Unit() = default;

    const std::string& get_name() const { return name; }
    int get_tier() const { return tier; } 
    int get_attack() const { return attack; }
    int get_defense() const { return defense; }
    int get_health() const { return health; }
    int get_damage_min() const { return damage_min; }
    int get_damage_max() const { return damage_max; }
    int get_speed() const { return speed; }
    int get_count() const { return count; }
    int get_health_left() const { return health_left; }

protected:
    std::string name;
    int tier = 1;
    int attack = 1;
    int defense = 1;
    int health = 1;
    int health_left = 1;
    int damage_min = 1;
    int damage_max = 1;
    int speed = 1;
    int count = 1;
};