/**
 * @file BattleScene.cc
 * @brief Implementation of the battle scene wrapper.
 * @author Łukasz Szydlik
 */
#include "BattleScene.h"

#include <array>
#include <exception>
#include <utility>

#include "../models/Unit.h"
#include "../models/UnitFactory.h"
#include "../models/UnitId.h"

namespace core {

using models::Hero;
using models::UnitFactory;

namespace {

/**
 * @brief Axial coordinates with derived cube component.
 */
struct Axial {
    int q_;
    int r_;
    int s_;
};

Axial offsetToAxial( int col, int row ) {
    const int q = col - ( row - ( row & 1 ) ) / 2;
    const int r = row;
    return { q, r, -q - r };
}

/**
 * @brief Fixed offset-coord positions per army slot index.
 *
 * Preserves the original hardcoded layout so a fresh settings.cfg
 * produces the same battle as before the army-setup feature shipped.
 */
constexpr std::array<std::pair<int, int>, K_ARMY_SLOT_COUNT> K_BLUE_SLOT_POSITIONS = { {
    { 0, 0 }, { 0, 2 }, { 1, 4 }, { 0, 6 }, { 0, 8 }, { 1, 10 }, { 1, 5 }
} };
constexpr std::array<std::pair<int, int>, K_ARMY_SLOT_COUNT> K_RED_SLOT_POSITIONS = { {
    { 14, 0 }, { 14, 2 }, { 13, 4 }, { 14, 6 }, { 14, 8 }, { 14, 10 }, { 13, 5 }
} };

void applyRoster( Hero& hero,
                          const ArmyConfig& army,
                          const std::array<std::pair<int, int>, K_ARMY_SLOT_COUNT>& positions ) {
    for ( std::size_t i = 0; i < army.size( ); ++i ) {
        const ArmySlot& slot = army[i];
        if ( ! slot.unitId_.has_value( ) || slot.count_ <= 0 ) {
            continue;
        }
        const auto [col, row] = positions[i];
        auto unit = UnitFactory::createUnit( *slot.unitId_, slot.count_ );
        const Axial a = offsetToAxial( col, row );
        unit->setPosition( a.q_, a.r_, a.s_ );
        hero.getArmy( ).addUnit( unit );
    }
}

} // namespace

Hero BattleScene::buildBlueHero( const ArmyConfig& army ) {
    Hero hero( "Blue Hero", 10, 10, 10, 10 );
    applyRoster( hero, army, K_BLUE_SLOT_POSITIONS );
    return hero;
}

Hero BattleScene::buildRedHero( const ArmyConfig& army ) {
    Hero hero( "Red Hero", 10, 10, 10, 10 );
    applyRoster( hero, army, K_RED_SLOT_POSITIONS );
    return hero;
}

BattleScene::BattleScene( sf::RenderWindow& window, const Settings& settings )
    : window_( window ),
      blueHero_(
          ( UnitFactory::init( "assets/units.json" ), buildBlueHero( settings.leftArmy_ ) ) ),
      redHero_( buildRedHero( settings.rightArmy_ ) ),
      gameManager_( blueHero_, redHero_ ),
      view_( window ),
      presenter_( gameManager_, view_ ) {
    try {
        presenter_.startBattle( );
    } catch ( const std::exception& ) {}
}

void BattleScene::processEvents( ) {
    try {
        view_.processEvents( presenter_ );
    } catch ( ... ) {}
}

void BattleScene::render( ) {
    try {
        view_.render( );
    } catch ( ... ) {}
}

bool BattleScene::isFinished( ) const {
    return ! window_.isOpen( );
}

views::SceneId BattleScene::nextSceneId( ) const {
    return views::SceneId::NONE;
}

} // namespace core
