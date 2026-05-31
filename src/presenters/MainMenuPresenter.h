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
 * exposes button click commands and matching query flags read by the
 * scene wrapper to decide which scene to switch into next.
 */
class MainMenuPresenter {
public:
    explicit MainMenuPresenter( views::IMainMenuView& view );

    void start( );

    void onNewGameClicked( );
    bool isNewGameRequested( ) const;

    void onArmySetupClicked( );
    bool isArmySetupRequested( ) const;

    void onQuitClicked( );
    bool isQuitRequested( ) const;

private:
    views::IMainMenuView& view_;
    bool newGameRequested_ = false;
    bool armySetupRequested_ = false;
    bool quitRequested_ = false;
};

} // namespace presenters
