/**
 * @file UnitFactory.cc
 * @brief Implementation of the data-driven Unit / RangeUnit factory.
 * @author Lukasz Szydlik
 */
#include <fstream>
#include <stdexcept>

#include "UnitFactory.h"

namespace models {

std::unordered_map<UnitID, nlohmann::json> UnitFactory::UnitData;

namespace {

constexpr const char* K_UNIT_NAMES[] = {
    "Pikeman",      "Halberdier",  "Archer",    "Marksman",    "Griffin",
    "RoyalGriffin", "Swordsman",   "Crusader",  "Monk",        "Zealot",
    "Cavalier",     "Champion",    "Angel",     "Archangel",   "Imp",
    "Familiar",     "Gog",         "Magog",     "HellHound",   "Cerberus",
    "Demon",        "HornedDemon", "PitFiend",  "PitLord",     "Efreet",
    "EfreetSultan", "Devil",       "ArchDevil", "Skeleton",    "SkeletonWarrior",
    "WalkingDead",  "Zombie",      "Wight",     "Wraith",      "Vampire",
    "VampireLord",  "Lich",        "PowerLich", "BlackKnight", "DreadKnight",
    "BoneDragon",   "GhostDragon"
};
constexpr int K_UNIT_COUNT = sizeof( K_UNIT_NAMES ) / sizeof( K_UNIT_NAMES[0] );

} // namespace

std::string UnitFactory::unitIdToString( UnitID id ) {
    int index = static_cast<int>( id );
    if ( index >= 0 && index < K_UNIT_COUNT ) {
        return K_UNIT_NAMES[index];
    }
    throw std::invalid_argument( "Unknown UnitID" );
}

std::string UnitFactory::idToString( UnitID id ) {
    return unitIdToString( id );
}

std::optional<UnitID> UnitFactory::idFromString( const std::string& name ) {
    for ( int i = 0; i < K_UNIT_COUNT; ++i ) {
        if ( name == K_UNIT_NAMES[i] ) {
            return static_cast<UnitID>( i );
        }
    }
    return std::nullopt;
}

std::optional<PortraitRect> UnitFactory::getPortraitRect( UnitID id ) {
    auto it = UnitData.find( id );
    if ( it == UnitData.end( ) ) {
        return std::nullopt;
    }
    const auto& data = it->second;
    if ( ! data.contains( "portrait" ) || ! data["portrait"].is_object( ) ) {
        return std::nullopt;
    }
    const auto& rect_json = data["portrait"];
    if ( ! rect_json.contains( "x" ) || ! rect_json.contains( "y" ) ||
         ! rect_json.contains( "w" ) || ! rect_json.contains( "h" ) ) {
        return std::nullopt;
    }
    PortraitRect rect;
    rect.x_ = rect_json["x"].get<int>( );
    rect.y_ = rect_json["y"].get<int>( );
    rect.w_ = rect_json["w"].get<int>( );
    rect.h_ = rect_json["h"].get<int>( );
    return rect;
}

void UnitFactory::init( const std::string& filepath ) {
    std::ifstream file( filepath );
    if ( ! file.is_open( ) ) {
        throw std::runtime_error( "UnitFactory: could not open " + filepath );
    }

    nlohmann::json j;
    file >> j;

    UnitData.clear( );
    for ( int i = 0; i < 42; ++i ) {
        UnitID id = static_cast<UnitID>( i );
        std::string key = unitIdToString( id );
        if ( j.contains( key ) ) {
            UnitData[id] = j[key];
        }
    }
}

std::shared_ptr<Unit> UnitFactory::createUnit( UnitID id, int count ) {
    auto it = UnitData.find( id );
    if ( it == UnitData.end( ) ) {
        throw std::runtime_error( "UnitFactory: JSON data not loaded or unit not found: " +
                                  unitIdToString( id ) );
    }

    const auto& data = it->second;
    std::string name = unitIdToString( id );

    try {
        int tier = data.at( "tier" ).get<int>( );
        int attack = data.at( "attack" ).get<int>( );
        int defense = data.at( "defense" ).get<int>( );
        int health = data.at( "health" ).get<int>( );

        int damage_min = data.at( "damage_min" ).get<int>( );
        int damage_max = data.at( "damage_max" ).get<int>( );

        int speed = data.at( "speed" ).get<int>( );
        int size = data.value( "size", 1 );
        bool is_teleporter = data.value( "is_teleporter", false );
        bool is_flying = data.value( "is_flying", false );
        std::string asset_filename = data.value( "asset_filename", "" );
        std::string description = data.value( "description", "" );

        std::shared_ptr<Unit> unit;
        if ( data.contains( "shoots" ) ) {
            int shoots = data.at( "shoots" ).get<int>( );
            unit = std::make_shared<RangeUnit>( name,
                                                tier,
                                                attack,
                                                defense,
                                                health,
                                                damage_min,
                                                damage_max,
                                                speed,
                                                count,
                                                shoots,
                                                asset_filename,
                                                description );
        } else {
            unit = std::make_shared<Unit>( name,
                                           tier,
                                           attack,
                                           defense,
                                           health,
                                           damage_min,
                                           damage_max,
                                           speed,
                                           count,
                                           asset_filename,
                                           description );
        }
        unit->setSize( size );
        unit->setIsTeleporter( is_teleporter );
        unit->setIsFlying( is_flying );
        return unit;
    } catch ( const std::exception& e ) {
        throw std::runtime_error( "UnitFactory: missing or invalid field for unit " + name + " (" +
                                  e.what( ) + ")" );
    }
}

} // namespace models
