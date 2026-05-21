/**
 * @file UnitFactory.h
 * @brief Factory that builds Unit / RangeUnit instances from JSON data.
 * @author Łukasz Szydlik
 */
#pragma once

#include <memory>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <unordered_map>

#include "RangeUnit.h"
#include "Unit.h"
#include "UnitId.h"

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
    static void init( const std::string& filepath );
    static std::shared_ptr<Unit> createUnit( UnitID id, int count );

    static std::string idToString( UnitID id );
    static std::optional<UnitID> idFromString( const std::string& name );

    static std::optional<PortraitRect> getPortraitRect( UnitID id );

private:
    static std::unordered_map<UnitID, nlohmann::json> UnitData;
    static std::string unitIdToString( UnitID id );
};

} // namespace models
