/**
 * @file BattleScene.h
 * @brief IScene wrapper composing the battle MVP triad.
 * @author Łukasz Szydlik
 */
#pragma once

#include <SFML/Graphics.hpp>

#include "../models/Hero.h"
#include "../presenters/BattlePresenter.h"
#include "../views/IScene.h"
#include "../views/SfmlBattleView.h"
#include "GameManager.h"
#include "Settings.h"

namespace core {

/**
 * @brief Top-level scene that owns the battle MVP triad.
 *
 * Bootstraps the unit factory, builds the two opposing heroes from the
 * army rosters stored in the shared Settings (edited by ArmySetupScene),
 * constructs the GameManager / SfmlBattleView / BattlePresenter chain
 * and drives the existing battle event loop. Reports finished when the
 * user closes the window.
 */
class BattleScene : public views::IScene {
public:
    BattleScene( sf::RenderWindow& window, const Settings& settings );

    void processEvents( ) override;
    void render( ) override;
    bool isFinished( ) const override;
    views::SceneId nextSceneId( ) const override;

private:
    static models::Hero buildBlueHero( const ArmyConfig& army, const HeroConfig& hero_config );
    static models::Hero buildRedHero( const ArmyConfig& army, const HeroConfig& hero_config );

    sf::RenderWindow& window_;
    models::Hero blueHero_;
    models::Hero redHero_;
    GameManager gameManager_;
    views::SfmlBattleView view_;
    presenters::BattlePresenter presenter_;
};

} // namespace core
