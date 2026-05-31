/**
 * @file MainMenuScene.cc
 * @brief Implementation of the main-menu scene wrapper.
 * @author Łukasz Szydlik
 */
#include "MainMenuScene.h"

namespace core {

MainMenuScene::MainMenuScene( sf::RenderWindow& window )
    : window_( window ), view_( window ), presenter_( view_ ) {
    presenter_.start( );
}

void MainMenuScene::processEvents( ) {
    view_.processEvents( presenter_ );
}

void MainMenuScene::render( ) {
    view_.render( );
}

bool MainMenuScene::isFinished( ) const {
    return ! window_.isOpen( ) || presenter_.isNewGameRequested( ) ||
           presenter_.isArmySetupRequested( ) || presenter_.isQuitRequested( );
}

views::SceneId MainMenuScene::nextSceneId( ) const {
    if ( presenter_.isNewGameRequested( ) ) {
        return views::SceneId::BATTLE;
    }
    if ( presenter_.isArmySetupRequested( ) ) {
        return views::SceneId::ARMY_SETUP;
    }
    // Quit (or anything else): NONE terminates the application loop.
    return views::SceneId::NONE;
}

} // namespace core
