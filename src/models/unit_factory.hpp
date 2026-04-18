#pragma once

#include "unit.hpp"
#include "range_unit.hpp"
#include "unit_id.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <unordered_map>

class UnitFactory {
public:
    static void init(const std::string& filepath);
    static std::unique_ptr<Unit> create_unit(UnitID id, int count);

private:
    static std::unordered_map<UnitID, nlohmann::json> unit_data;
    static std::string unit_id_to_string(UnitID id);
};
