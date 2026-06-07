/**
 * @file MainMenuScene.h
 * @brief IScene wrapper composing the main-menu MVP triad.
 * @author Lukasz Szydlik
 */
#pragma once

#include <SFML/Graphics.hpp>

#include "../presenters/MainMenuPresenter.h"
#include "../views/IScene.h"
#include "../views/SfmlMainMenuView.h"

namespace core {

/**
 * @brief Top-level scene that owns the menu view and presenter.
 *
 * Delegates IScene calls to the contained view and presenter. Reports
 * finished when the user clicks "New Game" or closes the window, and
 * picks the next scene accordingly.
 */
class MainMenuScene : public views::IScene {
public:
    explicit MainMenuScene( sf::RenderWindow& window );

    void processEvents( ) override;
    void render( ) override;
    bool isFinished( ) const override;
    views::SceneId nextSceneId( ) const override;

private:
    sf::RenderWindow& window_;
    views::SfmlMainMenuView view_;
    presenters::MainMenuPresenter presenter_;
};

} // namespace core
