/**
 * @file RangeUnit.h
 * @brief Specialised Unit that can attack at range with limited ammo.
 * @author Łukasz Szydlik
 */
#pragma once
#include <algorithm>
#include <cctype>
#include <string>

#include "Unit.h"

namespace models {

/**
 * @brief A Unit that can shoot projectiles at a distance.
 *
 * Tracks an ammunition counter that is depleted on each ranged
 * attack and, once exhausted, forces the unit to fight in melee.
 * Knows the projectile sprite to spawn for visual feedback.
 */
class RangeUnit : public Unit {
public:
    RangeUnit( ) = default;
    RangeUnit( std::string name,
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
               std::string description = "" )
        : Unit( std::move( name ),
                tier,
                attack,
                defense,
                health,
                damage_min,
                damage_max,
                speed,
                count,
                asset_filename,
                std::move( description ) ),
          maxAmmo_( shoots ),
          ammo_( shoots ),
          projectileAsset_( inferProjectileAsset( asset_filename ) ) {}
    ~RangeUnit( ) override = default;

    bool isRanged( ) const override { return true; }
    int getAmmo( ) const override { return ammo_; }
    int getMaxAmmo( ) const override { return maxAmmo_; }
    int getMaxRangeDamage( ) const override { return getBaseDamageMax( ); }
    void decrementAmmo( ) override {
        if ( ammo_ > 0 ) {
            --ammo_;
        }
    }
    const std::string& getProjectileAsset( ) const override { return projectileAsset_; }

    int getShoots( ) const { return maxAmmo_; }

private:
    static std::string inferProjectileAsset( const std::string& unit_asset ) {
        auto iequals = []( const std::string& a, const char* b ) {
            const std::size_t n = std::char_traits<char>::length( b );
            if ( a.size( ) != n ) {
                return false;
            }
            for ( std::size_t i = 0; i < n; ++i ) {
                if ( std::tolower( static_cast<unsigned char>( a[i] ) ) !=
                     std::tolower( static_cast<unsigned char>( b[i] ) ) ) {
                    return false;
                }
            }
            return true;
        };

        if ( iequals( unit_asset, "CLCBOW.def" ) || iequals( unit_asset, "CHCBOW.def" ) ) {
            return "archer_shoot.def";
        }
        if ( iequals( unit_asset, "Cmonkk.def" ) || iequals( unit_asset, "Czealt.def" ) ) {
            return "zealot_shoot.def";
        }
        if ( iequals( unit_asset, "CGOG.def" ) || iequals( unit_asset, "CMAGOG.def" ) ) {
            return "gog_shoot.def";
        }
        if ( iequals( unit_asset, "CLICH.def" ) || iequals( unit_asset, "CPLICH.def" ) ) {
            return "lich_shoot.def";
        }
        return { };
    }

    int maxAmmo_ = 0;
    int ammo_ = 0;
    std::string projectileAsset_;
};

} // namespace models
