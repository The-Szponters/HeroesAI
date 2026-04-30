/**
 * @file Unit.h
 * @brief Stack of identical creatures that fights as a single entity.
 * @author Dominik Śledziewski & Łukasz Szydlik
 */
#pragma once
#include <algorithm>
#include <string>
#include <vector>

#include "Buff.h"

namespace models {

/**
 * @brief A stack of identical creatures.
 *
 * Encapsulates the immutable base stats, the position on the board,
 * the active buff list (which derives the effective combat stats),
 * and rules for taking damage and consuming retaliations.
 * Specialised by RangeUnit for ranged shooters.
 */
class Unit {
public:
    Unit( ) = default;
    Unit( std::string name,
          int tier,
          int attack,
          int defense,
          int health,
          int damage_min,
          int damage_max,
          int speed,
          int count,
          std::string asset_filename = "",
          std::string description = "" )
        : name_( std::move( name ) ),
          tier_( tier ),
          attack_( attack ),
          defense_( defense ),
          health_( health ),
          damageMin_( damage_min ),
          damageMax_( damage_max ),
          speed_( speed ),
          count_( count ),
          healthLeft_( health ),
          totalAttack_( attack ),
          totalDefense_( defense ),
          totalDamageMin_( damage_min ),
          totalDamageMax_( damage_max ),
          totalSpeed_( speed ),
          assetFilename_( std::move( asset_filename ) ),
          description_( std::move( description ) ) {}
    virtual ~Unit( ) = default;

    const std::string& getName( ) const { return name_; }
    int getTier( ) const { return tier_; }
    int getAttack( ) const { return totalAttack_; }
    int getDefense( ) const { return totalDefense_; }
    int getHealth( ) const { return health_; }
    int getDamageMin( ) const { return totalDamageMin_; }
    int getDamageMax( ) const { return totalDamageMax_; }
    int getSpeed( ) const { return totalSpeed_; }
    int getCount( ) const { return count_; }
    int getHealthLeft( ) const { return healthLeft_; }
    int getQ( ) const { return q_; }
    int getR( ) const { return r_; }
    int getS( ) const { return s_; }

    int getBaseAttack( ) const { return attack_; }
    int getBaseDefense( ) const { return defense_; }
    int getBaseSpeed( ) const { return speed_; }
    int getBaseDamageMin( ) const { return damageMin_; }
    int getBaseDamageMax( ) const { return damageMax_; }
    const std::string& getAssetFilename( ) const { return assetFilename_; }
    const std::string& getDescription( ) const { return description_; }

    int getSize( ) const { return size_; }
    void setSize( int s ) { size_ = ( s == 2 ? 2 : 1 ); }
    bool isTeleporterUnit( ) const { return isTeleporter_; }
    void setIsTeleporter( bool value ) { isTeleporter_ = value; }
    bool isFlyingUnit( ) const { return isFlying_; }
    void setIsFlying( bool value ) { isFlying_ = value; }

    bool ignoresPathBlockers( ) const { return isFlying_ || isTeleporter_; }

    bool hasRetaliatedThisRound( ) const { return hasRetaliated_; }
    void setRetaliated( bool v ) { hasRetaliated_ = v; }

    virtual bool isRanged( ) const { return false; }
    virtual int getAmmo( ) const { return 0; }
    virtual int getMaxAmmo( ) const { return 0; }
    virtual int getMaxRangeDamage( ) const { return 0; }
    virtual void decrementAmmo( ) {}

    virtual const std::string& getProjectileAsset( ) const {
        static const std::string EMPTY;
        return EMPTY;
    }

    bool isFacingLeft( ) const { return logicalFacingLeft_; }

    bool getVisualFacingLeft( ) const { return visualFacingLeft_; }
    void setVisualFacingLeft( bool value ) { visualFacingLeft_ = value; }

    void setPosition( int new_q, int new_r, int new_s ) {
        if ( ! positionInitialized_ ) {
            logicalFacingLeft_ = ( new_q >= 7 );
            visualFacingLeft_ = ( new_q >= 7 );
            positionInitialized_ = true;
        } else if ( new_q != q_ ) {
            visualFacingLeft_ = ( new_q < q_ );
        }
        q_ = new_q;
        r_ = new_r;
        s_ = new_s;
    }

    void takeDamage( int damage ) {
        int total_health = healthLeft_ + ( count_ - 1 ) * health_;
        total_health -= damage;
        if ( total_health < 0 ) {
            total_health = 0;
        }

        count_ = ( total_health + health_ - 1 ) / health_;
        healthLeft_ = total_health % health_;
        if ( healthLeft_ == 0 && count_ > 0 ) {
            healthLeft_ = health_;
        }
    }

    void applyBuff( const Buff& buff ) {
        auto it = std::find_if( activeBuffs_.begin( ),
                                activeBuffs_.end( ),
                                [&buff]( const Buff& b ) { return b.type_ == buff.type_; } );
        if ( it != activeBuffs_.end( ) ) {
            *it = buff;
        } else {
            activeBuffs_.push_back( buff );
        }
        recalculateStats( );
    }

    void removeBuff( BuffType type ) {
        std::erase_if( activeBuffs_, [type]( const Buff& b ) { return b.type_ == type; } );
        recalculateStats( );
    }

    void recalculateStats( ) {
        totalAttack_ = attack_;
        totalDefense_ = defense_;
        totalDamageMin_ = damageMin_;
        totalDamageMax_ = damageMax_;
        totalSpeed_ = speed_;

        for ( const auto& buff : activeBuffs_ ) {
            totalAttack_ = buff.modifyAttack_( totalAttack_ );
            totalDefense_ = buff.modifyDefense_( totalDefense_ );
            totalDamageMin_ = buff.modifyDamageMin_( totalDamageMin_ );
            totalDamageMax_ = buff.modifyDamageMax_( totalDamageMax_ );
            totalSpeed_ = buff.modifySpeed_( totalSpeed_ );
        }

        totalAttack_ = std::max( 0, totalAttack_ );
        totalDefense_ = std::max( 0, totalDefense_ );
        totalDamageMin_ = std::max( 0, totalDamageMin_ );
        totalDamageMax_ = std::max( 0, totalDamageMax_ );
        totalSpeed_ = std::max( 0, totalSpeed_ );
    }

    void onTurnStart( ) {
        hasRetaliated_ = false;
        bool removed = false;
        for ( auto& buff : activeBuffs_ ) {
            buff.duration_--;
            if ( buff.duration_ <= 0 ) {
                removed = true;
            }
        }
        if ( removed ) {
            std::erase_if( activeBuffs_, []( const Buff& b ) { return b.duration_ <= 0; } );
            recalculateStats( );
        }
    }

private:
    std::string name_;
    int tier_ = 1;
    int attack_ = 1;
    int defense_ = 1;
    int health_ = 1;
    int healthLeft_ = 1;
    int damageMin_ = 1;
    int damageMax_ = 1;
    int speed_ = 1;
    int count_ = 1;
    int q_ = 0;
    int r_ = 0;
    int s_ = 0;

    int totalAttack_ = 1;
    int totalDefense_ = 1;
    int totalDamageMin_ = 1;
    int totalDamageMax_ = 1;
    int totalSpeed_ = 1;

    int size_ = 1;
    bool isTeleporter_ = false;
    bool isFlying_ = false;
    bool hasRetaliated_ = false;
    bool logicalFacingLeft_ = false;
    bool visualFacingLeft_ = false;
    bool positionInitialized_ = false;

    std::string assetFilename_;
    std::string description_;

    std::vector<Buff> activeBuffs_;
};

} // namespace models
