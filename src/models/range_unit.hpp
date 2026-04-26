#pragma once
#include "unit.hpp"

class RangeUnit : public Unit
{
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
               attack,
               defense,
               health,
               damage_min,
               damage_max,
               speed,
               count,
               std::move(asset_filename),
               std::move(description)),
          shoots(shoots) {}
    ~RangeUnit() override = default;

    int get_shoots() const { return shoots; }

private:
    int shoots = 1;
};