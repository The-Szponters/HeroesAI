#pragma once
#include <vector>
#include <memory>
#include "unit.hpp"

class Army {
public:
    Army() = default;
    ~Army() = default;

    bool add_unit(std::shared_ptr<Unit> unit) {
        if (units.size() < 7 && unit != nullptr) {
            units.push_back(std::move(unit));
            return true;
        }
        return false;
    }

    void remove_unit(size_t index) {
        if (index < units.size()) {
            units.erase(units.begin() + index);
        }
    }

    const std::vector<std::shared_ptr<Unit>>& get_units() const {
        return units;
    }

private:
    std::vector<std::shared_ptr<Unit>> units;
};