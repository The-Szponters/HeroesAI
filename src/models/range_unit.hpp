#pragma once
#include "unit.hpp"

#include <algorithm>
#include <cctype>
#include <string>

// A creature with a finite supply of shots (HoMM3 "ammo cart" mechanic).
// Combat policy lives in ActionManager / GameManager — RangeUnit only owns:
//   • max_ammo  — initial ammunition pool from units.json ("shoots").
//   • ammo      — remaining shots; decremented on every successful ranged
//                 attack and never restored mid-battle.
//   • the sprite filename of the projectile that flies from attacker to target
//     (the unit's *attack pose* is its own DEF; this is the in-flight art).
class RangeUnit : public Unit {
public:
    RangeUnit() = default;
    RangeUnit(std::string name,
              int tier,
              int attack,
              int defense,
              int health,
              int damage_min,
              int damage_max,
              int speed,
              int count,
              int shoots,
              std::string asset_filename = "",
              std::string description = "")
        : Unit(std::move(name),
               tier,
               attack, defense, health,
               damage_min, damage_max,
               speed, count,
               asset_filename,
               std::move(description)),
          max_ammo(shoots),
          ammo(shoots),
          projectile_asset(infer_projectile_asset(asset_filename)) {}
    ~RangeUnit() override = default;

    bool is_ranged() const override { return true; }
    int  get_ammo() const override { return ammo; }
    int  get_max_ammo() const override { return max_ammo; }
    int  get_max_range_damage() const override {
        // No unit currently has asymmetric melee/ranged damage; reuse the base
        // damage_max so the View can display ammo+damage from a single source
        // of truth.  Override per-unit later if a creature breaks this rule.
        return get_base_damage_max();
    }
    void decrement_ammo() override {
        if (ammo > 0) --ammo;
    }
    const std::string& get_projectile_asset() const override { return projectile_asset; }

    // Legacy alias kept until callers migrate.
    int get_shoots() const { return max_ammo; }

private:
    // Match the unit's main DEF filename (case-insensitive) to the projectile
    // DEF that ships with it.  Centralising the table here avoids leaking the
    // mapping into View/Presenter code.
    static std::string infer_projectile_asset(const std::string& unit_asset) {
        auto iequals = [](const std::string& a, const char* b) {
            const std::size_t n = std::char_traits<char>::length(b);
            if (a.size() != n) return false;
            for (std::size_t i = 0; i < n; ++i) {
                if (std::tolower(static_cast<unsigned char>(a[i])) !=
                    std::tolower(static_cast<unsigned char>(b[i]))) return false;
            }
            return true;
        };

        if (iequals(unit_asset, "CLCBOW.def") || iequals(unit_asset, "CHCBOW.def"))
            return "archer_shoot.def";
        if (iequals(unit_asset, "Cmonkk.def") || iequals(unit_asset, "Czealt.def"))
            return "zealot_shoot.def";
        if (iequals(unit_asset, "CGOG.def")   || iequals(unit_asset, "CMAGOG.def"))
            return "gog_shoot.def";
        if (iequals(unit_asset, "CLICH.def")  || iequals(unit_asset, "CPLICH.def"))
            return "lich_shoot.def";
        return {};
    }

    int max_ammo = 0;
    int ammo = 0;
    std::string projectile_asset;
};
