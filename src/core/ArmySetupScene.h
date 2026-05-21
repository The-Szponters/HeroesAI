/**
 * @file ArmySetupScene.h
 * @brief IScene wrapper composing the army-setup MVP triad.
 * @author Łukasz Szydlik
 */
#pragma once

#include <SFML/Graphics.hpp>

#include "../presenters/ArmySetupPresenter.h"
#include "../views/IScene.h"
#include "../views/SfmlArmySetupView.h"
#include "Settings.h"

namespace core {

/**
 * @brief Top-level scene that owns the army-setup view and presenter.
 *
 * Mutates the shared Settings reference through the presenter, and
 * reports finished + nextSceneId == MAIN_MENU once the user clicks
 * "Back".
 */
class ArmySetupScene : public views::IScene {
public:
    ArmySetupScene( sf::RenderWindow& window, Settings& settings );

    void processEvents( ) override;
    void render( ) override;
    bool isFinished( ) const override;
    views::SceneId nextSceneId( ) const override;

private:
    sf::RenderWindow& window_;
    Settings& settings_;
    views::SfmlArmySetupView view_;
    presenters::ArmySetupPresenter presenter_;
};

} // namespace core
