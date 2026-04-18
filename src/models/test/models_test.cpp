#include <gtest/gtest.h>
#include "../hex.hpp"
#include "../unit.hpp"
#include "../range_unit.hpp"

TEST(HexTest, InitializationValid) {
    Hex h(1, -1, 0);
    EXPECT_EQ(h.get_q(), 1);
    EXPECT_EQ(h.get_r(), -1);
    EXPECT_EQ(h.get_s(), 0);
}

TEST(HexTest, InitializationInvalidThrows) {
    EXPECT_THROW(Hex(1, 1, 1), std::invalid_argument);
}

TEST(HexTest, EqualityOperator) {
    Hex h1(1, -1, 0);
    Hex h2(1, -1, 0);
    Hex h3(0, 1, -1);
    EXPECT_TRUE(h1 == h2);
    EXPECT_FALSE(h1 == h3);
}

TEST(HexTest, DistanceOperator) {
    Hex h1(0, 0, 0);
    Hex h2(1, 0, -1);
    Hex h3(-2, 1, 1);
    
    EXPECT_EQ(h1 - h2, 1);
    EXPECT_EQ(h1 - h3, 2);
    EXPECT_EQ(h2 - h3, 3);
}

TEST(UnitTest, Initialization) {
    Unit u("Warrior", 2, 10, 5, 100, 15, 3, 1);
    EXPECT_EQ(u.get_name(), "Warrior");
    EXPECT_EQ(u.get_tier(), 2);
    EXPECT_EQ(u.get_attack(), 10);
    EXPECT_EQ(u.get_defense(), 5);
    EXPECT_EQ(u.get_health(), 100);
    EXPECT_EQ(u.get_damage(), 15);
    EXPECT_EQ(u.get_speed(), 3);
    EXPECT_EQ(u.get_size(), 1);
    EXPECT_EQ(u.get_health_left(), 100);
}

TEST(RangeUnitTest, Initialization) {
    RangeUnit ru("Archer", 3, 8, 3, 50, 12, 4, 1, 10);
    EXPECT_EQ(ru.get_name(), "Archer");
    EXPECT_EQ(ru.get_tier(), 3);
    EXPECT_EQ(ru.get_attack(), 8);
    EXPECT_EQ(ru.get_shoots(), 10);
}
