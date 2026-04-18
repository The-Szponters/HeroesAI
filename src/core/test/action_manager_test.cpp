#include <gtest/gtest.h>
#include "../ActionManager.hpp"
#include "../../models/board.hpp"
#include "../../models/unit.hpp"
#include <memory>

class ActionManagerTest : public ::testing::Test {
protected:
    Board board;
    ActionManager am;
    std::shared_ptr<Unit> u1;
    std::shared_ptr<Unit> u2;

    void SetUp() override {
        // Fast unit at 0, 0, 0
        u1 = std::make_shared<Unit>("HeroUnit", 1, 10, 5, 50, 5, 10, 2, 1);
        u1->set_position(0, 0, 0);
        board.get_hex(0, 0, 0).set_unit(u1);

        // Enemy unit at 1, 0, -1 (adjacent)
        u2 = std::make_shared<Unit>("Enemy", 1, 5, 5, 50, 2, 5, 1, 1);
        u2->set_position(1, 0, -1);
        board.get_hex(1, 0, -1).set_unit(u2);
    }
};

TEST_F(ActionManagerTest, GetAvailableDestinations) {
    std::vector<Hex*> dests = am.get_available_destinations(*u1, board);
    
    EXPECT_GT(dests.size(), 0);
    
    bool found_start = false;
    bool found_blocked = false;
    for (Hex* h : dests) {
        if (h->get_q() == 0 && h->get_r() == 0 && h->get_s() == 0) found_start = true;
        if (h->get_q() == 1 && h->get_r() == 0 && h->get_s() == -1) found_blocked = true;
    }
    
    EXPECT_FALSE(found_start);
    EXPECT_FALSE(found_blocked);
}

TEST_F(ActionManagerTest, GetAvailableAttacks) {
    auto attacks = am.get_available_attacks(*u1, board);
    ASSERT_EQ(attacks.size(), 1);
    EXPECT_EQ(attacks[0].first->get_name(), "Enemy");
    EXPECT_EQ(attacks[0].second->get_q(), 1);
}

TEST_F(ActionManagerTest, MoveAction) {
    Hex& dest = board.get_hex(0, 1, -1);
    
    EXPECT_TRUE(board.get_hex(0, 0, 0).has_unit());
    
    am.move(*u1, dest, board);
    
    EXPECT_EQ(u1->get_q(), 0);
    EXPECT_EQ(u1->get_r(), 1);
    EXPECT_EQ(u1->get_s(), -1);
    
    EXPECT_FALSE(board.get_hex(0, 0, 0).has_unit());
    EXPECT_TRUE(dest.has_unit());
    EXPECT_EQ(dest.get_unit(), u1);
}

TEST_F(ActionManagerTest, MoveActionThrowsOnOccupiedHex) {
    Hex& dest = board.get_hex(1, 0, -1); // occupied by u2
    
    EXPECT_TRUE(dest.has_unit());
    
    EXPECT_THROW(am.move(*u1, dest, board), std::runtime_error);
    
    // u1 should remain at 0, 0, 0
    EXPECT_EQ(u1->get_q(), 0);
    EXPECT_EQ(u1->get_r(), 0);
    EXPECT_EQ(u1->get_s(), 0);
}
