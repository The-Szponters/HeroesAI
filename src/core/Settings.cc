/**
 * @file Settings.cc
 * @brief Implementation of the JSON settings loader / saver.
 * @author Łukasz Szydlik
 */
#include "Settings.h"

#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

#include "../models/UnitFactory.h"

namespace core {

namespace {

ArmyConfig defaultLeftArmy( ) {
    using models::UnitID;
    ArmyConfig army;
    army[0] = { UnitID::PIKEMAN, 10 };
    army[1] = { UnitID::ARCHER, 8 };
    army[2] = { UnitID::GRIFFIN, 5 };
    army[3] = { UnitID::SWORDSMAN, 6 };
    army[4] = { UnitID::MONK, 4 };
    army[5] = { UnitID::CAVALIER, 3 };
    army[6] = { UnitID::ARCHANGEL, 1 };
    return army;
}

ArmyConfig defaultRightArmy( ) {
    using models::UnitID;
    ArmyConfig army;
    army[0] = { UnitID::IMP, 12 };
    army[1] = { UnitID::GOG, 8 };
    army[2] = { UnitID::HELL_HOUND, 5 };
    army[3] = { UnitID::DEMON, 6 };
    army[4] = { UnitID::PIT_FIEND, 4 };
    army[5] = { UnitID::EFREET, 3 };
    army[6] = { UnitID::DEVIL, 1 };
    return army;
}

nlohmann::json armyToJson( const ArmyConfig& army ) {
    nlohmann::json arr = nlohmann::json::array( );
    for ( const ArmySlot& slot : army ) {
        nlohmann::json entry;
        if ( slot.unitId_.has_value( ) ) {
            entry["unit"] = models::UnitFactory::idToString( *slot.unitId_ );
        } else {
            entry["unit"] = nullptr;
        }
        entry["count"] = slot.count_;
        arr.push_back( entry );
    }
    return arr;
}

void readArmyFromJson( const nlohmann::json& arr, ArmyConfig& out ) {
    if ( ! arr.is_array( ) ) {
        return;
    }
    // The array fully defines the roster: reset every slot to empty
    // first so that unspecified trailing slots don't keep the built-in
    // default units (a shorter array would otherwise only patch the
    // leading slots and leave the defaults behind).
    out = ArmyConfig{ };
    const std::size_t entries = std::min( arr.size( ), K_ARMY_SLOT_COUNT );
    for ( std::size_t i = 0; i < entries; ++i ) {
        const nlohmann::json& entry = arr[i];
        ArmySlot slot;
        if ( entry.contains( "unit" ) && entry["unit"].is_string( ) ) {
            const std::string unit_name = entry["unit"].get<std::string>( );
            slot.unitId_ = models::UnitFactory::idFromString( unit_name );
        }
        if ( entry.contains( "count" ) && entry["count"].is_number_integer( ) ) {
            slot.count_ = entry["count"].get<int>( );
        }
        out[i] = slot;
    }
}

} // namespace

Settings::Settings( ) : leftArmy_( defaultLeftArmy( ) ), rightArmy_( defaultRightArmy( ) ) {}

Settings Settings::loadFromFile( const std::string& filepath ) {
    Settings settings;
    std::ifstream file( filepath );
    if ( ! file.is_open( ) ) {
        std::cerr << "Settings: could not open '" << filepath
                  << "' -- using built-in defaults." << std::endl;
        return settings;
    }

    nlohmann::json j;
    try {
        file >> j;
    } catch ( const std::exception& e ) {
        // Surface the failure loudly: a malformed config otherwise looks
        // identical to a missing one and the game silently ignores every
        // user setting. nlohmann's message pinpoints the line/column.
        std::cerr << "Settings: failed to parse '" << filepath << "': " << e.what( )
                  << "\nSettings: using built-in defaults instead." << std::endl;
        return settings;
    }

    if ( j.contains( "window" ) && j["window"].is_object( ) ) {
        const nlohmann::json& w = j["window"];
        if ( w.contains( "width" ) && w["width"].is_number_integer( ) ) {
            settings.windowWidth_ = w["width"].get<unsigned int>( );
        }
        if ( w.contains( "height" ) && w["height"].is_number_integer( ) ) {
            settings.windowHeight_ = w["height"].get<unsigned int>( );
        }
        if ( w.contains( "title" ) && w["title"].is_string( ) ) {
            settings.windowTitle_ = w["title"].get<std::string>( );
        }
        if ( w.contains( "framerate_limit" ) && w["framerate_limit"].is_number_integer( ) ) {
            settings.framerateLimit_ = w["framerate_limit"].get<unsigned int>( );
        }
    }

    if ( j.contains( "left_army" ) ) {
        readArmyFromJson( j["left_army"], settings.leftArmy_ );
    }
    if ( j.contains( "right_army" ) ) {
        readArmyFromJson( j["right_army"], settings.rightArmy_ );
    }

    if ( j.contains( "ai" ) && j["ai"].is_object( ) ) {
        const nlohmann::json& ai = j["ai"];
        if ( ai.contains( "blue_is_bot" ) && ai["blue_is_bot"].is_boolean( ) ) {
            settings.blueIsBot_ = ai["blue_is_bot"].get<bool>( );
        }
        if ( ai.contains( "red_is_bot" ) && ai["red_is_bot"].is_boolean( ) ) {
            settings.redIsBot_ = ai["red_is_bot"].get<bool>( );
        }
    }

    return settings;
}

void Settings::saveToFile( const std::string& filepath ) const {
    nlohmann::json j;
    j["window"] = {
        { "width", windowWidth_ },
        { "height", windowHeight_ },
        { "title", windowTitle_ },
        { "framerate_limit", framerateLimit_ }
    };
    j["left_army"] = armyToJson( leftArmy_ );
    j["right_army"] = armyToJson( rightArmy_ );
    j["ai"] = {
        { "blue_is_bot", blueIsBot_ },
        { "red_is_bot", redIsBot_ }
    };

    std::ofstream file( filepath );
    if ( ! file.is_open( ) ) {
        return;
    }
    file << j.dump( 4 );
}

} // namespace core
