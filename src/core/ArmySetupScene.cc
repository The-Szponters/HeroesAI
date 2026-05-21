/**
 * @file ArmySetupScene.cc
 * @brief Implementation of the army-setup scene wrapper.
 * @author Łukasz Szydlik
 */
#include "ArmySetupScene.h"

namespace core {

ArmySetupScene::ArmySetupScene( sf::RenderWindow& window, Settings& settings )
    : window_( window ),
      settings_( settings ),
      view_( window ),
      presenter_( view_, settings_ ) {
    presenter_.start( );
}

void ArmySetupScene::processEvents( ) {
    view_.processEvents( presenter_ );
}

void ArmySetupScene::render( ) {
    view_.render( presenter_ );
}

bool ArmySetupScene::isFinished( ) const {
    return ! window_.isOpen( ) || presenter_.isBackRequested( );
}

views::SceneId ArmySetupScene::nextSceneId( ) const {
    if ( presenter_.isBackRequested( ) ) {
        return views::SceneId::MAIN_MENU;
    }
    return views::SceneId::NONE;
}

} // namespace core
