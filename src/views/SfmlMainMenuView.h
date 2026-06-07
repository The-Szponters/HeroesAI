/**
 * @file SfmlMainMenuView.h
 * @brief Concrete SFML-backed main menu view.
 * @author Dominik Sledziewski
 */
#pragma once

#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>

#include <memory>
#include <string>

#include "IMainMenuView.h"

namespace presenters {
class MainMenuPresenter;
}

namespace views {

/**
 * @brief SFML-based renderer for the main menu.
 *
 * Draws a background image and a single "New Game" button on top of
 * the shared application window. Does not own the window -- the
 * SceneManager does.
 */
class SfmlMainMenuView : public IMainMenuView {
public:
    explicit SfmlMainMenuView( sf::RenderWindow& window );

    bool isOpen( ) const;
    void processEvents( presenters::MainMenuPresenter& presenter );
    void render( );

    void showMessage( const std::string& msg ) override;

private:
    void loadAssets( );
    void layoutButtons( );
    void drawBackground( );
    void drawNewGameButton( );
    void drawCustomSettingsButton( );
    void drawQuitButton( );
    void drawMessage( );
    bool routeMenuClick( float x, float y, presenters::MainMenuPresenter& presenter );

    sf::RenderWindow& window_;
    float screenWidth_;
    float screenHeight_;

    sf::Texture backgroundTexture_;
    std::unique_ptr<sf::Sprite> backgroundSprite_;
    bool backgroundLoaded_ = false;

    sf::Texture newGameButtonTexture_;
    std::unique_ptr<sf::Sprite> newGameButtonSprite_;
    sf::FloatRect newGameButtonBounds_;
    bool newGameButtonHovered_ = false;
    bool newGameButtonLoaded_ = false;

    sf::Texture customSettingsButtonTexture_;
    std::unique_ptr<sf::Sprite> customSettingsButtonSprite_;
    sf::FloatRect customSettingsButtonBounds_;
    bool customSettingsButtonHovered_ = false;
    bool customSettingsButtonLoaded_ = false;

    sf::Texture quitButtonTexture_;
    std::unique_ptr<sf::Sprite> quitButtonSprite_;
    sf::FloatRect quitButtonBounds_;
    bool quitButtonHovered_ = false;
    bool quitButtonLoaded_ = false;

    sf::Font font_;
    bool fontLoaded_ = false;
    std::unique_ptr<sf::Text> messageText_;
    std::string latestMessage_;
};

} // namespace views
