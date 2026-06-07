/**
 * @file SceneManager.h
 * @brief Application-level scene orchestrator.
 * @author Lukasz Szydlik
 */
#pragma once

#include <SFML/Graphics.hpp>

#include <memory>
#include <string>

#include "../views/IScene.h"
#include "Settings.h"

namespace core {

/**
 * @brief Owns the SFML window and drives the active scene's lifecycle.
 *
 * The manager opens a single sf::RenderWindow shared by every scene
 * (so transitions don't flicker the window), starts on the main menu,
 * and constructs the next scene whenever the current one reports it
 * is finished. It also owns the shared application Settings so that
 * scenes can observe and mutate them (e.g. ArmySetupScene editing the
 * army rosters that BattleScene later consumes).
 */
class SceneManager {
public:
    explicit SceneManager( const std::string& settings_path = "settings.cfg" );

    void run( );

private:
    void switchTo( views::SceneId id );
    std::unique_ptr<views::IScene> createScene( views::SceneId id );

    Settings settings_;
    sf::RenderWindow window_;
    std::unique_ptr<views::IScene> currentScene_;
};

} // namespace core
