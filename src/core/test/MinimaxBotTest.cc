/**
 * @file MinimaxBotTest.cc
 * @brief Tests for GameManager::clone independence and the Minimax bot.
 * @author Lukasz Szydlik
 */
#include "../GameManager.h"
#include "../MinimaxBotService.h"
#include "../../models/Hero.h"
#include "../../models/RangeUnit.h"
#include "../../models/Unit.h"
#include <gtest/gtest.h>
#include <memory>

namespace test {

using core::GameManager;
using core::MinimaxBotService;
using models::Hero;
using models::Hex;
using models::Unit;

namespace {

Hero makeBlue( ) {
    auto u = std::make_shared<Unit>( "BlueUnit", 1, 10, 5, 50, 5, 10, 4, 5 );
    u->setPosition( 0, 0, 0 );
    Hero h( "BlueHero", 0, 0, 0, 0 );
    h.getArmy( ).addUnit( u );
    return h;
}

Hero makeRed( ) {
    auto u = std::make_shared<Unit>( "RedUnit", 1, 5, 5, 50, 2, 5, 1, 5 );
    u->setPosition( 1, 0, -1 );
    Hero h( "RedHero", 0, 0, 0, 0 );
    h.getArmy( ).addUnit( u );
    return h;
}

// A lone, fragile red stack adjacent to blue: blue one-shots it.
Hero makeWeakRed( ) {
    auto u = std::make_shared<Unit>( "WeakRed", 1, 1, 1, 5, 1, 1, 1, 1 );
    u->setPosition( 1, 0, -1 );
    Hero h( "RedHero", 0, 0, 0, 0 );
    h.getArmy( ).addUnit( u );
    return h;
}

} // namespace

TEST( CloneTest, ProducesIndependentDeepCopy ) {
    GameManager gm( makeBlue( ), makeRed( ) );
    std::unique_ptr<GameManager> copy = gm.clone( );

    ASSERT_EQ( copy->getAllUnits( ).size( ), gm.getAllUnits( ).size( ) );

    Unit* original_blue = gm.getBlueHero( ).getArmy( ).getUnits( )[0].get( );
    Unit* cloned_blue = copy->getBlueHero( ).getArmy( ).getUnits( )[0].get( );

    // Deep copy: different objects, same state.
    EXPECT_NE( original_blue, cloned_blue );
    EXPECT_EQ( cloned_blue->getCount( ), original_blue->getCount( ) );
    EXPECT_EQ( cloned_blue->getQ( ), original_blue->getQ( ) );

    // The clone's board references the clone's unit, not the original's.
    const Hex& cloned_hex = copy->getBoard( ).getHex( 0, 0, 0 );
    ASSERT_TRUE( cloned_hex.hasUnit( ) );
    EXPECT_EQ( cloned_hex.getUnit( ).get( ), cloned_blue );
}

TEST( CloneTest, MutatingCloneLeavesOriginalUntouched ) {
    GameManager gm( makeBlue( ), makeRed( ) );
    Unit* original_red = gm.getRedHero( ).getArmy( ).getUnits( )[0].get( );
    const int red_count_before = original_red->getCount( );

    std::unique_ptr<GameManager> copy = gm.clone( );

    // Kill the red stack in the clone via a huge attack.
    Unit* clone_blue = copy->getCurrentUnit( );
    ASSERT_NE( clone_blue, nullptr );
    Unit* clone_red = copy->getRedHero( ).getArmy( ).getUnits( )[0].get( );
    Hex& approach = copy->getBoard( ).getHex( clone_blue->getQ( ), clone_blue->getR( ),
                                                  clone_blue->getS( ) );
    copy->attack( *clone_blue, *clone_red, approach );

    // Original red stack is unaffected by the cloned battle.
    EXPECT_EQ( original_red->getCount( ), red_count_before );
}

TEST( MinimaxBotTest, ReturnsAnActionForLiveUnit ) {
    GameManager gm( makeBlue( ), makeRed( ) );
    MinimaxBotService bot( gm, 3 );

    Unit* active = gm.getCurrentUnit( );
    ASSERT_NE( active, nullptr );

    const auto command = bot.decideAction( *active );
    EXPECT_TRUE( command.has_value( ) );
}

TEST( MinimaxBotTest, AttacksToWinWhenItCanWipeTheEnemy ) {
    // Blue can one-shot the lone red stack, which wipes the enemy army --
    // an unambiguous win the search must take by attacking it.
    GameManager gm( makeBlue( ), makeWeakRed( ) );
    MinimaxBotService bot( gm, 3 );

    Unit* active = gm.getCurrentUnit( );
    ASSERT_NE( active, nullptr );
    Unit* red = gm.getRedHero( ).getArmy( ).getUnits( )[0].get( );

    const auto command = bot.decideAction( *active );
    ASSERT_TRUE( command.has_value( ) );
    EXPECT_EQ( command->type_, core::ActionType::MELEE_ATTACK );
    EXPECT_EQ( command->target_, red );
}

TEST( MinimaxBotTest, ShootsInsteadOfDefending ) {
    // A shooter whose only enemy is too far to melee should fire, not
    // defend. (Regression: the evaluator must not reward Defend's temporary
    // +5 defense by inflating the unit's material value.)
    auto shooter = std::make_shared<models::RangeUnit>( "Archer", 1, 10, 5, 50, 5, 10, 4, 10, 12 );
    shooter->setPosition( 0, 0, 0 );
    Hero blue( "BlueHero", 0, 0, 0, 0 );
    blue.getArmy( ).addUnit( shooter );

    auto enemy = std::make_shared<Unit>( "Target", 1, 5, 5, 50, 2, 5, 1, 10 );
    enemy->setPosition( 6, 0, -6 ); // distance 6: shooter (speed 4) can't reach for melee
    Hero red( "RedHero", 0, 0, 0, 0 );
    red.getArmy( ).addUnit( enemy );

    GameManager gm( blue, red );
    MinimaxBotService bot( gm, 3 );

    Unit* active = gm.getCurrentUnit( );
    ASSERT_NE( active, nullptr );
    ASSERT_TRUE( active->isRanged( ) );

    const auto command = bot.decideAction( *active );
    ASSERT_TRUE( command.has_value( ) );
    EXPECT_EQ( command->type_, core::ActionType::RANGED_ATTACK );
}

TEST( MinimaxBotTest, DecisionIsDeterministic ) {
    // decideAction does not mutate the live model, and the search uses
    // expected (not random) damage, so repeated calls on the same position
    // must yield the identical action.
    GameManager gm( makeBlue( ), makeRed( ) );
    MinimaxBotService bot( gm, 3 );

    Unit* active = gm.getCurrentUnit( );
    ASSERT_NE( active, nullptr );

    const auto a = bot.decideAction( *active );
    const auto b = bot.decideAction( *active );
    const auto c = bot.decideAction( *active );
    ASSERT_TRUE( a.has_value( ) && b.has_value( ) && c.has_value( ) );
    EXPECT_EQ( a->type_, b->type_ );
    EXPECT_EQ( a->type_, c->type_ );
    EXPECT_EQ( a->target_, b->target_ );
    EXPECT_EQ( a->destQ_, b->destQ_ );
    EXPECT_EQ( a->destR_, b->destR_ );
}

} // namespace test
