/**
 * @file HeroTest.cc
 * @brief Unit tests for the Hero class and its army integration.
 * @author Dominik Sledziewski
 */
#include "../Hero.h"
#include <gtest/gtest.h>

namespace test {

using models::Hero;
using models::Unit;

TEST( HeroTest, InitializationValid ) {
    Hero hero( "Hero", 1, 2, 3, 4 );
    EXPECT_EQ( hero.getName( ), "Hero" );
    EXPECT_EQ( hero.getAttack( ), 1 );
    EXPECT_EQ( hero.getDefense( ), 2 );
    EXPECT_EQ( hero.getPower( ), 3 );
    EXPECT_EQ( hero.getKnowledge( ), 4 );
}

TEST( HeroTest, ArmyAccess ) {
    Hero hero( "Hero", 1, 2, 3, 4 );
    EXPECT_EQ( hero.getArmy( ).getUnits( ).size( ), 0 );

    auto u1 = std::make_shared<Unit>( "Warrior", 1, 1, 1, 10, 1, 2, 4, 10 );
    hero.getArmy( ).addUnit( u1 );
    EXPECT_EQ( hero.getArmy( ).getUnits( ).size( ), 1 );
    EXPECT_EQ( hero.getArmy( ).getUnits( )[0]->getName( ), "Warrior" );
}

} // namespace test
