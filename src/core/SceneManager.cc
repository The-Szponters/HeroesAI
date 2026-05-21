/**
 * @file SceneManager.cc
 * @brief Implementation of the application-level scene orchestrator.
 * @author Łukasz Szydlik
 */
#include "SceneManager.h"

#include <exception>

#include "../models/UnitFactory.h"
#include "../views/ViewportUtils.h"
#include "ArmySetupScene.h"
#include "BattleScene.h"
#include "MainMenuScene.h"

namespace core {

SceneManager::SceneManager( const std::string& settings_path )
    : settings_( Settings::loadFromFile( settings_path ) ),
      window_( sf::VideoMode( { settings_.windowWidth_, settings_.windowHeight_ } ),
                   settings_.windowTitle_ ) {
    window_.setFramerateLimit( settings_.framerateLimit_ );
    views::applyLetterboxView( window_,
                                   views::K_LOGICAL_WIDTH,
                                   views::K_LOGICAL_HEIGHT );

    // Load the unit catalog up front so every scene (army setup as well
    // as battle) can query unit metadata -- previously the catalog was
    // only loaded inside BattleScene's constructor, leaving portraits
    // and asset filenames unavailable to the army-setup picker.
    try {
        models::UnitFactory::init( "assets/units.json" );
    } catch ( const std::exception& ) {}
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
    case views::SceneId::ARMY_SETUP:
        return std::make_unique<ArmySetupScene>( window_, settings_ );
    case views::SceneId::BATTLE:
        return std::make_unique<BattleScene>( window_, settings_ );
    case views::SceneId::NONE:
    default:
        return nullptr;
    }
}

} // namespace core
