/**
 * @file SceneManager.cc
 * @brief Implementation of the application-level scene orchestrator.
 * @author Łukasz Szydlik
 */
#include "SceneManager.h"

#include "BattleScene.h"
#include "MainMenuScene.h"

namespace core {

SceneManager::SceneManager( unsigned int width,
                                unsigned int height,
                                const std::string& title )
    : window_( sf::VideoMode( { width, height } ), title ) {
    window_.setFramerateLimit( 60 );
}

void SceneManager::run( ) {
    switchTo( views::SceneId::MAIN_MENU );
    while ( window_.isOpen( ) && currentScene_ != nullptr ) {
        try {
            currentScene_->processEvents( );
            currentScene_->render( );
        } catch ( ... ) {}

        if ( currentScene_->isFinished( ) ) {
            switchTo( currentScene_->nextSceneId( ) );
        }
    }
}

void SceneManager::switchTo( views::SceneId id ) {
    currentScene_.reset( );
    if ( id == views::SceneId::NONE ) {
        return;
    }
    currentScene_ = createScene( id );
}

std::unique_ptr<views::IScene> SceneManager::createScene( views::SceneId id ) {
    switch ( id ) {
    case views::SceneId::MAIN_MENU:
        return std::make_unique<MainMenuScene>( window_ );
    case views::SceneId::BATTLE:
        return std::make_unique<BattleScene>( window_ );
    case views::SceneId::NONE:
    default:
        return nullptr;
    }
}

} // namespace core
