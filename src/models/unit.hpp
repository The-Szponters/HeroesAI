#pragma once
#include <string>
#include <vector>
#include <algorithm>
#include "buff.hpp"

class Unit
{
public:
    Unit() = default;
    Unit(std::string name,
         int tier,
         int attack,
         int defense,
         int health,
         int damage_min,
         int damage_max,
         int speed,
         int count,
         std::string asset_filename = "",
         std::string description = "")
        : name(std::move(name)), tier(tier), attack(attack), defense(defense), health(health), damage_min(damage_min), damage_max(damage_max), speed(speed), count(count), health_left(health),
          total_attack(attack), total_defense(defense), total_damage_min(damage_min), total_damage_max(damage_max), total_speed(speed),
          asset_filename(std::move(asset_filename)), description(std::move(description)) {}
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

    int get_base_attack() const { return attack; }
    int get_base_defense() const { return defense; }
    int get_base_speed() const { return speed; }
    int get_base_damage_min() const { return damage_min; }
    int get_base_damage_max() const { return damage_max; }
    const std::string& get_asset_filename() const { return asset_filename; }
    const std::string& get_description() const { return description; }

    int get_size() const { return size; }
    void set_size(int s) { size = (s == 2 ? 2 : 1); }
    bool is_teleporter_unit() const { return is_teleporter; }
    void set_is_teleporter(bool value) { is_teleporter = value; }
    bool is_flying_unit() const { return is_flying; }
    void set_is_flying(bool value) { is_flying = value; }

    bool ignores_path_blockers() const { return is_flying || is_teleporter; }

    bool has_retaliated_this_round() const { return has_retaliated; }
    void set_retaliated(bool v) { has_retaliated = v; }

    virtual bool is_ranged() const { return false; }
    virtual int  get_ammo() const { return 0; }
    virtual int  get_max_ammo() const { return 0; }
    virtual int  get_max_range_damage() const { return 0; }
    virtual void decrement_ammo() {}

    virtual const std::string& get_projectile_asset() const {
        static const std::string empty;
        return empty;
    }

    bool is_facing_left() const { return logical_facing_left; }

    bool get_visual_facing_left() const { return visual_facing_left; }
    void set_visual_facing_left(bool value) { visual_facing_left = value; }

    void set_position(int new_q, int new_r, int new_s) {
        if (!position_initialized) {

            logical_facing_left = (new_q >= 7);
            visual_facing_left  = (new_q >= 7);
            position_initialized = true;
        } else if (new_q != q) {

            visual_facing_left = (new_q < q);

        }
        q = new_q; r = new_r; s = new_s;
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
        has_retaliated = false;
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

    int size = 1;
    bool is_teleporter = false;
    bool is_flying = false;
    bool has_retaliated = false;
    bool logical_facing_left = false;   
    bool visual_facing_left  = false;   
    bool position_initialized = false;

    std::string asset_filename;
    std::string description;

    std::vector<Buff> active_buffs;
};