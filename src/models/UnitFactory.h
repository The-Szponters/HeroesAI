/**
 * @file UnitFactory.h
 * @brief Factory that builds Unit / RangeUnit instances from JSON data.
 */
#pragma once

#include "Unit.h"
#include "RangeUnit.h"
#include "UnitId.h"
#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <unordered_map>

namespace models {

/**
 * @brief Static factory that constructs game units from JSON definitions.
 *
 * Loads a JSON catalog at startup (init()) and produces shared Unit /
 * RangeUnit instances on demand for a given UnitID and stack count.
 * Throws std::runtime_error if the catalog is missing fields.
 */
class UnitFactory {
public:
    static void init(const std::string& filepath );
    static std::shared_ptr<Unit> create_unit(UnitID id, int count );

private:
    static std::unordered_map<UnitID, nlohmann::json> unit_data;
    static std::string unit_id_to_string(UnitID id );
};

}  // namespace models
