/**
 * @file MainMenuPresenter.cc
 * @brief Implementation of the main menu presenter.
 * @author Łukasz Szydlik
 */
#include "MainMenuPresenter.h"

namespace presenters {

MainMenuPresenter::MainMenuPresenter( views::IMainMenuView& view ) : view_( view ) {}

void MainMenuPresenter::start( ) {
    view_.showMessage( "" );
    newGameRequested_ = false;
    armySetupRequested_ = false;
    quitRequested_ = false;
}

void MainMenuPresenter::onNewGameClicked( ) {
    newGameRequested_ = true;
}

bool MainMenuPresenter::isNewGameRequested( ) const {
    return newGameRequested_;
}

void MainMenuPresenter::onArmySetupClicked( ) {
    armySetupRequested_ = true;
}

bool MainMenuPresenter::isArmySetupRequested( ) const {
    return armySetupRequested_;
}

void MainMenuPresenter::onQuitClicked( ) {
    quitRequested_ = true;
}

bool MainMenuPresenter::isQuitRequested( ) const {
    return quitRequested_;
}

} // namespace presenters
