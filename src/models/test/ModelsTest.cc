/**
 * @file ModelsTest.cc
 * @brief Unit tests for Hex, Unit, RangeUnit and the buff system.
 */
#include "../Hex.h"
#include "../RangeUnit.h"
#include "../Unit.h"
#include <gtest/gtest.h>

namespace test {

using models::Buff;
using models::BuffFactory;
using models::BuffType;
using models::Hex;
using models::RangeUnit;
using models::Unit;

TEST( HexTest, InitializationValid ) {
    Hex h( 1, -1, 0 );
    EXPECT_EQ( h.getQ( ), 1 );
    EXPECT_EQ( h.getR( ), -1 );
    EXPECT_EQ( h.getS( ), 0 );
}

TEST( HexTest, InitializationInvalidThrows ) {
    EXPECT_THROW( Hex( 1, 1, 1 ), std::invalid_argument );
}

TEST( HexTest, EqualityOperator ) {
    Hex h1( 1, -1, 0 );
    Hex h2( 1, -1, 0 );
    Hex h3( 0, 1, -1 );
    EXPECT_TRUE( h1 == h2 );
    EXPECT_FALSE( h1 == h3 );
}

TEST( HexTest, DistanceOperator ) {
    Hex h1( 0, 0, 0 );
    Hex h2( 1, 0, -1 );
    Hex h3( -2, 1, 1 );

    EXPECT_EQ( h1.distanceTo( h2 ), 1 );
    EXPECT_EQ( h1.distanceTo( h3 ), 2 );
    EXPECT_EQ( h2.distanceTo( h3 ), 3 );
}

TEST( HexTest, InitializationWithUnit ) {
    auto u = std::make_shared<Unit>( "Warrior", 2, 10, 5, 100, 10, 15, 3, 5 );
    Hex h1( 0, 0, 0, u );
    EXPECT_EQ( h1.getQ( ), 0 );
    EXPECT_EQ( h1.getR( ), 0 );
    EXPECT_EQ( h1.getS( ), 0 );
    EXPECT_EQ( h1.getUnit( )->getName( ), "Warrior" );
    EXPECT_EQ( h1.getUnit( )->getTier( ), 2 );
    EXPECT_EQ( h1.getUnit( )->getAttack( ), 10 );
    EXPECT_EQ( h1.getUnit( )->getDefense( ), 5 );
    EXPECT_EQ( h1.getUnit( )->getHealth( ), 100 );
    EXPECT_EQ( h1.getUnit( )->getDamageMin( ), 10 );
    EXPECT_EQ( h1.getUnit( )->getDamageMax( ), 15 );
    EXPECT_EQ( h1.getUnit( )->getSpeed( ), 3 );
    EXPECT_EQ( h1.getUnit( )->getCount( ), 5 );
}

TEST( UnitTest, Initialization ) {
    Unit u( "Warrior", 2, 10, 5, 100, 10, 15, 3, 5 );
    EXPECT_EQ( u.getName( ), "Warrior" );
    EXPECT_EQ( u.getTier( ), 2 );
    EXPECT_EQ( u.getAttack( ), 10 );
    EXPECT_EQ( u.getDefense( ), 5 );
    EXPECT_EQ( u.getHealth( ), 100 );
    EXPECT_EQ( u.getDamageMin( ), 10 );
    EXPECT_EQ( u.getDamageMax( ), 15 );
    EXPECT_EQ( u.getSpeed( ), 3 );

    EXPECT_EQ( u.getCount( ), 5 );
    EXPECT_EQ( u.getHealthLeft( ), 100 );

    EXPECT_EQ( u.getQ( ), 0 );
    EXPECT_EQ( u.getR( ), 0 );
    EXPECT_EQ( u.getS( ), 0 );
}

TEST( UnitTest, SetPosition ) {
    Unit u;
    u.setPosition( 1, -1, 0 );
    EXPECT_EQ( u.getQ( ), 1 );
    EXPECT_EQ( u.getR( ), -1 );
    EXPECT_EQ( u.getS( ), 0 );
}

TEST( UnitTest, TakeDamageLessThanHealthLeft ) {
    Unit u( "Warrior", 2, 10, 5, 100, 10, 15, 3, 5 );
    u.takeDamage( 20 );
    EXPECT_EQ( u.getCount( ), 5 );
    EXPECT_EQ( u.getHealthLeft( ), 80 );
}

TEST( UnitTest, TakeDamageExactlyOneUnit ) {
    Unit u( "Warrior", 2, 10, 5, 100, 10, 15, 3, 5 );
    u.takeDamage( 100 );
    EXPECT_EQ( u.getCount( ), 4 );
    EXPECT_EQ( u.getHealthLeft( ), 100 );
}

TEST( UnitTest, TakeDamageMultipleUnits ) {
    Unit u( "Warrior", 2, 10, 5, 100, 10, 15, 3, 5 );
    u.takeDamage( 230 );
    EXPECT_EQ( u.getCount( ), 3 );
    EXPECT_EQ( u.getHealthLeft( ), 70 );
}

TEST( UnitTest, TakeDamageMoreThanTotalHealth ) {
    Unit u( "Warrior", 2, 10, 5, 100, 10, 15, 3, 5 );
    u.takeDamage( 1000 );
    EXPECT_EQ( u.getCount( ), 0 );
    EXPECT_EQ( u.getHealthLeft( ), 0 );
}

TEST( UnitTest, BuffSystemDefend ) {
    Unit u( "Warrior", 2, 10, 5, 100, 10, 15, 6, 5 );
    EXPECT_EQ( u.getDefense( ), 5 );

    Buff b = BuffFactory::createDefendBuff( );
    u.applyBuff( b );
    EXPECT_EQ( u.getDefense( ), 10 );

    u.removeBuff( BuffType::DEFEND );
    EXPECT_EQ( u.getDefense( ), 5 );
}

TEST( UnitTest, BuffSystemSlowPercentage ) {
    Unit u( "Warrior", 2, 10, 5, 100, 10, 15, 6, 5 );
    EXPECT_EQ( u.getSpeed( ), 6 );

    Buff b = BuffFactory::createSlowBuff( );
    u.applyBuff( b );
    // Spec: speed * 0.75 rounded up. ceil(6 * 0.75) = ceil(4.5) = 5.
    EXPECT_EQ( u.getSpeed( ), 5 );

    u.removeBuff( BuffType::SLOW );
    EXPECT_EQ( u.getSpeed( ), 6 );
}

TEST( UnitTest, BuffSystemBlindHardOverride ) {
    Unit u( "Warrior", 2, 10, 5, 100, 10, 15, 6, 5 );
    EXPECT_EQ( u.getSpeed( ), 6 );

    Buff b = BuffFactory::createBlindBuff( );
    u.applyBuff( b );
    EXPECT_EQ( u.getSpeed( ), 0 );

    u.removeBuff( BuffType::BLIND );
    EXPECT_EQ( u.getSpeed( ), 6 );
}

TEST( UnitTest, BuffSystemTurnTick ) {
    Unit u( "Warrior", 2, 10, 5, 100, 10, 15, 6, 5 );
    Buff b = BuffFactory::createDefendBuff( );
    u.applyBuff( b );
    EXPECT_EQ( u.getDefense( ), 10 );

    u.onTurnStart( );
    EXPECT_EQ( u.getDefense( ), 5 );
}

TEST( UnitTest, BuffSystemStatClamping ) {
    Unit u( "Warrior", 2, 10, 5, 100, 10, 15, 6, 5 );

    Buff b;
    b.type_ = BuffType::DEFEND;
    b.duration_ = 1;
    b.modifyDefense_ = []( int d ) { return d - 100; };

    u.applyBuff( b );
    EXPECT_EQ( u.getDefense( ), 0 );
}

TEST( RangeUnitTest, Initialization ) {
    RangeUnit ru( "Archer", 3, 8, 3, 50, 10, 12, 4, 10, 12 );
    EXPECT_EQ( ru.getName( ), "Archer" );
    EXPECT_EQ( ru.getTier( ), 3 );
    EXPECT_EQ( ru.getAttack( ), 8 );
    EXPECT_EQ( ru.getDamageMin( ), 10 );
    EXPECT_EQ( ru.getDamageMax( ), 12 );
    EXPECT_EQ( ru.getShoots( ), 12 );
    EXPECT_EQ( ru.getCount( ), 10 );
}

} // namespace test
