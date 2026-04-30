/**
 * @file ArmyTest.cc
 * @brief Unit tests for the Army class (capacity, add, remove).
 */
#include "../Army.h"
#include "../Unit.h"
#include <gtest/gtest.h>

namespace test {

using models::Army;
using models::Unit;

TEST( ArmyTest, AddUnitSuccess ) {
    Army army;
    auto u1 = std::make_shared<Unit>( "Warrior", 1, 1, 1, 10, 1, 2, 4, 10 );
    EXPECT_TRUE( army.addUnit( u1 ) );
    EXPECT_EQ( army.getUnits( ).size( ), 1 );
    EXPECT_EQ( army.getUnits( )[0]->getName( ), "Warrior" );
}

TEST( ArmyTest, AddUnitLimitFailure ) {
    Army army;
    for ( int i = 0; i < 7; ++i ) {
        EXPECT_TRUE( army.addUnit( std::make_shared<Unit>( ) ) );
    }

    EXPECT_FALSE( army.addUnit( std::make_shared<Unit>( ) ) );
    EXPECT_EQ( army.getUnits( ).size( ), 7 );
}

TEST( ArmyTest, AddNullptrUnitFailure ) {
    Army army;
    EXPECT_FALSE( army.addUnit( nullptr ) );
    EXPECT_EQ( army.getUnits( ).size( ), 0 );
}

TEST( ArmyTest, RemoveUnit ) {
    Army army;
    auto u1 = std::make_shared<Unit>( "Warrior", 1, 1, 1, 10, 1, 2, 4, 10 );
    auto u2 = std::make_shared<Unit>( "Archer", 1, 1, 1, 10, 1, 2, 4, 10 );
    army.addUnit( u1 );
    army.addUnit( u2 );
    EXPECT_EQ( army.getUnits( ).size( ), 2 );

    army.removeUnit( 0 );
    EXPECT_EQ( army.getUnits( ).size( ), 1 );
    EXPECT_EQ( army.getUnits( )[0]->getName( ), "Archer" );

    army.removeUnit( 10 );
    EXPECT_EQ( army.getUnits( ).size( ), 1 );
}

} // namespace test
