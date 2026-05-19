/**
 * @file Main.cc
 * @brief Application entry point.
 *
 * Constructs the SceneManager (which owns the SFML window) and hands
 * control to it. The SceneManager starts on the main menu and transitions
 * into the battle scene when the user clicks "New Game".
 * @author Dominik Śledziewski & Łukasz Szydlik
 */
#include "core/SceneManager.h"

int main( ) {
    core::SceneManager scene_manager( 1280, 960, "HeroesAI" );
    scene_manager.run( );
    return 0;
}
