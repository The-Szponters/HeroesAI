/**
 * @file Board.h
 * @brief Battlefield grid built out of axial-coordinate hex cells.
 * @author Łukasz Szydlik
 */
#pragma once
#include <cstddef>
#include <vector>

#include "Hex.h"

namespace models {

/**
 * @brief Fixed-size 15-by-11 hex battlefield.
 *
 * Owns every Hex on the field and provides lookup by both offset
 * (column, row) coordinates and cube (q, r, s) coordinates.
 */
class Board {
public:
    static constexpr int WIDTH = 15;
    static constexpr int HEIGHT = 11;

    Board( ) {
        grid_.reserve( static_cast<std::size_t>( WIDTH * HEIGHT ) );

        for ( int row = 0; row < HEIGHT; ++row ) {
            for ( int col = 0; col < WIDTH; ++col ) {
                int q = col - ( row - ( row & 1 ) ) / 2;
                int r = row;
                int s = -q - r;

                grid_.emplace_back( q, r, s );
            }
        }
    }

    Hex& getHexAtOffset( int col, int row ) { return grid_[row * WIDTH + col]; }

    Hex& getHex( int q, int r, int s ) {
        for ( Hex& hex : grid_ ) {
            if ( hex.getQ( ) == q && hex.getR( ) == r && hex.getS( ) == s ) {
                return hex;
            }
        }
        throw std::out_of_range( "Hex with given coordinates not found" );
    }

    const Hex& getHex( int q, int r, int s ) const {
        for ( const Hex& hex : grid_ ) {
            if ( hex.getQ( ) == q && hex.getR( ) == r && hex.getS( ) == s ) {
                return hex;
            }
        }
        throw std::out_of_range( "Hex with given coordinates not found" );
    }

    const std::vector<Hex>& getGrid( ) const { return grid_; }

private:
    std::vector<Hex> grid_;
};

} // namespace models