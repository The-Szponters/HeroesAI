/**
 * @file ActionManagerTest.cc
 * @brief Unit tests for ActionManager move, attack and defend logic.
 */
#include "../ActionManager.h"
#include "../../models/Board.h"
#include "../../models/Unit.h"
#include <gtest/gtest.h>
#include <memory>

namespace test {

using core::ActionManager;
using models::Board;
using models::Hex;
using models::Unit;

/**
 * @brief Shared test fixture for ActionManager behavior.
 */
class ActionManagerTest : public ::testing::Test {
protected:
    Board board_;
    ActionManager am_;
    std::shared_ptr<Unit> u1_;
    std::shared_ptr<Unit> u2_;

    void SetUp( ) override {
        u1_ = std::make_shared<Unit>( "HeroUnit", 1, 10, 5, 50, 5, 10, 2, 1 );
        u1_->setPosition( 0, 0, 0 );
        board_.getHex( 0, 0, 0 ).setUnit( u1_ );

        u2_ = std::make_shared<Unit>( "Enemy", 1, 5, 5, 50, 2, 5, 1, 1 );
        u2_->setPosition( 1, 0, -1 );
        board_.getHex( 1, 0, -1 ).setUnit( u2_ );
    }
};

TEST_F( ActionManagerTest, GetAvailableDestinations ) {
    std::vector<Hex*> dests = am_.getAvailableDestinations( *u1_, board_ );

    EXPECT_GT( dests.size( ), 0 );

    bool found_start = false;
    bool found_blocked = false;
    for ( Hex* h : dests ) {
        if ( h->getQ( ) == 0 && h->getR( ) == 0 && h->getS( ) == 0 ) {
            found_start = true;
}
        if ( h->getQ( ) == 1 && h->getR( ) == 0 && h->getS( ) == -1 ) {
            found_blocked = true;
}
    }

    EXPECT_FALSE( found_start );
    EXPECT_FALSE( found_blocked );
}

TEST_F( ActionManagerTest, GetAvailableAttacks ) {
    auto attacks = am_.getAvailableAttacks( *u1_, board_ );
    ASSERT_EQ( attacks.size( ), 1 );
    EXPECT_EQ( attacks[0].first->getName( ), "Enemy" );
    EXPECT_EQ( attacks[0].second->getQ( ), 1 );
}

TEST_F( ActionManagerTest, MoveAction ) {
    Hex& dest = board_.getHex( 0, 1, -1 );

    EXPECT_TRUE( board_.getHex( 0, 0, 0 ).hasUnit( ) );

    am_.move( *u1_, dest, board_ );

    EXPECT_EQ( u1_->getQ( ), 0 );
    EXPECT_EQ( u1_->getR( ), 1 );
    EXPECT_EQ( u1_->getS( ), -1 );

    EXPECT_FALSE( board_.getHex( 0, 0, 0 ).hasUnit( ) );
    EXPECT_TRUE( dest.hasUnit( ) );
    EXPECT_EQ( dest.getUnit( ), u1_ );
}

TEST_F( ActionManagerTest, MoveActionThrowsOnOccupiedHex ) {
    Hex& dest = board_.getHex( 1, 0, -1 );

    EXPECT_TRUE( dest.hasUnit( ) );

    EXPECT_THROW( am_.move( *u1_, dest, board_ ), std::runtime_error );

    EXPECT_EQ( u1_->getQ( ), 0 );
    EXPECT_EQ( u1_->getR( ), 0 );
    EXPECT_EQ( u1_->getS( ), 0 );
}

TEST_F( ActionManagerTest, DefendActionAppliesBuff ) {
    EXPECT_EQ( u1_->getDefense( ), 5 );
    am_.defend( *u1_ );
    EXPECT_EQ( u1_->getDefense( ), 10 );

    u1_->onTurnStart( );
    EXPECT_EQ( u1_->getDefense( ), 5 );
}

TEST_F( ActionManagerTest, CalculateDamageEqualStats ) {
    Unit attacker( "Attacker", 1, 10, 10, 100, 5, 5, 5, 10 );
    Unit defender( "Defender", 1, 10, 10, 100, 5, 5, 5, 10 );

    int damage = am_.calculateDamage( attacker, defender );
    EXPECT_EQ( damage, 50 );
}

TEST_F( ActionManagerTest, CalculateDamageAttackAdvantage ) {
    Unit attacker( "Attacker", 1, 20, 10, 100, 5, 5, 5, 10 );
    Unit defender( "Defender", 1, 10, 10, 100, 5, 5, 5, 10 );

    int damage = am_.calculateDamage( attacker, defender );
    EXPECT_EQ( damage, 75 );
}

TEST_F( ActionManagerTest, CalculateDamageDefenseAdvantage ) {
    Unit attacker( "Attacker", 1, 10, 10, 100, 5, 5, 5, 10 );
    Unit defender( "Defender", 1, 10, 20, 100, 5, 5, 5, 10 );

    int damage = am_.calculateDamage( attacker, defender );
    EXPECT_EQ( damage, 37 );
}

TEST_F( ActionManagerTest, CalculateDamageMaxCaps ) {
    Unit attacker( "Attacker", 1, 100, 10, 100, 5, 5, 5, 10 );
    Unit defender( "Defender", 1, 10, 10, 100, 5, 5, 5, 10 );
    EXPECT_EQ( am_.calculateDamage( attacker, defender ), 200 );

    Unit attacker2( "Attacker", 1, 10, 10, 100, 10, 10, 5, 10 );
    Unit defender2( "Defender", 1, 10, 100, 100, 10, 10, 5, 10 );
    EXPECT_EQ( am_.calculateDamage( attacker2, defender2 ), 30 );
}

TEST_F( ActionManagerTest, AttackActionAppliesDamageAndMoves ) {
    board_.getHex( 0, 0, 0 ).removeUnit( );
    board_.getHex( 1, 0, -1 ).removeUnit( );

    auto att = std::make_shared<Unit>( "Attacker", 1, 10, 10, 100, 5, 5, 5, 1 );
    att->setPosition( 0, 0, 0 );
    board_.getHex( 0, 0, 0 ).setUnit( att );

    auto def = std::make_shared<Unit>( "Defender", 1, 10, 10, 100, 5, 5, 5, 1 );
    def->setPosition( 2, 0, -2 );
    board_.getHex( 2, 0, -2 ).setUnit( def );

    Hex& attack_hex = board_.getHex( 1, 0, -1 );
    bool is_dead = am_.attack( *att, *def, attack_hex, board_ );

    EXPECT_FALSE( is_dead );
    EXPECT_EQ( att->getQ( ), 1 );
    EXPECT_EQ( att->getR( ), 0 );
    EXPECT_EQ( att->getS( ), -1 );

    EXPECT_EQ( def->getHealthLeft( ), 95 );
    EXPECT_TRUE( board_.getHex( 2, 0, -2 ).hasUnit( ) );
}

TEST_F( ActionManagerTest, AttackActionKillsUnitAndMovesToDeadUnits ) {
    board_.getHex( 0, 0, 0 ).removeUnit( );
    board_.getHex( 1, 0, -1 ).removeUnit( );

    auto att = std::make_shared<Unit>( "Attacker", 1, 200, 10, 100, 1000, 1000, 5, 1 );
    att->setPosition( 0, 0, 0 );
    board_.getHex( 0, 0, 0 ).setUnit( att );

    auto def = std::make_shared<Unit>( "Defender", 1, 10, 10, 100, 5, 5, 5, 1 );
    def->setPosition( 2, 0, -2 );
    board_.getHex( 2, 0, -2 ).setUnit( def );

    Hex& attack_hex = board_.getHex( 1, 0, -1 );
    bool is_dead = am_.attack( *att, *def, attack_hex, board_ );

    EXPECT_TRUE( is_dead );
    EXPECT_EQ( def->getCount( ), 0 );

    Hex& def_hex = board_.getHex( 2, 0, -2 );
    EXPECT_FALSE( def_hex.hasUnit( ) );
    EXPECT_EQ( def_hex.getDeadUnits( ).size( ), 1 );

    auto dead_unit_ptr = def_hex.getDeadUnits( )[0].lock( );
    ASSERT_NE( dead_unit_ptr, nullptr );
    EXPECT_EQ( dead_unit_ptr->getName( ), "Defender" );
}

TEST_F( ActionManagerTest, AttackActionThrowsWhenOccupiedByAnother ) {
    board_.getHex( 0, 0, 0 ).removeUnit( );
    board_.getHex( 1, 0, -1 ).removeUnit( );
    board_.getHex( 2, 0, -2 ).removeUnit( );

    auto att = std::make_shared<Unit>( "Attacker", 1, 10, 10, 100, 5, 5, 5, 1 );
    att->setPosition( 0, 0, 0 );
    board_.getHex( 0, 0, 0 ).setUnit( att );

    auto blocker = std::make_shared<Unit>( "Blocker", 1, 10, 10, 100, 5, 5, 5, 1 );
    blocker->setPosition( 1, 0, -1 );
    board_.getHex( 1, 0, -1 ).setUnit( blocker );

    auto def = std::make_shared<Unit>( "Defender", 1, 10, 10, 100, 5, 5, 5, 1 );
    def->setPosition( 2, 0, -2 );
    board_.getHex( 2, 0, -2 ).setUnit( def );

    Hex& attack_hex = board_.getHex( 1, 0, -1 );
    EXPECT_THROW( am_.attack( *att, *def, attack_hex, board_ ), std::runtime_error );
}

} // namespace test
