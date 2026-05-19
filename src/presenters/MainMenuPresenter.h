/**
 * @file MainMenuPresenter.h
 * @brief Mediator handling main menu interactions.
 * @author Łukasz Szydlik
 */
#pragma once

#include "../views/IMainMenuView.h"

namespace presenters {

/**
 * @brief Translates main-menu UI events into scene-transition requests.
 *
 * Holds no domain model -- the menu has no game state to manage. It
 * exposes a single command (onNewGameClicked) and a single query
 * (isNewGameRequested) used by the scene wrapper to decide when to
 * switch to the battle scene.
 */
class MainMenuPresenter {
public:
    explicit MainMenuPresenter( views::IMainMenuView& view );

    void start( );
    void onNewGameClicked( );
    bool isNewGameRequested( ) const;

private:
    views::IMainMenuView& view_;
    bool newGameRequested_ = false;
};

} // namespace presenters
