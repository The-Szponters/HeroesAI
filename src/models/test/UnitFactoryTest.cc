/**
 * @file UnitFactoryTest.cc
 * @brief Unit tests for the data-driven UnitFactory loader.
 */
#include "../UnitFactory.h"
#include <cstdio>
#include <fstream>
#include <gtest/gtest.h>
#include <stdexcept>

namespace test {

using models::RangeUnit;
using models::Unit;
using models::UnitFactory;
using models::UnitID;

/**
 * @brief Shared test fixture for UnitFactory behavior.
 */
class UnitFactoryTest : public ::testing::Test {
protected:
    const std::string TEST_JSON_PATH = "mock_units.json";

    void SetUp( ) override {
        std::ofstream file( TEST_JSON_PATH );
        file << "{\n"
             << "  \"Pikeman\": { \"tier\": 1, \"attack\": 4, \"defense\": 5, \"health\": 10, "
                "\"damage_min\": 1, \"damage_max\": 3, \"speed\": 4, \"size\": 1 },\n"
             << "  \"Archer\": { \"tier\": 2, \"attack\": 6, \"defense\": 3, \"health\": 10, "
                "\"damage_min\": 2, \"damage_max\": 3, \"speed\": 4, \"size\": 1, \"shoots\": 12 "
                "},\n"
             << "  \"Imp\": { \"tier\": 1 }\n"
             << "}\n";
        file.close( );
    }

    void TearDown( ) override { std::remove( TEST_JSON_PATH.c_str( ) ); }
};

TEST_F( UnitFactoryTest, InitThrowsOnMissingFile ) {
    EXPECT_THROW( UnitFactory::init( "non_existent_file.json" ), std::runtime_error );
}

TEST_F( UnitFactoryTest, InitSucceedsWithMockFile ) {
    EXPECT_NO_THROW( UnitFactory::init( TEST_JSON_PATH ) );
}

TEST_F( UnitFactoryTest, CreateStandardUnitFromData ) {
    UnitFactory::init( TEST_JSON_PATH );
    auto unit = UnitFactory::createUnit( UnitID::PIKEMAN, 10 );

    EXPECT_NE( unit, nullptr );
    EXPECT_EQ( std::dynamic_pointer_cast<RangeUnit>( unit ), nullptr );

    EXPECT_EQ( unit->getName( ), "Pikeman" );
    EXPECT_EQ( unit->getTier( ), 1 );
    EXPECT_EQ( unit->getAttack( ), 4 );
    EXPECT_EQ( unit->getDefense( ), 5 );
    EXPECT_EQ( unit->getHealth( ), 10 );
    EXPECT_EQ( unit->getDamageMin( ), 1 );
    EXPECT_EQ( unit->getDamageMax( ), 3 );
    EXPECT_EQ( unit->getSpeed( ), 4 );
    EXPECT_EQ( unit->getCount( ), 10 );
}

TEST_F( UnitFactoryTest, CreateRangeUnitFromDataWithShootsInt ) {
    UnitFactory::init( TEST_JSON_PATH );
    auto unit = UnitFactory::createUnit( UnitID::ARCHER, 5 );

    auto range_unit = std::dynamic_pointer_cast<RangeUnit>( unit );
    ASSERT_NE( range_unit, nullptr );

    EXPECT_EQ( range_unit->getName( ), "Archer" );
    EXPECT_EQ( range_unit->getTier( ), 2 );
    EXPECT_EQ( range_unit->getAttack( ), 6 );
    EXPECT_EQ( range_unit->getShoots( ), 12 );
    EXPECT_EQ( range_unit->getCount( ), 5 );
}

TEST_F( UnitFactoryTest, CreateThrowsOnMissingFields ) {
    UnitFactory::init( TEST_JSON_PATH );

    EXPECT_THROW( UnitFactory::createUnit( UnitID::IMP, 3 ), std::runtime_error );
}

TEST_F( UnitFactoryTest, CreateUnitNotLoadedThrowsException ) {
    UnitFactory::init( TEST_JSON_PATH );

    EXPECT_THROW( UnitFactory::createUnit( UnitID::ANGEL, 1 ), std::runtime_error );
}

} // namespace test
