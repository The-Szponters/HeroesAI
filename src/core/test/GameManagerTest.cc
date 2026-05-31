/**
 * @file GameManagerTest.cc
 * @brief Unit tests for the GameManager battle facade.
 */
#include "../GameManager.h"
#include "../../models/Army.h"
#include "../../models/Board.h"
#include "../../models/Hero.h"
#include "../../models/Unit.h"
#include <gtest/gtest.h>

namespace test {

using core::GameManager;
using models::Army;
using models::Board;
using models::Hero;
using models::Hex;
using models::Unit;

/**
 * @brief Shared test fixture for GameManager behavior.
 */
class GameManagerTest : public ::testing::Test {
protected:
    Hero blue_;
    Hero red_;

    void SetUp( ) override {
        auto u1 = std::make_shared<Unit>( "BlueUnit", 1, 10, 5, 50, 5, 10, 2, 2 );
        u1->setPosition( 0, 0, 0 );
        blue_ = Hero( "BlueHero", 0, 0, 0, 0 );
        blue_.getArmy( ).addUnit( u1 );

        auto u2 = std::make_shared<Unit>( "RedUnit", 1, 5, 5, 50, 2, 5, 1, 2 );
        u2->setPosition( 1, 0, -1 );
        red_ = Hero( "RedHero", 0, 0, 0, 0 );
        red_.getArmy( ).addUnit( u2 );
    }
};

TEST_F( GameManagerTest, Initialization ) {
    GameManager gm( blue_, red_ );
    EXPECT_NE( gm.getCurrentUnit( ), nullptr );
}

TEST_F( GameManagerTest, AttackRemovesDeadUnit ) {
    auto u1 = std::make_shared<Unit>( "BlueKiller", 1, 200, 5, 50, 1000, 1000, 9, 1 );
    u1->setPosition( 0, 0, 0 );
    blue_.getArmy( ).removeUnit( 0 );
    blue_.getArmy( ).addUnit( u1 );

    GameManager gm( blue_, red_ );

    Unit* current = gm.getCurrentUnit( );
    ASSERT_EQ( current->getName( ), "BlueKiller" );

    Unit* red_unit = gm.getRedHero( ).getArmy( ).getUnits( )[0].get( );
    Hex placeholder_hex( 0, 0, 0 );
    gm.attack( *current, *red_unit, placeholder_hex );

    EXPECT_EQ( gm.getRedHero( ).getArmy( ).getUnits( ).size( ), 1 );

    std::vector<Unit*> units_left = gm.getUnitsLeftInRound( );
    EXPECT_EQ( std::find( units_left.begin( ), units_left.end( ), red_unit ), units_left.end( ) );
}

TEST( GameManagerHeroBonusTest, BoostsStacks ) {
    auto blue_unit = std::make_shared<Unit>( "BlueUnit", 1, 10, 5, 50, 5, 10, 6, 3 );
    blue_unit->setPosition( 0, 0, 0 );
    Hero blue( "Blue", 4, 2, 0, 0 ); // hero attack +4, defense +2
    blue.getArmy( ).addUnit( blue_unit );

    auto red_unit = std::make_shared<Unit>( "RedUnit", 1, 5, 5, 50, 2, 5, 1, 3 );
    red_unit->setPosition( 1, 0, -1 );
    Hero red( "Red", 0, 0, 0, 0 ); // no hero skill
    red.getArmy( ).addUnit( red_unit );

    GameManager gm( blue, red );

    Unit* b = gm.getBlueHero( ).getArmy( ).getUnits( )[0].get( );
    EXPECT_EQ( b->getBaseAttack( ), 10 );        // intrinsic base unchanged
    EXPECT_EQ( b->getAttack( ), 14 );            // base + hero attack
    EXPECT_EQ( b->getBaseDefense( ), 5 );
    EXPECT_EQ( b->getDefense( ), 7 );            // base + hero defense

    Unit* r = gm.getRedHero( ).getArmy( ).getUnits( )[0].get( );
    EXPECT_EQ( r->getAttack( ), r->getBaseAttack( ) ); // red hero grants nothing
}

} // namespace test
