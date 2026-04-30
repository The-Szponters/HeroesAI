/**
 * @file Main.cc
 * @brief Application entry point.
 *
 * Bootstraps the unit factory from JSON, builds two opposing heroes with
 * a fixed test roster, wires the model/view/presenter triad, and drives
 * the SFML event loop until the window closes.
 */
#include "core/GameManager.h"
#include "models/Hero.h"
#include "models/Unit.h"
#include "models/UnitFactory.h"
#include "models/UnitId.h"
#include "presenters/BattlePresenter.h"
#include "views/SfmlBattleView.h"

#include <memory>

using core::GameManager;
using models::Hero;
using models::UnitFactory;
using models::UnitID;
using presenters::BattlePresenter;
using views::SfmlBattleView;

namespace src {

/**
 * @brief Axial coordinates with derived cube component.
 */
struct Axial {
    int q;
    int r;
    int s;
};
Axial offset_to_axial( int col, int row ) {
    const int q = col - ( row - ( row & 1 ) ) / 2;
    const int r = row;
    return { q, r, -q - r };
}

/**
 * @brief Static unit placement data for initial army layouts.
 */
struct Placement {
    UnitID id;
    int count;
    int col;
    int row;
};

} // namespace src

int main( ) {
    UnitFactory::init( "assets/units.json" );

    Hero blue_hero( "Blue Hero", 0, 0, 0, 0 );
    Hero red_hero( "Red Hero", 0, 0, 0, 0 );

    const src::Placement blue_roster[] = {
        { UnitID::PIKEMAN, 10, 0, 0 },
        { UnitID::ARCHER, 8, 0, 2 },
        { UnitID::GRIFFIN, 5, 1, 4 },
        { UnitID::SWORDSMAN, 6, 0, 6 },
        { UnitID::MONK, 4, 0, 8 },
        { UnitID::CAVALIER, 3, 1, 10 },
        { UnitID::ARCHANGEL, 1, 1, 5 },
    };
    for ( const auto& p : blue_roster ) {
        auto unit = UnitFactory::create_unit( p.id, p.count );
        const src::Axial a = src::offset_to_axial( p.col, p.row );
        unit->set_position( a.q, a.r, a.s );
        blue_hero.get_army( ).add_unit( unit );
    }

    const src::Placement red_roster[] = {
        { UnitID::IMP, 12, 14, 0 },
        { UnitID::GOG, 8, 14, 2 },
        { UnitID::HELL_HOUND, 5, 13, 4 },
        { UnitID::DEMON, 6, 14, 6 },
        { UnitID::PIT_FIEND, 4, 14, 8 },
        { UnitID::EFREET, 3, 14, 10 },
        { UnitID::DEVIL, 1, 13, 5 },
    };
    for ( const auto& p : red_roster ) {
        auto unit = UnitFactory::create_unit( p.id, p.count );
        const src::Axial a = src::offset_to_axial( p.col, p.row );
        unit->set_position( a.q, a.r, a.s );
        red_hero.get_army( ).add_unit( unit );
    }

    GameManager model( blue_hero, red_hero );
    SfmlBattleView view( 1280, 960, "HeroesAI - Battle" );
    BattlePresenter presenter( model, view );

    presenter.start_battle( );
    view.render( );

    while ( view.is_open( ) ) {
        try {
            view.process_events( presenter );
        } catch ( const std::exception& e ) {
            (void) e;
        }
        view.render( );
    }

    return 0;
}
