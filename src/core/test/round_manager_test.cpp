#include <gtest/gtest.h>
#include "../RoundManager.hpp"
#include "../../models/unit.hpp"

class RoundManagerTest : public ::testing::Test {
protected:
    Unit u1;
    Unit u2;
    Unit u3;
    std::vector<Unit*> units;
    RoundManager* rm;

    RoundManagerTest() : 
        u1("Fast", 1, 1, 1, 10, 1, 1, 10, 1),
        u2("Medium", 1, 1, 1, 10, 1, 1, 5, 1),
        u3("Slow", 1, 1, 1, 10, 1, 1, 1, 1) 
    {
        units = {&u1, &u2, &u3};
        rm = new RoundManager(units);
    }
    
    ~RoundManagerTest() override {
        delete rm;
    }
};

TEST_F(RoundManagerTest, InitialStateSizeCheck) {
    std::vector<Unit*> empty_vec;
    RoundManager empty_rm(empty_vec);
    EXPECT_EQ(empty_rm.get_current_unit(), nullptr);
    EXPECT_TRUE(empty_rm.get_units_left_in_round().empty());
}

TEST_F(RoundManagerTest, StartRoundPopulatesCorrectly) {
    rm->start_round();
    
    // First should be u1 (highest speed - 10)
    EXPECT_EQ(rm->get_current_unit(), &u1);
    rm->end_current_unit_turn();
    
    // Next should be u2 (medium speed - 5)
    EXPECT_EQ(rm->get_current_unit(), &u2);
    rm->end_current_unit_turn();
    
    // Next should be u3 (slowest speed - 1)
    EXPECT_EQ(rm->get_current_unit(), &u3);
    rm->end_current_unit_turn();
}

TEST_F(RoundManagerTest, WaitMechanicReverseSpeedOrder) {
    rm->start_round();
    
    EXPECT_EQ(rm->get_current_unit(), &u1); // 10
    rm->wait_current_unit();
    
    EXPECT_EQ(rm->get_current_unit(), &u2); // 5
    rm->wait_current_unit();
    
    // Unactivated remaining: u3
    EXPECT_EQ(rm->get_current_unit(), &u3); // 1
    rm->end_current_unit_turn(); // Unactivated is now empty
    
    // We expect MIN heap for waited: smallest returns first
    EXPECT_EQ(rm->get_current_unit(), &u2); // 5
    rm->end_current_unit_turn();
    
    EXPECT_EQ(rm->get_current_unit(), &u1); // 10
    rm->end_current_unit_turn();
}

TEST_F(RoundManagerTest, GetUnitsLeftInRoundSizeCheck) {
    rm->start_round();
    
    EXPECT_EQ(rm->get_units_left_in_round().size(), 3);
    
    rm->wait_current_unit();
    EXPECT_EQ(rm->get_units_left_in_round().size(), 3); // 2 in unactivated, 1 in waited
    
    EXPECT_EQ(rm->get_current_unit(), &u2);
    rm->end_current_unit_turn();
    EXPECT_EQ(rm->get_units_left_in_round().size(), 2); // 1 in unactivated, 1 in waited
}

TEST_F(RoundManagerTest, GetUnitQueueInRoundSorting) {
    std::vector<Unit*> queue = rm->get_unit_queue_in_round();
    
    ASSERT_EQ(queue.size(), 3);
    EXPECT_EQ(queue[0], &u1); // 10
    EXPECT_EQ(queue[1], &u2); // 5
    EXPECT_EQ(queue[2], &u3); // 1
}
