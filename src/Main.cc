/**
 * @file Main.cc
 * @brief Application entry point.
 *
 * Loads settings from settings.cfg next to the executable, constructs the SceneManager
 * (which owns the SFML window) and hands control to it. The SceneManager
 * starts on the main menu and transitions into the battle scene when the
 * user clicks "New Game".
 * @author Dominik Śledziewski & Łukasz Szydlik
 */
#include "core/SceneManager.h"
#include "core/Settings.h"

int main( ) {
    const core::Settings settings = core::Settings::loadFromFile( "settings.cfg" );
    core::SceneManager scene_manager( settings.windowWidth_,
                                          settings.windowHeight_,
                                          settings.windowTitle_,
                                          settings.framerateLimit_ );
    scene_manager.run( );
    return 0;
}
