/**
 * @file BoardTest.cc
 * @brief Unit tests for the hex Board grid construction and lookup.
 */
#include "../Board.h"
#include <gtest/gtest.h>

namespace test {

using models::Board;
using models::Hex;

TEST( BoardTest, InitializationCoordinates ) {
    Board board;

    Hex& hex_0_0 = board.getHexAtOffset( 0, 0 );
    EXPECT_EQ( hex_0_0.getQ( ), 0 );
    EXPECT_EQ( hex_0_0.getR( ), 0 );
    EXPECT_EQ( hex_0_0.getS( ), 0 );

    Hex& hex_0_1 = board.getHexAtOffset( 0, 1 );
    EXPECT_EQ( hex_0_1.getQ( ), 0 );
    EXPECT_EQ( hex_0_1.getR( ), 1 );
    EXPECT_EQ( hex_0_1.getS( ), -1 );

    Hex& hex_0_2 = board.getHexAtOffset( 0, 2 );
    EXPECT_EQ( hex_0_2.getQ( ), -1 );
    EXPECT_EQ( hex_0_2.getR( ), 2 );
    EXPECT_EQ( hex_0_2.getS( ), -1 );

    Hex& hex_14_10 = board.getHexAtOffset( 14, 10 );
    EXPECT_EQ( hex_14_10.getQ( ), 9 );
    EXPECT_EQ( hex_14_10.getR( ), 10 );
    EXPECT_EQ( hex_14_10.getS( ), -19 );
}

TEST( BoardTest, GetHexByCubeCoordinates ) {
    Board board;

    Hex& hex = board.getHex( 9, 10, -19 );
    EXPECT_EQ( hex.getQ( ), 9 );
    EXPECT_EQ( hex.getR( ), 10 );
    EXPECT_EQ( hex.getS( ), -19 );

    Hex& origin = board.getHex( 0, 0, 0 );
    EXPECT_EQ( origin.getQ( ), 0 );
    EXPECT_EQ( origin.getR( ), 0 );
    EXPECT_EQ( origin.getS( ), 0 );
}

TEST( BoardTest, GetHexThrowsOnInvalidCoordinates ) {
    Board board;

    EXPECT_THROW( board.getHex( 1, 2, 3 ), std::out_of_range );

    EXPECT_THROW( board.getHex( -100, 50, 50 ), std::out_of_range );

    EXPECT_THROW( board.getHex( 15, 0, -15 ), std::out_of_range );
}

} // namespace test
