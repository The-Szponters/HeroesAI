#pragma once
#include <string>

class Unit
{
public:
    Unit() = default;
    Unit(std::string name, int tier, int attack, int defense, int health, int damage, int speed, int size)
        : name(name), tier(tier), attack(attack), defense(defense), health(health), damage(damage), speed(speed), size(size), health_left(health) {}
    virtual ~Unit() = default;

    std::string get_name() const { return name; }
    int get_tier() const { return tier; } 
    int get_attack() const { return attack; }
    int get_defense() const { return defense; }
    int get_health() const { return health; }
    int get_damage() const { return damage; }
    int get_speed() const { return speed; }
    int get_size() const { return size; }
    int get_health_left() const { return health_left; }

protected:
    std::string name;
    int tier = 1;
    int attack = 1;
    int defense = 1;
    int health = 1;
    int health_left = 1;
    int damage = 1;
    int speed = 1;
    int size = 1;
};