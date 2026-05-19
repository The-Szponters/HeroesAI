/**
 * @file BattleScene.cc
 * @brief Implementation of the battle scene wrapper.
 * @author Łukasz Szydlik
 */
#include "BattleScene.h"

#include <exception>

#include "../models/Unit.h"
#include "../models/UnitFactory.h"
#include "../models/UnitId.h"

namespace core {

using models::Hero;
using models::UnitFactory;
using models::UnitID;

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
 * @brief Static unit placement data for initial army layouts.
 */
struct Placement {
    UnitID id_;
    int count_;
    int col_;
    int row_;
};

void applyRoster( Hero& hero, const Placement* roster_begin, const Placement* roster_end ) {
    for ( const Placement* p = roster_begin; p != roster_end; ++p ) {
        auto unit = UnitFactory::createUnit( p->id_, p->count_ );
        const Axial a = offsetToAxial( p->col_, p->row_ );
        unit->setPosition( a.q_, a.r_, a.s_ );
        hero.getArmy( ).addUnit( unit );
    }
}

} // namespace

Hero BattleScene::buildBlueHero( ) {
    Hero hero( "Blue Hero", 0, 0, 0, 0 );
    static const Placement blue_roster[] = {
        { UnitID::PIKEMAN, 10, 0, 0 },
        { UnitID::ARCHER, 8, 0, 2 },
        { UnitID::GRIFFIN, 5, 1, 4 },
        { UnitID::SWORDSMAN, 6, 0, 6 },
        { UnitID::MONK, 4, 0, 8 },
        { UnitID::CAVALIER, 3, 1, 10 },
        { UnitID::ARCHANGEL, 1, 1, 5 },
    };
    applyRoster( hero, std::begin( blue_roster ), std::end( blue_roster ) );
    return hero;
}

Hero BattleScene::buildRedHero( ) {
    Hero hero( "Red Hero", 0, 0, 0, 0 );
    static const Placement red_roster[] = {
        { UnitID::IMP, 12, 14, 0 },
        { UnitID::GOG, 8, 14, 2 },
        { UnitID::HELL_HOUND, 5, 13, 4 },
        { UnitID::DEMON, 6, 14, 6 },
        { UnitID::PIT_FIEND, 4, 14, 8 },
        { UnitID::EFREET, 3, 14, 10 },
        { UnitID::DEVIL, 1, 13, 5 },
    };
    applyRoster( hero, std::begin( red_roster ), std::end( red_roster ) );
    return hero;
}

BattleScene::BattleScene( sf::RenderWindow& window )
    : window_( window ),
      blueHero_( ( UnitFactory::init( "assets/units.json" ), buildBlueHero( ) ) ),
      redHero_( buildRedHero( ) ),
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
