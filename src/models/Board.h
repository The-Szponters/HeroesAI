/**
 * @file Board.h
 * @brief Battlefield grid built out of axial-coordinate hex cells.
 */
#pragma once
#include "Hex.h"
#include <vector>

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
        grid.reserve( WIDTH * HEIGHT );

        for ( int row = 0; row < HEIGHT; ++row ) {
            for ( int col = 0; col < WIDTH; ++col ) {
                int q = col - ( row - ( row & 1 ) ) / 2;
                int r = row;
                int s = -q - r;

                grid.emplace_back( q, r, s );
            }
        }
    }

    Hex& get_hex_at_offset( int col, int row ) { return grid[row * WIDTH + col]; }

    Hex& get_hex( int q, int r, int s ) {
        for ( Hex& hex : grid ) {
            if ( hex.get_q( ) == q && hex.get_r( ) == r && hex.get_s( ) == s ) {
                return hex;
            }
        }
        throw std::out_of_range( "Hex with given coordinates not found" );
    }

    const Hex& get_hex( int q, int r, int s ) const {
        for ( const Hex& hex : grid ) {
            if ( hex.get_q( ) == q && hex.get_r( ) == r && hex.get_s( ) == s ) {
                return hex;
            }
        }
        throw std::out_of_range( "Hex with given coordinates not found" );
    }

    const std::vector<Hex>& get_grid( ) const { return grid; }

private:
    std::vector<Hex> grid;
};

} // namespace models