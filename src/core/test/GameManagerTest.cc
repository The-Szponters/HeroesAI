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
    Hero blue;
    Hero red;

    void SetUp( ) override {
        auto u1 = std::make_shared<Unit>( "BlueUnit", 1, 10, 5, 50, 5, 10, 2, 2 );
        u1->set_position( 0, 0, 0 );
        blue = Hero( "BlueHero", 0, 0, 0, 0 );
        blue.get_army( ).add_unit( u1 );

        auto u2 = std::make_shared<Unit>( "RedUnit", 1, 5, 5, 50, 2, 5, 1, 2 );
        u2->set_position( 1, 0, -1 );
        red = Hero( "RedHero", 0, 0, 0, 0 );
        red.get_army( ).add_unit( u2 );
    }
};

TEST_F( GameManagerTest, Initialization ) {
    GameManager gm( blue, red );
    EXPECT_NE( gm.get_current_unit( ), nullptr );
}

TEST_F( GameManagerTest, AttackRemovesDeadUnit ) {
    auto u1 = std::make_shared<Unit>( "BlueKiller", 1, 200, 5, 50, 1000, 1000, 9, 1 );
    u1->set_position( 0, 0, 0 );
    blue.get_army( ).remove_unit( 0 );
    blue.get_army( ).add_unit( u1 );

    GameManager gm( blue, red );

    Unit* current = gm.get_current_unit( );
    ASSERT_EQ( current->get_name( ), "BlueKiller" );

    Unit* redUnit = gm.get_red_hero( ).get_army( ).get_units( )[0].get( );
    Hex placeholder_hex( 0, 0, 0 );
    gm.attack( *current, *redUnit, placeholder_hex );

    EXPECT_EQ( gm.get_red_hero( ).get_army( ).get_units( ).size( ), 1 );

    std::vector<Unit*> units_left = gm.get_units_left_in_round( );
    EXPECT_EQ( std::find( units_left.begin( ), units_left.end( ), redUnit ), units_left.end( ) );
}

} // namespace test
