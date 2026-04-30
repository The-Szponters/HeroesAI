/**
 * @file HeroTest.cc
 * @brief Unit tests for the Hero class and its army integration.
 */
#include <gtest/gtest.h>
#include "../Hero.h"

namespace test {

using models::Hero;
using models::Unit;

TEST(HeroTest, InitializationValid ){
    Hero hero("Hero", 1, 2, 3, 4 );
    EXPECT_EQ(hero.get_name(), "Hero" );
    EXPECT_EQ(hero.get_attack(), 1 );
    EXPECT_EQ(hero.get_defense(), 2 );
    EXPECT_EQ(hero.get_power(), 3 );
    EXPECT_EQ(hero.get_knowledge(), 4 );
}

TEST(HeroTest, ArmyAccess ){
    Hero hero("Hero", 1, 2, 3, 4 );
    EXPECT_EQ(hero.get_army().get_units().size(), 0 );

    auto u1 = std::make_shared<Unit>("Warrior", 1, 1, 1, 10, 1, 2, 4, 10 );
    hero.get_army().add_unit(u1 );
    EXPECT_EQ(hero.get_army().get_units().size(), 1 );
    EXPECT_EQ(hero.get_army().get_units()[0]->get_name(), "Warrior" );
}

}  // namespace test
