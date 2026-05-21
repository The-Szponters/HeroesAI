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
constexpr float K_BUTTON_VERTICAL_GAP = 20.0f;
constexpr unsigned int K_MESSAGE_LABEL_SIZE = 18;

} // namespace

SfmlMainMenuView::SfmlMainMenuView( sf::RenderWindow& window )
    : window_( window ),
      screenWidth_( window.getView( ).getSize( ).x ),
      screenHeight_( window.getView( ).getSize( ).y ),
      newGameButtonBounds_( { 0.0f, 0.0f }, { K_BUTTON_WIDTH, K_BUTTON_HEIGHT } ),
      armySetupButtonBounds_( { 0.0f, 0.0f }, { K_BUTTON_WIDTH, K_BUTTON_HEIGHT } ) {
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

    if ( armySetupButtonTexture_.loadFromFile( "assets/ui/menu/army_setup_button.psd" ) ) {
        armySetupButtonSprite_ = std::make_unique<sf::Sprite>( armySetupButtonTexture_ );
        armySetupButtonLoaded_ = true;
    }
}

void SfmlMainMenuView::layoutButtons( ) {
    const float button_x = ( screenWidth_ - K_BUTTON_WIDTH ) * 0.5f;
    const float stack_height = K_BUTTON_HEIGHT * 2.0f + K_BUTTON_VERTICAL_GAP;
    const float stack_top = ( screenHeight_ - stack_height ) * 0.5f + screenHeight_ * 0.10f;

    const float new_game_y = stack_top;
    const float army_setup_y = stack_top + K_BUTTON_HEIGHT + K_BUTTON_VERTICAL_GAP;

    newGameButtonBounds_ = sf::FloatRect( { button_x, new_game_y },
                                          { K_BUTTON_WIDTH, K_BUTTON_HEIGHT } );
    armySetupButtonBounds_ = sf::FloatRect( { button_x, army_setup_y },
                                            { K_BUTTON_WIDTH, K_BUTTON_HEIGHT } );

    if ( newGameButtonSprite_ ) {
        const sf::Vector2u ts = newGameButtonTexture_.getSize( );
        if ( ts.x > 0 && ts.y > 0 ) {
            newGameButtonSprite_->setScale( { K_BUTTON_WIDTH / static_cast<float>( ts.x ),
                                              K_BUTTON_HEIGHT / static_cast<float>( ts.y ) } );
        }
        newGameButtonSprite_->setPosition( { button_x, new_game_y } );
    }

    if ( armySetupButtonSprite_ ) {
        const sf::Vector2u ts = armySetupButtonTexture_.getSize( );
        if ( ts.x > 0 && ts.y > 0 ) {
            armySetupButtonSprite_->setScale( { K_BUTTON_WIDTH / static_cast<float>( ts.x ),
                                                K_BUTTON_HEIGHT / static_cast<float>( ts.y ) } );
        }
        armySetupButtonSprite_->setPosition( { button_x, army_setup_y } );
    }

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
            armySetupButtonHovered_ = armySetupButtonBounds_.contains( { world.x, world.y } );
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
    if ( armySetupButtonBounds_.contains( { x, y } ) ) {
        presenter.onArmySetupClicked( );
        return true;
    }
    return false;
}

void SfmlMainMenuView::render( ) {
    window_.clear( sf::Color( 12, 12, 18 ) );
    drawBackground( );
    drawNewGameButton( );
    drawArmySetupButton( );
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
    sf::RectangleShape fallback( { K_BUTTON_WIDTH, K_BUTTON_HEIGHT } );
    fallback.setPosition( newGameButtonBounds_.position );
    fallback.setFillColor( newGameButtonHovered_ ? sf::Color( 90, 70, 30 )
                                                 : sf::Color( 60, 45, 20 ) );
    fallback.setOutlineColor( sf::Color( 200, 180, 110 ) );
    fallback.setOutlineThickness( 2.0f );
    window_.draw( fallback );
}

void SfmlMainMenuView::drawArmySetupButton( ) {
    if ( armySetupButtonLoaded_ && armySetupButtonSprite_ ) {
        armySetupButtonSprite_->setColor( armySetupButtonHovered_
                                              ? sf::Color( 220, 220, 220 )
                                              : sf::Color::White );
        window_.draw( *armySetupButtonSprite_ );
        return;
    }
    sf::RectangleShape fallback( { K_BUTTON_WIDTH, K_BUTTON_HEIGHT } );
    fallback.setPosition( armySetupButtonBounds_.position );
    fallback.setFillColor( armySetupButtonHovered_ ? sf::Color( 30, 70, 90 )
                                                   : sf::Color( 20, 45, 60 ) );
    fallback.setOutlineColor( sf::Color( 110, 180, 200 ) );
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
