/**
 * @file BoardTest.cc
 * @brief Unit tests for the hex Board grid construction and lookup.
 */
#include <gtest/gtest.h>
#include "../Board.h"

namespace test {

using models::Board;
using models::Hex;

TEST(BoardTest, InitializationCoordinates ){
    Board board;

    Hex& hex_0_0 = board.get_hex_at_offset(0, 0 );
    EXPECT_EQ(hex_0_0.get_q(), 0 );
    EXPECT_EQ(hex_0_0.get_r(), 0 );
    EXPECT_EQ(hex_0_0.get_s(), 0 );

    Hex& hex_0_1 = board.get_hex_at_offset(0, 1 );
    EXPECT_EQ(hex_0_1.get_q(), 0 );
    EXPECT_EQ(hex_0_1.get_r(), 1 );
    EXPECT_EQ(hex_0_1.get_s(), -1 );

    Hex& hex_0_2 = board.get_hex_at_offset(0, 2 );
    EXPECT_EQ(hex_0_2.get_q(), -1 );
    EXPECT_EQ(hex_0_2.get_r(), 2 );
    EXPECT_EQ(hex_0_2.get_s(), -1 );

    Hex& hex_14_10 = board.get_hex_at_offset(14, 10 );
    EXPECT_EQ(hex_14_10.get_q(), 9 );
    EXPECT_EQ(hex_14_10.get_r(), 10 );
    EXPECT_EQ(hex_14_10.get_s(), -19 );
}

TEST(BoardTest, GetHexByCubeCoordinates ){
    Board board;

    Hex& hex = board.get_hex(9, 10, -19 );
    EXPECT_EQ(hex.get_q(), 9 );
    EXPECT_EQ(hex.get_r(), 10 );
    EXPECT_EQ(hex.get_s(), -19 );

    Hex& origin = board.get_hex(0, 0, 0 );
    EXPECT_EQ(origin.get_q(), 0 );
    EXPECT_EQ(origin.get_r(), 0 );
    EXPECT_EQ(origin.get_s(), 0 );
}

TEST(BoardTest, GetHexThrowsOnInvalidCoordinates ){
    Board board;

    EXPECT_THROW(board.get_hex(1, 2, 3), std::out_of_range );

    EXPECT_THROW(board.get_hex(-100, 50, 50), std::out_of_range );

    EXPECT_THROW(board.get_hex(15, 0, -15), std::out_of_range );
}

}  // namespace test
