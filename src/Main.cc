/**
 * @file Main.cc
 * @brief Application entry point.
 *
 * Constructs the SceneManager (which loads settings.cfg internally,
 * opens the SFML window, and runs the scene loop). The scene flow is
 * MainMenu -> [ArmySetup] -> Battle.
 * @author Dominik Śledziewski & Łukasz Szydlik
 */
#include "core/SceneManager.h"

int main( ) {
    core::SceneManager scene_manager( "settings.cfg" );
    scene_manager.run( );
    return 0;
}
