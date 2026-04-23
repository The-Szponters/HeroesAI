#include <gtest/gtest.h>
#include "../GameManager.hpp"
#include "../../models/hero.hpp"
#include "../../models/unit.hpp"
#include "../../models/board.hpp"
#include "../../models/army.hpp"

class GameManagerTest : public ::testing::Test {
protected:
    Hero blue;
    Hero red;

    void SetUp() override {
        auto u1 = std::make_shared<Unit>("BlueUnit", 1, 10, 5, 50, 5, 10, 2, 2);
        u1->set_position(0, 0, 0);
        blue = Hero("BlueHero", 0, 0, 0, 0);
        blue.get_army().add_unit(u1);

        auto u2 = std::make_shared<Unit>("RedUnit", 1, 5, 5, 50, 2, 5, 1, 2);
        u2->set_position(1, 0, -1);
        red = Hero("RedHero", 0, 0, 0, 0);
        red.get_army().add_unit(u2);
    }
};

TEST_F(GameManagerTest, Initialization) {
    GameManager gm(blue, red);
    EXPECT_NE(gm.get_current_unit(), nullptr);
}

TEST_F(GameManagerTest, AttackRemovesDeadUnit) {
    // RedUnit has 50 Health, 1 defense, 2 count (100 total HP). 
    // BlueUnit has 200 attack, dmg=100. It will definitely kill RedUnit and its count will drop to 0.
    auto u1 = std::make_shared<Unit>("BlueKiller", 1, 200, 5, 50, 1000, 1000, 9, 1);
    u1->set_position(0, 0, 0);
    blue.get_army().remove_unit(0);
    blue.get_army().add_unit(u1);
    
    GameManager gm(blue, red);
    
    Unit* current = gm.get_current_unit();
    ASSERT_EQ(current->get_name(), "BlueKiller"); // has speed 2

    // Get RedUnit from red army inside GM
    Unit* redUnit = gm.get_red_hero().get_army().get_units()[0].get();
    Hex placeholder_hex(0, 0, 0); // Attacking from 0, 0, 0
    gm.attack(*current, *redUnit, placeholder_hex);
    
    // The dead unit should NOT be removed from the hero's army to allow resurrection
    EXPECT_EQ(gm.get_red_hero().get_army().get_units().size(), 1);
    
    // But it should be removed from the active pool of units (e.g. from the round queues)
    std::vector<Unit*> units_left = gm.get_units_left_in_round();
    EXPECT_EQ(std::find(units_left.begin(), units_left.end(), redUnit), units_left.end());
}
