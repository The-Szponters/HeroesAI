/**
 * @file Hex.h
 * @brief Single battlefield cell using cube (q, r, s) coordinates.
 * @author Dominik Śledziewski
 */
#pragma once
#include <algorithm>
#include <memory>
#include <stdexcept>

#include "Unit.h"

namespace models {

/**
 * @brief One hex tile on the battlefield.
 *
 * Stores its immutable cube coordinates, an optional weak reference
 * to the Unit currently standing on it, and a list of dead units
 * that have fallen on this tile.
 */
class Hex {
public:
    Hex( int q, int r, int s ) : Q( q ), R( r ), S( s ) {
        if ( ! isValid( ) ) {
            throw std::invalid_argument( "q + r + s must sum to 0" );
        }
    }
    Hex( int q, int r, int s, std::weak_ptr<Unit> unit )
        : Q( q ), R( r ), S( s ), unit_( std::move( unit ) ) {
        if ( ! isValid( ) ) {
            throw std::invalid_argument( "q + r + s must sum to 0" );
        }
    }
    Hex( const Hex& other ) = default;
    ~Hex( ) = default;

    const int getQ( ) const { return Q; }
    const int getR( ) const { return R; }
    const int getS( ) const { return S; }

    std::shared_ptr<Unit> getUnit( ) const {
        auto shared_unit = unit_.lock( );
        if ( ! shared_unit ) {
            throw std::runtime_error( "Hex does not contain a unit" );
        }
        return shared_unit;
    }

    bool operator==( const Hex& other ) const {
        return Q == other.Q && R == other.R && S == other.S;
    }
    const int distanceTo( const Hex& other ) const {
        return std::max(
            { std::abs( Q - other.Q ), std::abs( R - other.R ), std::abs( S - other.S ) } );
    }

    bool hasUnit( ) const { return ! unit_.expired( ); }

    void setUnit( std::weak_ptr<Unit> new_unit ) { unit_ = std::move( new_unit ); }

    void removeUnit( ) { unit_.reset( ); }

    void unitDied( ) {
        if ( ! unit_.expired( ) ) {
            deadUnits_.push_back( unit_ );
            unit_.reset( );
        }
    }

    const std::vector<std::weak_ptr<Unit>>& getDeadUnits( ) const { return deadUnits_; }

private:
    const int Q;
    const int R;
    const int S;
    std::weak_ptr<Unit> unit_;
    std::vector<std::weak_ptr<Unit>> deadUnits_;
    bool isValid( ) const { return Q + R + S == 0; }
};

} // namespace models