/**
 * @file UnitFactoryTest.cc
 * @brief Unit tests for the data-driven UnitFactory loader.
 */
#include <gtest/gtest.h>
#include "../UnitFactory.h"
#include <fstream>
#include <cstdio>
#include <stdexcept>

namespace test {

using models::RangeUnit;
using models::UnitFactory;
using models::UnitID;
using models::Unit;

/**
 * @brief Shared test fixture for UnitFactory behavior.
 */
class UnitFactoryTest : public ::testing::Test {
protected:
    const std::string test_json_path = "mock_units.json";

    void SetUp() override {
        std::ofstream file(test_json_path );
        file << "{\n"
             << "  \"Pikeman\": { \"tier\": 1, \"attack\": 4, \"defense\": 5, \"health\": 10, \"damage_min\": 1, \"damage_max\": 3, \"speed\": 4, \"size\": 1 },\n"
             << "  \"Archer\": { \"tier\": 2, \"attack\": 6, \"defense\": 3, \"health\": 10, \"damage_min\": 2, \"damage_max\": 3, \"speed\": 4, \"size\": 1, \"shoots\": 12 },\n"
             << "  \"Imp\": { \"tier\": 1 }\n"
             << "}\n";
        file.close( );
    }

    void TearDown() override {
        std::remove(test_json_path.c_str() );
    }
};

TEST_F(UnitFactoryTest, InitThrowsOnMissingFile ){
    EXPECT_THROW(UnitFactory::init("non_existent_file.json"), std::runtime_error );
}

TEST_F(UnitFactoryTest, InitSucceedsWithMockFile ){
    EXPECT_NO_THROW(UnitFactory::init(test_json_path) );
}

TEST_F(UnitFactoryTest, CreateStandardUnitFromData ){
    UnitFactory::init(test_json_path );
    auto unit = UnitFactory::create_unit(UnitID::PIKEMAN, 10 );

    EXPECT_NE(unit, nullptr );
    EXPECT_EQ(std::dynamic_pointer_cast<RangeUnit>(unit), nullptr );

    EXPECT_EQ(unit->get_name(), "Pikeman" );
    EXPECT_EQ(unit->get_tier(), 1 );
    EXPECT_EQ(unit->get_attack(), 4 );
    EXPECT_EQ(unit->get_defense(), 5 );
    EXPECT_EQ(unit->get_health(), 10 );
    EXPECT_EQ(unit->get_damage_min(), 1 );
    EXPECT_EQ(unit->get_damage_max(), 3 );
    EXPECT_EQ(unit->get_speed(), 4 );
    EXPECT_EQ(unit->get_count(), 10 );
}

TEST_F(UnitFactoryTest, CreateRangeUnitFromDataWithShootsInt ){
    UnitFactory::init(test_json_path );
    auto unit = UnitFactory::create_unit(UnitID::ARCHER, 5 );

    auto range_unit = std::dynamic_pointer_cast<RangeUnit>(unit );
    ASSERT_NE(range_unit, nullptr );

    EXPECT_EQ(range_unit->get_name(), "Archer" );
    EXPECT_EQ(range_unit->get_tier(), 2 );
    EXPECT_EQ(range_unit->get_attack(), 6 );
    EXPECT_EQ(range_unit->get_shoots(), 12 );
    EXPECT_EQ(range_unit->get_count(), 5 );
}

TEST_F(UnitFactoryTest, CreateThrowsOnMissingFields ){
    UnitFactory::init(test_json_path );

    EXPECT_THROW(UnitFactory::create_unit(UnitID::IMP, 3), std::runtime_error );
}

TEST_F(UnitFactoryTest, CreateUnitNotLoadedThrowsException ){
    UnitFactory::init(test_json_path );

    EXPECT_THROW(UnitFactory::create_unit(UnitID::ANGEL, 1), std::runtime_error );
}

}  // namespace test
