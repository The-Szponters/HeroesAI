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

TEST_F(ActionManagerTest, DefendActionAppliesBuff) {
    EXPECT_EQ(u1->get_defense(), 5);
    am.defend(*u1);
    EXPECT_EQ(u1->get_defense(), 10);
    
    u1->on_turn_start();
    EXPECT_EQ(u1->get_defense(), 5);
}

TEST_F(ActionManagerTest, CalculateDamageEqualStats) {
    // Attack 10, Defense 10, count 10, damage min 5, max 5 -> base damage 50
    Unit attacker("Attacker", 1, 10, 10, 100, 5, 5, 5, 10);
    Unit defender("Defender", 1, 10, 10, 100, 5, 5, 5, 10);
    
    int damage = am.calculate_damage(attacker, defender);
    EXPECT_EQ(damage, 50); // 1.0 modifier
}

TEST_F(ActionManagerTest, CalculateDamageAttackAdvantage) {
    // Attack 20, Defense 10 -> I = 10 -> 1.0 + 0.05 * 10 = 1.5 modifier
    // count 10, damage min 5, max 5 -> base damage 50
    Unit attacker("Attacker", 1, 20, 10, 100, 5, 5, 5, 10);
    Unit defender("Defender", 1, 10, 10, 100, 5, 5, 5, 10);
    
    int damage = am.calculate_damage(attacker, defender);
    EXPECT_EQ(damage, 75); // 50 * 1.5 = 75
}

TEST_F(ActionManagerTest, CalculateDamageDefenseAdvantage) {
    // Attack 10, Defense 20 -> I = -10 -> 1.0 - 0.025 * 10 = 0.75 modifier
    // count 10, damage min 5, max 5 -> base damage 50
    Unit attacker("Attacker", 1, 10, 10, 100, 5, 5, 5, 10);
    Unit defender("Defender", 1, 10, 20, 100, 5, 5, 5, 10);
    
    int damage = am.calculate_damage(attacker, defender);
    EXPECT_EQ(damage, 37); // 50 * 0.75 = 37.5 -> 37
}

TEST_F(ActionManagerTest, CalculateDamageMaxCaps) {
    // Attack 100, Defense 10 -> I = 90 -> modifier capped at 4.0
    // count 10, dmg 5 -> 50 * 4.0 = 200
    Unit attacker("Attacker", 1, 100, 10, 100, 5, 5, 5, 10);
    Unit defender("Defender", 1, 10, 10, 100, 5, 5, 5, 10);
    EXPECT_EQ(am.calculate_damage(attacker, defender), 200);
    
    // Attack 10, Defense 100 -> I = -90 -> modifier capped at 0.3
    // count 10, dmg 10 -> 100 * 0.3 = 30
    Unit attacker2("Attacker", 1, 10, 10, 100, 10, 10, 5, 10);
    Unit defender2("Defender", 1, 10, 100, 100, 10, 10, 5, 10);
    EXPECT_EQ(am.calculate_damage(attacker2, defender2), 30);
}

TEST_F(ActionManagerTest, AttackActionAppliesDamageAndMoves) {
    board.get_hex(0, 0, 0).remove_unit();
    board.get_hex(1, 0, -1).remove_unit();
    
    // Attacker at 0, 0, 0
    auto att = std::make_shared<Unit>("Attacker", 1, 10, 10, 100, 5, 5, 5, 1);
    att->set_position(0, 0, 0);
    board.get_hex(0, 0, 0).set_unit(att);
    
    // Defender at 2, 0, -2
    auto def = std::make_shared<Unit>("Defender", 1, 10, 10, 100, 5, 5, 5, 1);
    def->set_position(2, 0, -2);
    board.get_hex(2, 0, -2).set_unit(def);
    
    // Move to 1, 0, -1 to attack
    Hex& attack_hex = board.get_hex(1, 0, -1);
    bool is_dead = am.attack(*att, *def, attack_hex, board);
    
    EXPECT_FALSE(is_dead);
    EXPECT_EQ(att->get_q(), 1);
    EXPECT_EQ(att->get_r(), 0);
    EXPECT_EQ(att->get_s(), -1);
    
    EXPECT_EQ(def->get_health_left(), 95); // 100 - base damage 5
    EXPECT_TRUE(board.get_hex(2, 0, -2).has_unit());
}

TEST_F(ActionManagerTest, AttackActionKillsUnitAndMovesToDeadUnits) {
    board.get_hex(0, 0, 0).remove_unit();
    board.get_hex(1, 0, -1).remove_unit();
    
    // Overpowered Attacker at 0, 0, 0
    auto att = std::make_shared<Unit>("Attacker", 1, 200, 10, 100, 1000, 1000, 5, 1);
    att->set_position(0, 0, 0);
    board.get_hex(0, 0, 0).set_unit(att);
    
    // Weak Defender at 2, 0, -2
    auto def = std::make_shared<Unit>("Defender", 1, 10, 10, 100, 5, 5, 5, 1);
    def->set_position(2, 0, -2);
    board.get_hex(2, 0, -2).set_unit(def);
    
    // Move to 1, 0, -1 to attack
    Hex& attack_hex = board.get_hex(1, 0, -1);
    bool is_dead = am.attack(*att, *def, attack_hex, board);
    
    EXPECT_TRUE(is_dead);
    EXPECT_EQ(def->get_count(), 0);
    
    // The hex should no longer have an active unit, but should have a dead unit
    Hex& def_hex = board.get_hex(2, 0, -2);
    EXPECT_FALSE(def_hex.has_unit());
    EXPECT_EQ(def_hex.get_dead_units().size(), 1);
    
    // Validate we can reach the unit through weak ptr
    auto dead_unit_ptr = def_hex.get_dead_units()[0].lock();
    ASSERT_NE(dead_unit_ptr, nullptr);
    EXPECT_EQ(dead_unit_ptr->get_name(), "Defender");
}

TEST_F(ActionManagerTest, AttackActionThrowsWhenOccupiedByAnother) {
    board.get_hex(0, 0, 0).remove_unit();
    board.get_hex(1, 0, -1).remove_unit();
    board.get_hex(2, 0, -2).remove_unit();
    
    auto att = std::make_shared<Unit>("Attacker", 1, 10, 10, 100, 5, 5, 5, 1);
    att->set_position(0, 0, 0);
    board.get_hex(0, 0, 0).set_unit(att);
    
    auto blocker = std::make_shared<Unit>("Blocker", 1, 10, 10, 100, 5, 5, 5, 1);
    blocker->set_position(1, 0, -1);
    board.get_hex(1, 0, -1).set_unit(blocker);
    
    auto def = std::make_shared<Unit>("Defender", 1, 10, 10, 100, 5, 5, 5, 1);
    def->set_position(2, 0, -2);
    board.get_hex(2, 0, -2).set_unit(def);
    
    Hex& attack_hex = board.get_hex(1, 0, -1);
    EXPECT_THROW(am.attack(*att, *def, attack_hex, board), std::runtime_error);
}

