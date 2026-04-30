/**
 * @file ModelsTest.cc
 * @brief Unit tests for Hex, Unit, RangeUnit and the buff system.
 */
#include <gtest/gtest.h>
#include "../Hex.h"
#include "../Unit.h"
#include "../RangeUnit.h"

namespace test {

using models::Buff;
using models::BuffFactory;
using models::BuffType;
using models::Hex;
using models::RangeUnit;
using models::Unit;

TEST(HexTest, InitializationValid ){
    Hex h(1, -1, 0 );
    EXPECT_EQ(h.get_q(), 1 );
    EXPECT_EQ(h.get_r(), -1 );
    EXPECT_EQ(h.get_s(), 0 );
}

TEST(HexTest, InitializationInvalidThrows ){
    EXPECT_THROW(Hex(1, 1, 1), std::invalid_argument );
}

TEST(HexTest, EqualityOperator ){
    Hex h1(1, -1, 0 );
    Hex h2(1, -1, 0 );
    Hex h3(0, 1, -1 );
    EXPECT_TRUE(h1 == h2 );
    EXPECT_FALSE(h1 == h3 );
}

TEST(HexTest, DistanceOperator ){
    Hex h1(0, 0, 0 );
    Hex h2(1, 0, -1 );
    Hex h3(-2, 1, 1 );

    EXPECT_EQ(h1.distance_to(h2), 1 );
    EXPECT_EQ(h1.distance_to(h3), 2 );
    EXPECT_EQ(h2.distance_to(h3), 3 );
}

TEST(HexTest, InitializationWithUnit ){
    auto u = std::make_shared<Unit>("Warrior", 2, 10, 5, 100, 10, 15, 3, 5 );
    Hex h1(0, 0, 0, u );
    EXPECT_EQ(h1.get_q(), 0 );
    EXPECT_EQ(h1.get_r(), 0 );
    EXPECT_EQ(h1.get_s(), 0 );
    EXPECT_EQ(h1.get_unit()->get_name(), "Warrior" );
    EXPECT_EQ(h1.get_unit()->get_tier(), 2 );
    EXPECT_EQ(h1.get_unit()->get_attack(), 10 );
    EXPECT_EQ(h1.get_unit()->get_defense(), 5 );
    EXPECT_EQ(h1.get_unit()->get_health(), 100 );
    EXPECT_EQ(h1.get_unit()->get_damage_min(), 10 );
    EXPECT_EQ(h1.get_unit()->get_damage_max(), 15 );
    EXPECT_EQ(h1.get_unit()->get_speed(), 3 );
    EXPECT_EQ(h1.get_unit()->get_count(), 5 );
}

TEST(UnitTest, Initialization ){
    Unit u("Warrior", 2, 10, 5, 100, 10, 15, 3, 5 );
    EXPECT_EQ(u.get_name(), "Warrior" );
    EXPECT_EQ(u.get_tier(), 2 );
    EXPECT_EQ(u.get_attack(), 10 );
    EXPECT_EQ(u.get_defense(), 5 );
    EXPECT_EQ(u.get_health(), 100 );
    EXPECT_EQ(u.get_damage_min(), 10 );
    EXPECT_EQ(u.get_damage_max(), 15 );
    EXPECT_EQ(u.get_speed(), 3 );

    EXPECT_EQ(u.get_count(), 5 );
    EXPECT_EQ(u.get_health_left(), 100 );

    EXPECT_EQ(u.get_q(), 0 );
    EXPECT_EQ(u.get_r(), 0 );
    EXPECT_EQ(u.get_s(), 0 );
}

TEST(UnitTest, SetPosition ){
    Unit u;
    u.set_position(1, -1, 0 );
    EXPECT_EQ(u.get_q(), 1 );
    EXPECT_EQ(u.get_r(), -1 );
    EXPECT_EQ(u.get_s(), 0 );
}

TEST(UnitTest, TakeDamageLessThanHealthLeft ){
    Unit u("Warrior", 2, 10, 5, 100, 10, 15, 3, 5 );
    u.take_damage(20 );
    EXPECT_EQ(u.get_count(), 5 );
    EXPECT_EQ(u.get_health_left(), 80 );
}

TEST(UnitTest, TakeDamageExactlyOneUnit ){
    Unit u("Warrior", 2, 10, 5, 100, 10, 15, 3, 5 );
    u.take_damage(100 );
    EXPECT_EQ(u.get_count(), 4 );
    EXPECT_EQ(u.get_health_left(), 100 );
}

TEST(UnitTest, TakeDamageMultipleUnits ){
    Unit u("Warrior", 2, 10, 5, 100, 10, 15, 3, 5 );
    u.take_damage(230 );
    EXPECT_EQ(u.get_count(), 3 );
    EXPECT_EQ(u.get_health_left(), 70 );
}

TEST(UnitTest, TakeDamageMoreThanTotalHealth ){
    Unit u("Warrior", 2, 10, 5, 100, 10, 15, 3, 5 );
    u.take_damage(1000 );
    EXPECT_EQ(u.get_count(), 0 );
    EXPECT_EQ(u.get_health_left(), 0 );
}

TEST(UnitTest, BuffSystemDefend ){
    Unit u("Warrior", 2, 10, 5, 100, 10, 15, 6, 5 );
    EXPECT_EQ(u.get_defense(), 5 );

    Buff b = BuffFactory::create_defend_buff( );
    u.apply_buff(b );
    EXPECT_EQ(u.get_defense(), 10 );

    u.remove_buff(BuffType::DEFEND );
    EXPECT_EQ(u.get_defense(), 5 );
}

TEST(UnitTest, BuffSystemSlowPercentage ){
    Unit u("Warrior", 2, 10, 5, 100, 10, 15, 6, 5 );
    EXPECT_EQ(u.get_speed(), 6 );

    Buff b = BuffFactory::create_slow_buff( );
    u.apply_buff(b );
    EXPECT_EQ(u.get_speed(), 3 );

    u.remove_buff(BuffType::SLOW );
    EXPECT_EQ(u.get_speed(), 6 );
}

TEST(UnitTest, BuffSystemBlindHardOverride ){
    Unit u("Warrior", 2, 10, 5, 100, 10, 15, 6, 5 );
    EXPECT_EQ(u.get_speed(), 6 );

    Buff b = BuffFactory::create_blind_buff( );
    u.apply_buff(b );
    EXPECT_EQ(u.get_speed(), 0 );

    u.remove_buff(BuffType::BLIND );
    EXPECT_EQ(u.get_speed(), 6 );
}

TEST(UnitTest, BuffSystemTurnTick ){
    Unit u("Warrior", 2, 10, 5, 100, 10, 15, 6, 5 );
    Buff b = BuffFactory::create_defend_buff( );
    u.apply_buff(b );
    EXPECT_EQ(u.get_defense(), 10 );

    u.on_turn_start( );
    EXPECT_EQ(u.get_defense(), 5 );
}

TEST(UnitTest, BuffSystemStatClamping ){
    Unit u("Warrior", 2, 10, 5, 100, 10, 15, 6, 5 );

    Buff b;
    b.type = BuffType::DEFEND;
    b.duration = 1;
    b.modify_defense = [](int d ){ return d - 100; };

    u.apply_buff(b );
    EXPECT_EQ(u.get_defense(), 0 );
}

TEST(RangeUnitTest, Initialization ){
    RangeUnit ru("Archer", 3, 8, 3, 50, 10, 12, 4, 10, 12 );
    EXPECT_EQ(ru.get_name(), "Archer" );
    EXPECT_EQ(ru.get_tier(), 3 );
    EXPECT_EQ(ru.get_attack(), 8 );
    EXPECT_EQ(ru.get_damage_min(), 10 );
    EXPECT_EQ(ru.get_damage_max(), 12 );
    EXPECT_EQ(ru.get_shoots(), 12 );
    EXPECT_EQ(ru.get_count(), 10 );
}

}  // namespace test
