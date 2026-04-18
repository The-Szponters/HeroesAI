#include <gtest/gtest.h>
#include "../army.hpp"
#include "../unit.hpp"

TEST(ArmyTest, AddUnitSuccess) {
    Army army;
    auto u1 = std::make_shared<Unit>("Warrior", 1, 1, 1, 10, 1, 2, 4, 10);
    EXPECT_TRUE(army.add_unit(u1));
    EXPECT_EQ(army.get_units().size(), 1);
    EXPECT_EQ(army.get_units()[0]->get_name(), "Warrior");
}

TEST(ArmyTest, AddUnitLimitFailure) {
    Army army;
    for (int i = 0; i < 7; ++i) {
        EXPECT_TRUE(army.add_unit(std::make_shared<Unit>()));
    }
    // 8th unit should fail
    EXPECT_FALSE(army.add_unit(std::make_shared<Unit>()));
    EXPECT_EQ(army.get_units().size(), 7);
}

TEST(ArmyTest, AddNullptrUnitFailure) {
    Army army;
    EXPECT_FALSE(army.add_unit(nullptr));
    EXPECT_EQ(army.get_units().size(), 0);
}

TEST(ArmyTest, RemoveUnit) {
    Army army;
    auto u1 = std::make_shared<Unit>("Warrior", 1, 1, 1, 10, 1, 2, 4, 10);
    auto u2 = std::make_shared<Unit>("Archer", 1, 1, 1, 10, 1, 2, 4, 10);
    army.add_unit(u1);
    army.add_unit(u2);
    EXPECT_EQ(army.get_units().size(), 2);
    
    army.remove_unit(0);
    EXPECT_EQ(army.get_units().size(), 1);
    EXPECT_EQ(army.get_units()[0]->get_name(), "Archer");
    
    army.remove_unit(10); // out of bounds, shouldn't crash
    EXPECT_EQ(army.get_units().size(), 1);
}
