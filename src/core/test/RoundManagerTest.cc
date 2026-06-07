/**
 * @file RoundManagerTest.cc
 * @brief Unit tests for the RoundManager initiative ordering and waits.
 * @author Lukasz Szydlik
 */
#include "../RoundManager.h"
#include "../../models/Unit.h"
#include <gtest/gtest.h>

namespace test {

using core::RoundManager;
using models::Unit;

/**
 * @brief Shared test fixture for RoundManager behavior.
 */
class RoundManagerTest : public ::testing::Test {
protected:
    Unit u1_;
    Unit u2_;
    Unit u3_;
    std::vector<Unit*> units_;
    RoundManager* rm_;

    RoundManagerTest( )
        : u1_( "Fast", 1, 1, 1, 10, 1, 1, 10, 1 ),
          u2_( "Medium", 1, 1, 1, 10, 1, 1, 5, 1 ),
          u3_( "Slow", 1, 1, 1, 10, 1, 1, 1, 1 ) {
        units_ = { &u1_, &u2_, &u3_ };
        rm_ = new RoundManager( units_ );
    }

    ~RoundManagerTest( ) override { delete rm_; }
};

TEST_F( RoundManagerTest, InitialStateSizeCheck ) {
    std::vector<Unit*> empty_vec;
    RoundManager empty_rm( empty_vec );
    EXPECT_EQ( empty_rm.getCurrentUnit( ), nullptr );
    EXPECT_TRUE( empty_rm.getUnitsLeftInRound( ).empty( ) );
}

TEST_F( RoundManagerTest, StartRoundPopulatesCorrectly ) {
    rm_->startRound( );

    EXPECT_EQ( rm_->getCurrentUnit( ), &u1_ );
    rm_->endCurrentUnitTurn( );

    EXPECT_EQ( rm_->getCurrentUnit( ), &u2_ );
    rm_->endCurrentUnitTurn( );

    EXPECT_EQ( rm_->getCurrentUnit( ), &u3_ );
    rm_->endCurrentUnitTurn( );
}

TEST_F( RoundManagerTest, WaitMechanicReverseSpeedOrder ) {
    rm_->startRound( );

    EXPECT_EQ( rm_->getCurrentUnit( ), &u1_ );
    rm_->waitCurrentUnit( );

    EXPECT_EQ( rm_->getCurrentUnit( ), &u2_ );
    rm_->waitCurrentUnit( );

    EXPECT_EQ( rm_->getCurrentUnit( ), &u3_ );
    rm_->endCurrentUnitTurn( );

    EXPECT_EQ( rm_->getCurrentUnit( ), &u2_ );
    rm_->endCurrentUnitTurn( );

    EXPECT_EQ( rm_->getCurrentUnit( ), &u1_ );
    rm_->endCurrentUnitTurn( );
}

TEST_F( RoundManagerTest, GetUnitsLeftInRoundSizeCheck ) {
    rm_->startRound( );

    EXPECT_EQ( rm_->getUnitsLeftInRound( ).size( ), 3 );

    rm_->waitCurrentUnit( );
    EXPECT_EQ( rm_->getUnitsLeftInRound( ).size( ), 3 );

    EXPECT_EQ( rm_->getCurrentUnit( ), &u2_ );
    rm_->endCurrentUnitTurn( );
    EXPECT_EQ( rm_->getUnitsLeftInRound( ).size( ), 2 );
}

TEST_F( RoundManagerTest, CurrentUnitCanWaitReflectsPhase ) {
    rm_->startRound( );

    // Units still in the unactivated phase may wait.
    EXPECT_TRUE( rm_->currentUnitCanWait( ) ); // u1 active, unactivated
    rm_->waitCurrentUnit( );
    EXPECT_TRUE( rm_->currentUnitCanWait( ) ); // u2 active, unactivated
    rm_->waitCurrentUnit( );
    EXPECT_TRUE( rm_->currentUnitCanWait( ) ); // u3 active, unactivated
    rm_->endCurrentUnitTurn( );

    // Now the current unit is served from the waited queue and may not
    // wait again.
    EXPECT_FALSE( rm_->currentUnitCanWait( ) );
}

TEST_F( RoundManagerTest, GetUnitQueueInRoundSorting ) {
    rm_->startRound( );
    std::vector<Unit*> queue = rm_->getUnitQueueInRound( );

    ASSERT_EQ( queue.size( ), 3 );
    EXPECT_EQ( queue[0], &u1_ );
    EXPECT_EQ( queue[1], &u2_ );
    EXPECT_EQ( queue[2], &u3_ );
}

} // namespace test
