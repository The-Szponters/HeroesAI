#pragma once
#include <string>
#include <vector>
#include <algorithm>
#include "buff.hpp"

class Unit
{
public:
    Unit() = default;
    Unit(std::string name, int tier, int attack, int defense, int health, int damage_min, int damage_max, int speed, int count)
        : name(std::move(name)), tier(tier), attack(attack), defense(defense), health(health), damage_min(damage_min), damage_max(damage_max), speed(speed), count(count), health_left(health),
          total_attack(attack), total_defense(defense), total_damage_min(damage_min), total_damage_max(damage_max), total_speed(speed) {}
    virtual ~Unit() = default;

    const std::string& get_name() const { return name; }
    int get_tier() const { return tier; } 
    int get_attack() const { return total_attack; }
    int get_defense() const { return total_defense; }
    int get_health() const { return health; }
    int get_damage_min() const { return total_damage_min; }
    int get_damage_max() const { return total_damage_max; }
    int get_speed() const { return total_speed; }
    int get_count() const { return count; }
    int get_health_left() const { return health_left; }
    int get_q() const { return q; }
    int get_r() const { return r; }
    int get_s() const { return s; }

    void set_position(int q, int r, int s) {
        this->q = q;
        this->r = r;
        this->s = s;
    }

    void take_damage(int damage) {
        int total_health = health_left + (count - 1) * health;
        total_health -= damage;
        if (total_health < 0) total_health = 0;

        count = (total_health + health - 1) / health;
        health_left = total_health % health;
        if (health_left == 0 && count > 0) {
            health_left = health;
        }
    }

    void apply_buff(const Buff& buff) {
        auto it = std::find_if(active_buffs.begin(), active_buffs.end(), [&buff](const Buff& b) {
            return b.type == buff.type;
        });
        if (it != active_buffs.end()) {
            *it = buff;
        } else {
            active_buffs.push_back(buff);
        }
        recalculate_stats();
    }

    void remove_buff(BuffType type) {
        std::erase_if(active_buffs, [type](const Buff& b) {
            return b.type == type;
        });
        recalculate_stats();
    }

    void recalculate_stats() {
        total_attack = attack;
        total_defense = defense;
        total_damage_min = damage_min;
        total_damage_max = damage_max;
        total_speed = speed;

        for (const auto& buff : active_buffs) {
            total_attack = buff.modify_attack(total_attack);
            total_defense = buff.modify_defense(total_defense);
            total_damage_min = buff.modify_damage_min(total_damage_min);
            total_damage_max = buff.modify_damage_max(total_damage_max);
            total_speed = buff.modify_speed(total_speed);
        }

        total_attack = std::max(0, total_attack);
        total_defense = std::max(0, total_defense);
        total_damage_min = std::max(0, total_damage_min);
        total_damage_max = std::max(0, total_damage_max);
        total_speed = std::max(0, total_speed);
    }

    void on_turn_start() {
        bool removed = false;
        for (auto& buff : active_buffs) {
            buff.duration--;
            if (buff.duration <= 0) {
                removed = true;
            }
        }
        if (removed) {
            std::erase_if(active_buffs, [](const Buff& b) {
                return b.duration <= 0;
            });
            recalculate_stats();
        }
    }

private:
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
    int q = 0; int r = 0; int s = 0;

    int total_attack = 1;
    int total_defense = 1;
    int total_damage_min = 1;
    int total_damage_max = 1;
    int total_speed = 1;

    std::vector<Buff> active_buffs;
};