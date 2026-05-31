/**
 * @file Settings.cc
 * @brief Implementation of the JSON settings loader / saver.
 * @author Łukasz Szydlik
 */
#include "Settings.h"

#include <algorithm>
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

// Uses ordered_json so written objects keep insertion order instead of
// being alphabetised (nlohmann::json sorts keys; ordered_json does not).
nlohmann::ordered_json armyToJson( const ArmyConfig& army ) {
    nlohmann::ordered_json arr = nlohmann::ordered_json::array( );
    for ( const ArmySlot& slot : army ) {
        nlohmann::ordered_json entry;
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

/**
 * @brief Outcome of attempting to read a JSON document from disk.
 */
enum class JsonReadStatus {
    Ok,         ///< Parsed successfully into the output.
    Missing,    ///< File could not be opened (output left unchanged).
    ParseError  ///< File opened but JSON was invalid (output left unchanged).
};

/**
 * @brief Reads a JSON document from @p filepath.
 *
 * The output is only modified on success, so callers can keep a default
 * value. On a parse error the parser's message is returned via
 * @p error_out so the caller can phrase a context-specific diagnostic.
 */
template <typename Json>
JsonReadStatus readJsonFromFile( const std::string& filepath,
                                        Json& out,
                                        std::string& error_out ) {
    std::ifstream in( filepath );
    if ( ! in.is_open( ) ) {
        return JsonReadStatus::Missing;
    }
    Json parsed;
    try {
        in >> parsed;
    } catch ( const std::exception& e ) {
        error_out = e.what( );
        return JsonReadStatus::ParseError;
    }
    out = std::move( parsed );
    return JsonReadStatus::Ok;
}

// Resolves one side's controller from the "player" object, using the
// per-side string ("human"/"random"/"easy"); otherwise keeps @p fallback.
PlayerType readPlayerType( const nlohmann::json& player,
                                  const char* side_key,
                                  PlayerType fallback ) {
    if ( player.contains( side_key ) && player[side_key].is_string( ) ) {
        return playerTypeFromString( player[side_key].get<std::string>( ) );
    }
    return fallback;
}

nlohmann::ordered_json heroToJson( const HeroConfig& hero ) {
    return nlohmann::ordered_json{
        { "attack", hero.attack_ },
        { "defense", hero.defense_ },
        { "power", hero.power_ },
        { "knowledge", hero.knowledge_ }
    };
}

// Reads one side's hero stats from a "heroes" object, keeping @p out's
// current value for any field that is absent.
void readHeroConfig( const nlohmann::json& heroes, const char* side_key, HeroConfig& out ) {
    if ( ! heroes.contains( side_key ) || ! heroes[side_key].is_object( ) ) {
        return;
    }
    const nlohmann::json& h = heroes[side_key];
    if ( h.contains( "attack" ) && h["attack"].is_number_integer( ) ) {
        out.attack_ = h["attack"].get<int>( );
    }
    if ( h.contains( "defense" ) && h["defense"].is_number_integer( ) ) {
        out.defense_ = h["defense"].get<int>( );
    }
    if ( h.contains( "power" ) && h["power"].is_number_integer( ) ) {
        out.power_ = h["power"].get<int>( );
    }
    if ( h.contains( "knowledge" ) && h["knowledge"].is_number_integer( ) ) {
        out.knowledge_ = h["knowledge"].get<int>( );
    }
}

} // namespace

Settings::Settings( ) : leftArmy_( defaultLeftArmy( ) ), rightArmy_( defaultRightArmy( ) ) {}

Settings Settings::loadFromFile( const std::string& filepath ) {
    Settings settings;

    nlohmann::json j;
    std::string parse_error;
    const JsonReadStatus status = readJsonFromFile( filepath, j, parse_error );
    if ( status == JsonReadStatus::Missing ) {
        std::cerr << "Settings: could not open '" << filepath
                  << "' -- using built-in defaults." << std::endl;
        return settings;
    }
    if ( status == JsonReadStatus::ParseError ) {
        // Surface the failure loudly: a malformed config otherwise looks
        // identical to a missing one and the game silently ignores every
        // user setting. nlohmann's message pinpoints the line/column.
        std::cerr << "Settings: failed to parse '" << filepath << "': " << parse_error
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

    if ( j.contains( "player" ) && j["player"].is_object( ) ) {
        const nlohmann::json& player = j["player"];
        settings.bluePlayer_ = readPlayerType( player, "blue", settings.bluePlayer_ );
        settings.redPlayer_ = readPlayerType( player, "red", settings.redPlayer_ );
        if ( player.contains( "depth" ) && player["depth"].is_number_integer( ) ) {
            settings.minimaxDepth_ = std::max( 1, player["depth"].get<int>( ) );
        }
    }

    if ( j.contains( "heroes" ) && j["heroes"].is_object( ) ) {
        const nlohmann::json& heroes = j["heroes"];
        readHeroConfig( heroes, "blue", settings.blueHeroConfig_ );
        readHeroConfig( heroes, "red", settings.redHeroConfig_ );
    }

    return settings;
}

void Settings::saveToFile( const std::string& filepath ) const {
    // ordered_json keeps this insertion order in the output instead of
    // alphabetising the keys. Rosters are written last.
    nlohmann::ordered_json j;
    j["player"] = {
        { "blue", playerTypeToString( bluePlayer_ ) },
        { "red", playerTypeToString( redPlayer_ ) },
        { "depth", minimaxDepth_ }
    };
    j["heroes"] = {
        { "blue", heroToJson( blueHeroConfig_ ) },
        { "red", heroToJson( redHeroConfig_ ) }
    };
    j["window"] = {
        { "width", windowWidth_ },
        { "height", windowHeight_ },
        { "title", windowTitle_ },
        { "framerate_limit", framerateLimit_ }
    };
    j["left_army"] = armyToJson( leftArmy_ );
    j["right_army"] = armyToJson( rightArmy_ );

    std::ofstream file( filepath );
    if ( ! file.is_open( ) ) {
        return;
    }
    file << j.dump( 4 );
}

void Settings::saveArmySetupToFile( const std::string& filepath ) const {
    // Merge into whatever is already on disk so unrelated sections
    // (window, anything else) survive untouched. ordered_json preserves
    // the existing key order rather than alphabetising it.
    nlohmann::ordered_json j = nlohmann::ordered_json::object( );
    std::string parse_error;
    const JsonReadStatus status = readJsonFromFile( filepath, j, parse_error );
    if ( status == JsonReadStatus::ParseError ) {
        // Don't touch a malformed file -- repairing it is not this
        // function's job. Report and leave the file as the user left it.
        std::cerr << "Settings: could not merge army setup into '" << filepath
                  << "': " << parse_error << std::endl;
        return;
    }
    // A missing file is fine here: fall through and write a fresh
    // document. Guard against the file holding a non-object JSON value.
    if ( ! j.is_object( ) ) {
        j = nlohmann::ordered_json::object( );
    }

    // Per-side player type -- update in place so an existing "depth"
    // (and the block's position) is preserved.
    if ( ! j.contains( "player" ) || ! j["player"].is_object( ) ) {
        j["player"] = nlohmann::ordered_json::object( );
    }
    j["player"]["blue"] = playerTypeToString( bluePlayer_ );
    j["player"]["red"] = playerTypeToString( redPlayer_ );
    if ( ! j["player"].contains( "depth" ) ) {
        j["player"]["depth"] = minimaxDepth_;
    }

    // Hero stats.
    j["heroes"] = {
        { "blue", heroToJson( blueHeroConfig_ ) },
        { "red", heroToJson( redHeroConfig_ ) }
    };

    // Rosters last (erase first so the re-inserted arrays land at the end).
    j.erase( "left_army" );
    j.erase( "right_army" );
    j["left_army"] = armyToJson( leftArmy_ );
    j["right_army"] = armyToJson( rightArmy_ );

    std::ofstream out( filepath );
    if ( ! out.is_open( ) ) {
        std::cerr << "Settings: could not open '" << filepath << "' for writing." << std::endl;
        return;
    }
    out << j.dump( 4 );
}

} // namespace core
