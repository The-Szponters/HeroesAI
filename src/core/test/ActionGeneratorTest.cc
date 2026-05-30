/**
 * @file ActionGeneratorTest.cc
 * @brief Unit tests for the AI legal-action generator.
 */
#include "../ActionGenerator.h"
#include "../GameManager.h"
#include "../SpellResolver.h"
#include "../../models/Hero.h"
#include "../../models/Unit.h"
#include <gtest/gtest.h>
#include <memory>
#include <vector>

namespace test {

using core::ActionCommand;
using core::ActionGenerator;
using core::ActionType;
using core::GameManager;
using core::SpellResolver;
using models::Hero;
using models::Unit;

namespace {

int countType( const std::vector<ActionCommand>& actions, ActionType type ) {
    int n = 0;
    for ( const ActionCommand& a : actions ) {
        if ( a.type_ == type ) {
            ++n;
        }
    }
    return n;
}

bool hasMeleeAgainst( const std::vector<ActionCommand>& actions, const Unit* target ) {
    for ( const ActionCommand& a : actions ) {
        if ( a.type_ == ActionType::MELEE_ATTACK && a.target_ == target ) {
            return true;
        }
    }
    return false;
}

// Blue stack at (0,0,0), faster than red so it activates first.
Hero makeBlueHero( int power, int knowledge ) {
    auto u = std::make_shared<Unit>( "BlueUnit", 1, 10, 5, 50, 5, 10, 4, 2 );
    u->setPosition( 0, 0, 0 );
    Hero h( "BlueHero", 0, 0, power, knowledge );
    h.getArmy( ).addUnit( u );
    return h;
}

// Red stack adjacent to blue at (1,0,-1).
Hero makeRedHero( ) {
    auto u = std::make_shared<Unit>( "RedUnit", 1, 5, 5, 50, 2, 5, 1, 2 );
    u->setPosition( 1, 0, -1 );
    Hero h( "RedHero", 0, 0, 0, 0 );
    h.getArmy( ).addUnit( u );
    return h;
}

} // namespace

TEST( ActionGeneratorTest, MeleeWaitDefendMovePresentWhenAdjacent ) {
    GameManager gm( makeBlueHero( 0, 0 ), makeRedHero( ) );
    SpellResolver resolver( gm );
    ActionGenerator generator( gm, resolver );

    Unit* active = gm.getCurrentUnit( );
    ASSERT_NE( active, nullptr );
    EXPECT_EQ( active->getName( ), "BlueUnit" );

    Unit* red = gm.getRedHero( ).getArmy( ).getUnits( )[0].get( );
    const std::vector<ActionCommand> actions = generator.generate( *active );

    EXPECT_TRUE( hasMeleeAgainst( actions, red ) );
    EXPECT_EQ( countType( actions, ActionType::DEFEND ), 1 );
    EXPECT_EQ( countType( actions, ActionType::WAIT ), 1 );
    EXPECT_GT( countType( actions, ActionType::MOVE ), 0 );
}

TEST( ActionGeneratorTest, NoSpellsWhenHeroHasNoMana ) {
    GameManager gm( makeBlueHero( 0, 0 ), makeRedHero( ) ); // knowledge 0 -> 0 mana
    SpellResolver resolver( gm );
    ActionGenerator generator( gm, resolver );

    Unit* active = gm.getCurrentUnit( );
    ASSERT_NE( active, nullptr );

    const std::vector<ActionCommand> actions = generator.generate( *active );
    EXPECT_EQ( countType( actions, ActionType::CAST_SPELL ), 0 );
}

TEST( ActionGeneratorTest, SpellsAppearWhenHeroHasMana ) {
    GameManager gm( makeBlueHero( 3, 10 ), makeRedHero( ) ); // knowledge 10 -> 100 mana
    SpellResolver resolver( gm );
    ActionGenerator generator( gm, resolver );

    Unit* active = gm.getCurrentUnit( );
    ASSERT_NE( active, nullptr );

    const std::vector<ActionCommand> actions = generator.generate( *active );
    EXPECT_GT( countType( actions, ActionType::CAST_SPELL ), 0 );
}

} // namespace test
