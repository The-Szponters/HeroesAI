#include "core/GameManager.hpp"
#include "models/hero.hpp"
#include "models/unit.hpp"
#include "models/unit_factory.hpp"
#include "models/unit_id.hpp"
#include "presenters/BattlePresenter.hpp"
#include "views/SfmlBattleView.hpp"

#include <memory>

namespace {

struct Axial { int q; int r; int s; };
Axial offset_to_axial(int col, int row) {
    const int q = col - (row - (row & 1)) / 2;
    const int r = row;
    return { q, r, -q - r };
}
}

int main() {

    UnitFactory::init("assets/units.json");

    Hero blue_hero("Blue Hero", 0, 0, 0, 0);
    Hero red_hero("Red Hero",  0, 0, 0, 0);

    struct Placement { UnitID id; int count; int col; int row; };

    const Placement blue_roster[] = {
        { UnitID::Pikeman,    10, 0, 0  },
        { UnitID::Archer,      8, 0, 2  },
        { UnitID::Griffin,     5, 1, 4  },
        { UnitID::Swordsman,   6, 0, 6  },
        { UnitID::Monk,        4, 0, 8  },
        { UnitID::Cavalier,    3, 1, 10 },
        { UnitID::Archangel,   1, 1, 5  },
    };
    for (const auto& p : blue_roster) {
        auto unit = UnitFactory::create_unit(p.id, p.count);
        const Axial a = offset_to_axial(p.col, p.row);
        unit->set_position(a.q, a.r, a.s);
        blue_hero.get_army().add_unit(unit);
    }

    const Placement red_roster[] = {
        { UnitID::Imp,         12, 14, 0  },
        { UnitID::Gog,          8, 14, 2  },
        { UnitID::HellHound,    5, 13, 4  },
        { UnitID::Demon,        6, 14, 6  },
        { UnitID::PitFiend,     4, 14, 8  },
        { UnitID::Efreet,       3, 14, 10 },
        { UnitID::Devil,        1, 13, 5  },
    };
    for (const auto& p : red_roster) {
        auto unit = UnitFactory::create_unit(p.id, p.count);
        const Axial a = offset_to_axial(p.col, p.row);
        unit->set_position(a.q, a.r, a.s);
        red_hero.get_army().add_unit(unit);
    }

    GameManager model(blue_hero, red_hero);
    SfmlBattleView view(1280, 960, "HeroesAI - Battle");
    BattlePresenter presenter(model, view);

    presenter.start_battle();
    view.render();

    while (view.is_open()) {
        try {
            view.process_events(presenter);
        } catch (const std::exception& e) {

            (void)e;
        }
        view.render();
    }

    return 0;
}
