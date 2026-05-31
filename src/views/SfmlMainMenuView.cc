/**
 * @file SfmlMainMenuView.cc
 * @brief Implementation of the SFML-backed main menu renderer.
 * @author Łukasz Szydlik
 */
#include <array>
#include <optional>

#include "../presenters/MainMenuPresenter.h"
#include "SfmlMainMenuView.h"
#include "ViewportUtils.h"

namespace views {

using presenters::MainMenuPresenter;

namespace {

constexpr float K_BUTTON_WIDTH = 400.0f;
constexpr float K_BUTTON_HEIGHT = 180.0f;
constexpr unsigned int K_MESSAGE_LABEL_SIZE = 18;

// Per-button size multipliers (relative to the base New Game button).
// Custom is narrower but a touch taller; quit is uniformly larger.
constexpr float K_CUSTOM_BUTTON_WIDTH_SCALE = 0.82f;
constexpr float K_CUSTOM_BUTTON_HEIGHT_SCALE = 1.10f;
constexpr float K_QUIT_BUTTON_SCALE = 1.15f;

// Top of the button stack as a fraction of screen height (smaller = higher).
constexpr float K_STACK_TOP_FRACTION = 0.05f;

// Independent vertical gaps between the stacked buttons. Negative values
// tighten / overlap the bounding boxes -- fine given the buttons'
// transparent padding -- and let Custom sit closer to each neighbour.
constexpr float K_GAP_NEW_TO_CUSTOM = 90.0f;
constexpr float K_GAP_CUSTOM_TO_QUIT = 60.0f;

} // namespace

SfmlMainMenuView::SfmlMainMenuView( sf::RenderWindow& window )
    : window_( window ),
      screenWidth_( window.getView( ).getSize( ).x ),
      screenHeight_( window.getView( ).getSize( ).y ),
      newGameButtonBounds_( { 0.0f, 0.0f }, { K_BUTTON_WIDTH, K_BUTTON_HEIGHT } ),
      customSettingsButtonBounds_( { 0.0f, 0.0f }, { K_BUTTON_WIDTH, K_BUTTON_HEIGHT } ),
      quitButtonBounds_( { 0.0f, 0.0f }, { K_BUTTON_WIDTH, K_BUTTON_HEIGHT } ) {
    const std::array<const char*, 11> font_candidates = {
        "assets/font.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
        "/usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "/System/Library/Fonts/Helvetica.ttc",
        "/Library/Fonts/Arial.ttf",
        "/System/Library/Fonts/SFNS.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/segoeui.ttf",
    };
    for ( const char* path : font_candidates ) {
        if ( font_.openFromFile( path ) ) {
            fontLoaded_ = true;
            break;
        }
    }
    if ( fontLoaded_ ) {
        messageText_ = std::make_unique<sf::Text>( font_ );
        messageText_->setCharacterSize( K_MESSAGE_LABEL_SIZE );
        messageText_->setFillColor( sf::Color( 230, 230, 230 ) );
    }

    loadAssets( );
    layoutButtons( );

    window_.setMouseCursorVisible( true );
}

void SfmlMainMenuView::loadAssets( ) {
    if ( backgroundTexture_.loadFromFile( "assets/ui/menu/main_menu_bg.jpg" ) ) {
        backgroundSprite_ = std::make_unique<sf::Sprite>( backgroundTexture_ );
        const sf::Vector2u ts = backgroundTexture_.getSize( );
        if ( ts.x > 0 && ts.y > 0 ) {
            backgroundSprite_->setScale( { screenWidth_ / static_cast<float>( ts.x ),
                                            screenHeight_ / static_cast<float>( ts.y ) } );
        }
        backgroundSprite_->setPosition( { 0.0f, 0.0f } );
        backgroundLoaded_ = true;
    }

    if ( newGameButtonTexture_.loadFromFile( "assets/ui/menu/new_game_button.psd" ) ) {
        newGameButtonSprite_ = std::make_unique<sf::Sprite>( newGameButtonTexture_ );
        newGameButtonLoaded_ = true;
    }

    if ( customSettingsButtonTexture_.loadFromFile( "assets/ui/menu/custom_settings_button.psd" ) ) {
        customSettingsButtonSprite_ = std::make_unique<sf::Sprite>( customSettingsButtonTexture_ );
        customSettingsButtonLoaded_ = true;
    }

    if ( quitButtonTexture_.loadFromFile( "assets/ui/menu/quit-button.psd" ) ) {
        quitButtonSprite_ = std::make_unique<sf::Sprite>( quitButtonTexture_ );
        quitButtonLoaded_ = true;
    }
}

void SfmlMainMenuView::layoutButtons( ) {
    const float custom_w = K_BUTTON_WIDTH * K_CUSTOM_BUTTON_WIDTH_SCALE;
    const float custom_h = K_BUTTON_HEIGHT * K_CUSTOM_BUTTON_HEIGHT_SCALE;
    const float quit_w = K_BUTTON_WIDTH * K_QUIT_BUTTON_SCALE;
    const float quit_h = K_BUTTON_HEIGHT * K_QUIT_BUTTON_SCALE;

    // Top-anchored stack: each button is placed below the previous one with
    // its own gap, so adjusting a gap moves only the buttons below it.
    float y = screenHeight_ * K_STACK_TOP_FRACTION;

    // Each button is centred horizontally according to its own width.
    auto centered = [&]( float w, float h, float top ) {
        return sf::FloatRect( { ( screenWidth_ - w ) * 0.5f, top }, { w, h } );
    };

    newGameButtonBounds_ = centered( K_BUTTON_WIDTH, K_BUTTON_HEIGHT, y );
    y += K_BUTTON_HEIGHT + K_GAP_NEW_TO_CUSTOM;
    customSettingsButtonBounds_ = centered( custom_w, custom_h, y );
    y += custom_h + K_GAP_CUSTOM_TO_QUIT;
    quitButtonBounds_ = centered( quit_w, quit_h, y );

    auto place_sprite = [&]( std::unique_ptr<sf::Sprite>& sprite,
                                     const sf::Texture& texture,
                                     const sf::FloatRect& bounds ) {
        if ( ! sprite ) {
            return;
        }
        const sf::Vector2u ts = texture.getSize( );
        if ( ts.x > 0 && ts.y > 0 ) {
            sprite->setScale( { bounds.size.x / static_cast<float>( ts.x ),
                                bounds.size.y / static_cast<float>( ts.y ) } );
        }
        sprite->setPosition( bounds.position );
    };
    place_sprite( newGameButtonSprite_, newGameButtonTexture_, newGameButtonBounds_ );
    place_sprite( customSettingsButtonSprite_, customSettingsButtonTexture_,
                      customSettingsButtonBounds_ );
    place_sprite( quitButtonSprite_, quitButtonTexture_, quitButtonBounds_ );

    if ( messageText_ ) {
        messageText_->setPosition( { 16.0f, screenHeight_ - 32.0f } );
    }
}

bool SfmlMainMenuView::isOpen( ) const {
    return window_.isOpen( );
}

void SfmlMainMenuView::processEvents( MainMenuPresenter& presenter ) {
    while ( const std::optional<sf::Event> event = window_.pollEvent( ) ) {
        if ( event->is<sf::Event::Closed>( ) ) {
            window_.close( );
            continue;
        }

        if ( event->is<sf::Event::Resized>( ) ) {
            views::applyLetterboxView( window_, screenWidth_, screenHeight_ );
            continue;
        }

        if ( const auto* mouse_move = event->getIf<sf::Event::MouseMoved>( ) ) {
            const sf::Vector2f world = window_.mapPixelToCoords( mouse_move->position );
            newGameButtonHovered_ = newGameButtonBounds_.contains( { world.x, world.y } );
            customSettingsButtonHovered_ = customSettingsButtonBounds_.contains( { world.x, world.y } );
            quitButtonHovered_ = quitButtonBounds_.contains( { world.x, world.y } );
            continue;
        }

        if ( const auto* mouse_press = event->getIf<sf::Event::MouseButtonPressed>( ) ) {
            if ( mouse_press->button != sf::Mouse::Button::Left ) {
                continue;
            }
            const sf::Vector2f world = window_.mapPixelToCoords( mouse_press->position );
            routeMenuClick( world.x, world.y, presenter );
            continue;
        }
    }
}

bool SfmlMainMenuView::routeMenuClick( float x,
                                          float y,
                                          MainMenuPresenter& presenter ) {
    if ( newGameButtonBounds_.contains( { x, y } ) ) {
        presenter.onNewGameClicked( );
        return true;
    }
    if ( customSettingsButtonBounds_.contains( { x, y } ) ) {
        presenter.onArmySetupClicked( );
        return true;
    }
    if ( quitButtonBounds_.contains( { x, y } ) ) {
        presenter.onQuitClicked( );
        return true;
    }
    return false;
}

void SfmlMainMenuView::render( ) {
    window_.clear( sf::Color( 12, 12, 18 ) );
    drawBackground( );
    drawNewGameButton( );
    drawCustomSettingsButton( );
    drawQuitButton( );
    drawMessage( );
    window_.display( );
}

void SfmlMainMenuView::drawBackground( ) {
    if ( backgroundLoaded_ && backgroundSprite_ ) {
        window_.draw( *backgroundSprite_ );
        return;
    }
    sf::RectangleShape fallback( { screenWidth_, screenHeight_ } );
    fallback.setPosition( { 0.0f, 0.0f } );
    fallback.setFillColor( sf::Color( 22, 18, 36 ) );
    window_.draw( fallback );
}

void SfmlMainMenuView::drawNewGameButton( ) {
    if ( newGameButtonLoaded_ && newGameButtonSprite_ ) {
        newGameButtonSprite_->setColor( newGameButtonHovered_
                                            ? sf::Color( 220, 220, 220 )
                                            : sf::Color::White );
        window_.draw( *newGameButtonSprite_ );
        return;
    }
    sf::RectangleShape fallback( newGameButtonBounds_.size );
    fallback.setPosition( newGameButtonBounds_.position );
    fallback.setFillColor( newGameButtonHovered_ ? sf::Color( 90, 70, 30 )
                                                 : sf::Color( 60, 45, 20 ) );
    fallback.setOutlineColor( sf::Color( 200, 180, 110 ) );
    fallback.setOutlineThickness( 2.0f );
    window_.draw( fallback );
}

void SfmlMainMenuView::drawCustomSettingsButton( ) {
    if ( customSettingsButtonLoaded_ && customSettingsButtonSprite_ ) {
        customSettingsButtonSprite_->setColor( customSettingsButtonHovered_
                                              ? sf::Color( 220, 220, 220 )
                                              : sf::Color::White );
        window_.draw( *customSettingsButtonSprite_ );
        return;
    }
    sf::RectangleShape fallback( customSettingsButtonBounds_.size );
    fallback.setPosition( customSettingsButtonBounds_.position );
    fallback.setFillColor( customSettingsButtonHovered_ ? sf::Color( 30, 70, 90 )
                                                   : sf::Color( 20, 45, 60 ) );
    fallback.setOutlineColor( sf::Color( 110, 180, 200 ) );
    fallback.setOutlineThickness( 2.0f );
    window_.draw( fallback );
}

void SfmlMainMenuView::drawQuitButton( ) {
    if ( quitButtonLoaded_ && quitButtonSprite_ ) {
        quitButtonSprite_->setColor( quitButtonHovered_ ? sf::Color( 220, 220, 220 )
                                                        : sf::Color::White );
        window_.draw( *quitButtonSprite_ );
        return;
    }
    sf::RectangleShape fallback( quitButtonBounds_.size );
    fallback.setPosition( quitButtonBounds_.position );
    fallback.setFillColor( quitButtonHovered_ ? sf::Color( 90, 40, 40 )
                                              : sf::Color( 60, 25, 25 ) );
    fallback.setOutlineColor( sf::Color( 200, 110, 110 ) );
    fallback.setOutlineThickness( 2.0f );
    window_.draw( fallback );
}

void SfmlMainMenuView::drawMessage( ) {
    if ( latestMessage_.empty( ) || ! messageText_ ) {
        return;
    }
    messageText_->setString( latestMessage_ );
    window_.draw( *messageText_ );
}

void SfmlMainMenuView::showMessage( const std::string& msg ) {
    latestMessage_ = msg;
}

} // namespace views
