/**
 * @file SceneManager.h
 * @brief Application-level scene orchestrator.
 * @author Łukasz Szydlik
 */
#pragma once

#include <SFML/Graphics.hpp>

#include <memory>
#include <string>

#include "../views/IScene.h"

namespace core {

/**
 * @brief Owns the SFML window and drives the active scene's lifecycle.
 *
 * The manager opens a single sf::RenderWindow shared by every scene
 * (so transitions don't flicker the window), starts on the main menu,
 * and constructs the next scene whenever the current one reports it
 * is finished.
 */
class SceneManager {
public:
    SceneManager( unsigned int width,
                      unsigned int height,
                      const std::string& title,
                      unsigned int framerate_limit );

    void run( );

private:
    void switchTo( views::SceneId id );
    std::unique_ptr<views::IScene> createScene( views::SceneId id );

    sf::RenderWindow window_;
    std::unique_ptr<views::IScene> currentScene_;
};

} // namespace core
