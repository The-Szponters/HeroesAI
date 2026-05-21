/**
 * @file SfmlArmySetupView.cc
 * @brief Implementation of the SFML-backed army setup renderer.
 * @author Łukasz Szydlik
 */
#include <array>
#include <optional>
#include <sstream>

#include "../models/UnitFactory.h"
#include "../models/UnitId.h"
#include "../presenters/ArmySetupPresenter.h"
#include "SfmlArmySetupView.h"
#include "ViewportUtils.h"

namespace views {

using presenters::ArmySetupPresenter;

namespace {

constexpr int K_UNIT_COUNT = 42;

constexpr float K_SLOT_WIDTH = 490.0f;
constexpr float K_SLOT_HEIGHT = 85.0f;
constexpr float K_SLOT_VERTICAL_GAP = 10.0f;
constexpr float K_SLOT_STACK_TOP = 100.0f;

constexpr float K_LEFT_COLUMN_X = 50.0f;
constexpr float K_RIGHT_COLUMN_X = 740.0f;

constexpr float K_ICON_INSET = 8.0f;
constexpr float K_ICON_SIZE = 70.0f;

constexpr float K_COUNT_AREA_X = 340.0f;
constexpr float K_COUNT_AREA_WIDTH = 70.0f;

constexpr float K_PLUS_MINUS_SIZE = 30.0f;
constexpr float K_MINUS_OFFSET_X = 410.0f;
constexpr float K_PLUS_OFFSET_X = 450.0f;

constexpr float K_BACK_BUTTON_WIDTH = 280.0f;
constexpr float K_BACK_BUTTON_HEIGHT = 80.0f;

constexpr float K_PICKER_PANEL_X = 90.0f;
constexpr float K_PICKER_PANEL_Y = 80.0f;
constexpr float K_PICKER_PANEL_WIDTH = 1100.0f;
constexpr float K_PICKER_PANEL_HEIGHT = 800.0f;

constexpr float K_PICKER_HEADER_Y = 95.0f;
constexpr float K_PICKER_EMPTY_X = 110.0f;
constexpr float K_PICKER_EMPTY_Y = 130.0f;
constexpr float K_PICKER_EMPTY_WIDTH = 280.0f;
constexpr float K_PICKER_EMPTY_HEIGHT = 60.0f;

constexpr int K_PICKER_COLS = 6;
constexpr int K_PICKER_ROWS = 7;
constexpr float K_PICKER_CELL_WIDTH = 170.0f;
constexpr float K_PICKER_CELL_HEIGHT = 76.0f;
constexpr float K_PICKER_CELL_GAP = 6.0f;
constexpr float K_PICKER_GRID_Y = 210.0f;
constexpr float K_PICKER_GRID_X = 115.0f;

constexpr int K_EMPTY_CELL_INDEX = -1;

constexpr unsigned int K_HEADER_FONT_SIZE = 26;
constexpr unsigned int K_NAME_FONT_SIZE = 14;
constexpr unsigned int K_COUNT_FONT_SIZE = 22;
constexpr unsigned int K_BUTTON_FONT_SIZE = 22;
constexpr unsigned int K_MESSAGE_FONT_SIZE = 18;

bool loadFontFromCandidates( sf::Font& font ) {
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
        if ( font.openFromFile( path ) ) {
            return true;
        }
    }
    return false;
}

} // namespace

SfmlArmySetupView::SfmlArmySetupView( sf::RenderWindow& window )
    : window_( window ),
      screenWidth_( window.getView( ).getSize( ).x ),
      screenHeight_( window.getView( ).getSize( ).y ),
      backButtonBounds_( { 0.0f, 0.0f }, { K_BACK_BUTTON_WIDTH, K_BACK_BUTTON_HEIGHT } ),
      pickerPanelBounds_( { K_PICKER_PANEL_X, K_PICKER_PANEL_Y },
                                  { K_PICKER_PANEL_WIDTH, K_PICKER_PANEL_HEIGHT } ),
      pickerEmptyCellBounds_( { K_PICKER_EMPTY_X, K_PICKER_EMPTY_Y },
                                    { K_PICKER_EMPTY_WIDTH, K_PICKER_EMPTY_HEIGHT } ) {
    fontLoaded_ = loadFontFromCandidates( font_ );
    if ( fontLoaded_ ) {
        headerText_ = std::make_unique<sf::Text>( font_ );
        headerText_->setCharacterSize( K_HEADER_FONT_SIZE );
        headerText_->setFillColor( sf::Color( 240, 235, 200 ) );
        headerText_->setStyle( sf::Text::Bold );

        slotNameText_ = std::make_unique<sf::Text>( font_ );
        slotNameText_->setCharacterSize( K_NAME_FONT_SIZE );
        slotNameText_->setFillColor( sf::Color::White );

        slotCountText_ = std::make_unique<sf::Text>( font_ );
        slotCountText_->setCharacterSize( K_COUNT_FONT_SIZE );
        slotCountText_->setFillColor( sf::Color( 255, 230, 120 ) );
        slotCountText_->setStyle( sf::Text::Bold );

        slotButtonText_ = std::make_unique<sf::Text>( font_ );
        slotButtonText_->setCharacterSize( K_BUTTON_FONT_SIZE );
        slotButtonText_->setFillColor( sf::Color::White );
        slotButtonText_->setStyle( sf::Text::Bold );

        pickerLabelText_ = std::make_unique<sf::Text>( font_ );
        pickerLabelText_->setCharacterSize( K_NAME_FONT_SIZE );
        pickerLabelText_->setFillColor( sf::Color::White );

        messageText_ = std::make_unique<sf::Text>( font_ );
        messageText_->setCharacterSize( K_MESSAGE_FONT_SIZE );
        messageText_->setFillColor( sf::Color( 230, 230, 230 ) );
    }

    defManager_.setSearchRoots( {
        "assets/units",
        "assets/ui",
    } );

    // Build the unit catalog once -- name + asset filename + optional
    // portrait rect (sourced from the shared sprite atlas) for every UnitID.
    unitCatalog_.reserve( K_UNIT_COUNT );
    for ( int i = 0; i < K_UNIT_COUNT; ++i ) {
        const models::UnitID id = static_cast<models::UnitID>( i );
        UnitEntry entry;
        entry.displayName_ = models::UnitFactory::idToString( id );
        try {
            auto unit = models::UnitFactory::createUnit( id, 1 );
            if ( unit ) {
                entry.assetFilename_ = unit->getAssetFilename( );
            }
        } catch ( const std::exception& ) {}
        const auto portrait = models::UnitFactory::getPortraitRect( id );
        if ( portrait.has_value( ) && portrait->w_ > 0 && portrait->h_ > 0 ) {
            entry.hasPortrait_ = true;
            entry.portraitRect_ = sf::IntRect( { portrait->x_, portrait->y_ },
                                                       { portrait->w_, portrait->h_ } );
        }
        unitCatalog_.push_back( std::move( entry ) );
    }

    loadAssets( );
    layout( );

    window_.setMouseCursorVisible( true );
}

void SfmlArmySetupView::loadAssets( ) {
    if ( backgroundTexture_.loadFromFile( "assets/ui/army_setup/army_setup_bg.jpg" ) ) {
        backgroundSprite_ = std::make_unique<sf::Sprite>( backgroundTexture_ );
        const sf::Vector2u ts = backgroundTexture_.getSize( );
        if ( ts.x > 0 && ts.y > 0 ) {
            backgroundSprite_->setScale( { screenWidth_ / static_cast<float>( ts.x ),
                                            screenHeight_ / static_cast<float>( ts.y ) } );
        }
        backgroundSprite_->setPosition( { 0.0f, 0.0f } );
        backgroundLoaded_ = true;
    }

    if ( backButtonTexture_.loadFromFile( "assets/ui/army_setup/back_button.psd" ) ) {
        backButtonSprite_ = std::make_unique<sf::Sprite>( backButtonTexture_ );
        backButtonLoaded_ = true;
    }

    if ( portraitAtlasTexture_.loadFromFile( "assets/ui/army_setup/creatures_portraits.png" ) ) {
        portraitAtlasLoaded_ = true;
    }
}

void SfmlArmySetupView::layout( ) {
    auto layoutColumn = [&]( std::array<SlotUi, 7>& slots, float column_x ) {
        for ( int i = 0; i < static_cast<int>( slots.size( ) ); ++i ) {
            const float y =
                K_SLOT_STACK_TOP + static_cast<float>( i ) * ( K_SLOT_HEIGHT + K_SLOT_VERTICAL_GAP );
            slots[i].bounds_ =
                sf::FloatRect( { column_x, y }, { K_SLOT_WIDTH, K_SLOT_HEIGHT } );
            const float btn_y = y + ( K_SLOT_HEIGHT - K_PLUS_MINUS_SIZE ) * 0.5f;
            slots[i].minusBounds_ =
                sf::FloatRect( { column_x + K_MINUS_OFFSET_X, btn_y },
                                   { K_PLUS_MINUS_SIZE, K_PLUS_MINUS_SIZE } );
            slots[i].plusBounds_ =
                sf::FloatRect( { column_x + K_PLUS_OFFSET_X, btn_y },
                                   { K_PLUS_MINUS_SIZE, K_PLUS_MINUS_SIZE } );
        }
    };
    layoutColumn( leftSlots_, K_LEFT_COLUMN_X );
    layoutColumn( rightSlots_, K_RIGHT_COLUMN_X );

    const float back_x = ( screenWidth_ - K_BACK_BUTTON_WIDTH ) * 0.5f;
    const float back_y = screenHeight_ - K_BACK_BUTTON_HEIGHT - 25.0f;
    backButtonBounds_ =
        sf::FloatRect( { back_x, back_y }, { K_BACK_BUTTON_WIDTH, K_BACK_BUTTON_HEIGHT } );
    if ( backButtonSprite_ ) {
        const sf::Vector2u ts = backButtonTexture_.getSize( );
        if ( ts.x > 0 && ts.y > 0 ) {
            backButtonSprite_->setScale(
                { K_BACK_BUTTON_WIDTH / static_cast<float>( ts.x ),
                  K_BACK_BUTTON_HEIGHT / static_cast<float>( ts.y ) } );
        }
        backButtonSprite_->setPosition( { back_x, back_y } );
    }

    pickerCellBounds_.clear( );
    pickerCellBounds_.reserve( K_UNIT_COUNT );
    for ( int i = 0; i < K_UNIT_COUNT; ++i ) {
        const int col = i % K_PICKER_COLS;
        const int row = i / K_PICKER_COLS;
        const float x =
            K_PICKER_GRID_X + static_cast<float>( col ) * ( K_PICKER_CELL_WIDTH + K_PICKER_CELL_GAP );
        const float y =
            K_PICKER_GRID_Y + static_cast<float>( row ) * ( K_PICKER_CELL_HEIGHT + K_PICKER_CELL_GAP );
        pickerCellBounds_.emplace_back( sf::Vector2f{ x, y },
                                          sf::Vector2f{ K_PICKER_CELL_WIDTH, K_PICKER_CELL_HEIGHT } );
    }
}

bool SfmlArmySetupView::isOpen( ) const {
    return window_.isOpen( );
}

void SfmlArmySetupView::processEvents( ArmySetupPresenter& presenter ) {
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
            updateHover( world.x, world.y, presenter.pickerOpen( ) );
            continue;
        }

        if ( const auto* mouse_press = event->getIf<sf::Event::MouseButtonPressed>( ) ) {
            if ( mouse_press->button != sf::Mouse::Button::Left ) {
                continue;
            }
            const sf::Vector2f world = window_.mapPixelToCoords( mouse_press->position );
            routeClick( world.x, world.y, presenter );
            continue;
        }
    }
}

void SfmlArmySetupView::updateHover( float x, float y, bool picker_open ) {
    if ( picker_open ) {
        pickerHoveredCell_ = -2;
        if ( pickerEmptyCellBounds_.contains( { x, y } ) ) {
            pickerHoveredCell_ = K_EMPTY_CELL_INDEX;
        } else {
            for ( int i = 0; i < static_cast<int>( pickerCellBounds_.size( ) ); ++i ) {
                if ( pickerCellBounds_[i].contains( { x, y } ) ) {
                    pickerHoveredCell_ = i;
                    break;
                }
            }
        }
        return;
    }

    auto refresh = [&]( std::array<SlotUi, 7>& slots ) {
        for ( SlotUi& slot : slots ) {
            slot.hovered_ = slot.bounds_.contains( { x, y } );
            slot.minusHovered_ = slot.minusBounds_.contains( { x, y } );
            slot.plusHovered_ = slot.plusBounds_.contains( { x, y } );
        }
    };
    refresh( leftSlots_ );
    refresh( rightSlots_ );
    backButtonHovered_ = backButtonBounds_.contains( { x, y } );
}

bool SfmlArmySetupView::routeClick( float x, float y, ArmySetupPresenter& presenter ) {
    if ( presenter.pickerOpen( ) ) {
        if ( pickerEmptyCellBounds_.contains( { x, y } ) ) {
            presenter.onPickerCellClicked( K_EMPTY_CELL_INDEX );
            return true;
        }
        for ( int i = 0; i < static_cast<int>( pickerCellBounds_.size( ) ); ++i ) {
            if ( pickerCellBounds_[i].contains( { x, y } ) ) {
                presenter.onPickerCellClicked( i );
                return true;
            }
        }
        // Click outside any cell but inside or outside panel -> cancel.
        presenter.onPickerCancelled( );
        return true;
    }

    if ( backButtonBounds_.contains( { x, y } ) ) {
        presenter.onBackClicked( );
        return true;
    }

    auto check_column = [&]( std::array<SlotUi, 7>& slots, int side ) -> bool {
        for ( int i = 0; i < static_cast<int>( slots.size( ) ); ++i ) {
            const SlotUi& slot = slots[i];
            if ( slot.minusBounds_.contains( { x, y } ) ) {
                presenter.onCountChanged( side, i, -1 );
                return true;
            }
            if ( slot.plusBounds_.contains( { x, y } ) ) {
                presenter.onCountChanged( side, i, +1 );
                return true;
            }
            if ( slot.bounds_.contains( { x, y } ) ) {
                presenter.onSlotClicked( side, i );
                return true;
            }
        }
        return false;
    };
    if ( check_column( leftSlots_, 0 ) ) {
        return true;
    }
    if ( check_column( rightSlots_, 1 ) ) {
        return true;
    }
    return false;
}

void SfmlArmySetupView::render( ArmySetupPresenter& presenter ) {
    window_.clear( sf::Color( 12, 12, 18 ) );

    drawBackground( );

    if ( headerText_ ) {
        headerText_->setString( "Left army" );
        headerText_->setPosition(
            { K_LEFT_COLUMN_X + ( K_SLOT_WIDTH - 200.0f ) * 0.5f, K_PICKER_HEADER_Y - 60.0f } );
        window_.draw( *headerText_ );
        headerText_->setString( "Right army" );
        headerText_->setPosition(
            { K_RIGHT_COLUMN_X + ( K_SLOT_WIDTH - 220.0f ) * 0.5f, K_PICKER_HEADER_Y - 60.0f } );
        window_.draw( *headerText_ );
    }

    for ( int i = 0; i < static_cast<int>( leftSlots_.size( ) ); ++i ) {
        drawSlot( leftSlots_[i], 0, i, presenter );
    }
    for ( int i = 0; i < static_cast<int>( rightSlots_.size( ) ); ++i ) {
        drawSlot( rightSlots_[i], 1, i, presenter );
    }

    drawBackButton( );

    if ( presenter.pickerOpen( ) ) {
        drawPickerOverlay( presenter );
    }

    drawMessage( );

    window_.display( );
}

void SfmlArmySetupView::drawBackground( ) {
    if ( backgroundLoaded_ && backgroundSprite_ ) {
        window_.draw( *backgroundSprite_ );
        return;
    }
    sf::RectangleShape fallback( { screenWidth_, screenHeight_ } );
    fallback.setPosition( { 0.0f, 0.0f } );
    fallback.setFillColor( sf::Color( 18, 22, 36 ) );
    window_.draw( fallback );
}

void SfmlArmySetupView::drawSlot( const SlotUi& slot_ui,
                                       int side,
                                       int slot_index,
                                       ArmySetupPresenter& presenter ) {
    const core::ArmyConfig& army = ( side == 0 ) ? presenter.leftArmy( ) : presenter.rightArmy( );
    const core::ArmySlot& slot = army[slot_index];

    sf::RectangleShape frame( { K_SLOT_WIDTH, K_SLOT_HEIGHT } );
    frame.setPosition( slot_ui.bounds_.position );
    frame.setFillColor( slot_ui.hovered_ ? sf::Color( 30, 30, 50, 220 )
                                         : sf::Color( 18, 18, 30, 200 ) );
    frame.setOutlineColor( sf::Color( 110, 130, 170 ) );
    frame.setOutlineThickness( 1.5f );
    window_.draw( frame );

    const sf::Vector2f slot_pos = slot_ui.bounds_.position;
    const sf::FloatRect icon_rect( { slot_pos.x + K_ICON_INSET,
                                          slot_pos.y + ( K_SLOT_HEIGHT - K_ICON_SIZE ) * 0.5f },
                                          { K_ICON_SIZE, K_ICON_SIZE } );

    if ( slot.unitId_.has_value( ) ) {
        const int idx = static_cast<int>( *slot.unitId_ );
        if ( idx >= 0 && idx < static_cast<int>( unitCatalog_.size( ) ) ) {
            drawUnitIcon( unitCatalog_[idx], icon_rect );
            if ( slotNameText_ ) {
                slotNameText_->setString( unitCatalog_[idx].displayName_ );
                slotNameText_->setCharacterSize( K_NAME_FONT_SIZE + 4 );
                slotNameText_->setPosition(
                    { slot_pos.x + K_ICON_SIZE + 20.0f, slot_pos.y + 22.0f } );
                window_.draw( *slotNameText_ );
                slotNameText_->setCharacterSize( K_NAME_FONT_SIZE );
            }
        }
    } else {
        sf::RectangleShape empty_icon( { K_ICON_SIZE, K_ICON_SIZE } );
        empty_icon.setPosition( icon_rect.position );
        empty_icon.setFillColor( sf::Color( 30, 30, 40 ) );
        empty_icon.setOutlineColor( sf::Color( 80, 80, 100 ) );
        empty_icon.setOutlineThickness( 1.0f );
        window_.draw( empty_icon );
        if ( slotNameText_ ) {
            slotNameText_->setString( "(empty)" );
            slotNameText_->setCharacterSize( K_NAME_FONT_SIZE + 4 );
            slotNameText_->setPosition(
                { slot_pos.x + K_ICON_SIZE + 20.0f, slot_pos.y + 22.0f } );
            window_.draw( *slotNameText_ );
            slotNameText_->setCharacterSize( K_NAME_FONT_SIZE );
        }
    }

    if ( slotCountText_ ) {
        slotCountText_->setString( std::to_string( slot.count_ ) );
        const sf::FloatRect tb = slotCountText_->getLocalBounds( );
        const float count_x =
            slot_pos.x + K_COUNT_AREA_X + ( K_COUNT_AREA_WIDTH - tb.size.x ) * 0.5f - tb.position.x;
        const float count_y = slot_pos.y + ( K_SLOT_HEIGHT - tb.size.y ) * 0.5f - tb.position.y;
        slotCountText_->setPosition( { count_x, count_y } );
        window_.draw( *slotCountText_ );
    }

    auto draw_pm_button = [&]( const sf::FloatRect& bounds,
                                       const std::string& label,
                                       bool hovered ) {
        sf::RectangleShape rect( bounds.size );
        rect.setPosition( bounds.position );
        rect.setFillColor( hovered ? sf::Color( 80, 100, 140 ) : sf::Color( 45, 60, 90 ) );
        rect.setOutlineColor( sf::Color( 140, 180, 220 ) );
        rect.setOutlineThickness( 1.5f );
        window_.draw( rect );
        if ( slotButtonText_ ) {
            slotButtonText_->setString( label );
            const sf::FloatRect tb = slotButtonText_->getLocalBounds( );
            slotButtonText_->setPosition(
                { bounds.position.x + ( bounds.size.x - tb.size.x ) * 0.5f - tb.position.x,
                  bounds.position.y + ( bounds.size.y - tb.size.y ) * 0.5f - tb.position.y } );
            window_.draw( *slotButtonText_ );
        }
    };
    draw_pm_button( slot_ui.minusBounds_, "-", slot_ui.minusHovered_ );
    draw_pm_button( slot_ui.plusBounds_, "+", slot_ui.plusHovered_ );
}

void SfmlArmySetupView::drawBackButton( ) {
    if ( backButtonLoaded_ && backButtonSprite_ ) {
        backButtonSprite_->setColor( backButtonHovered_ ? sf::Color( 220, 220, 220 )
                                                        : sf::Color::White );
        window_.draw( *backButtonSprite_ );
        return;
    }
    sf::RectangleShape fallback( { K_BACK_BUTTON_WIDTH, K_BACK_BUTTON_HEIGHT } );
    fallback.setPosition( backButtonBounds_.position );
    fallback.setFillColor( backButtonHovered_ ? sf::Color( 90, 70, 30 )
                                              : sf::Color( 60, 45, 20 ) );
    fallback.setOutlineColor( sf::Color( 200, 180, 110 ) );
    fallback.setOutlineThickness( 2.0f );
    window_.draw( fallback );
    if ( slotButtonText_ ) {
        slotButtonText_->setString( "Back" );
        const sf::FloatRect tb = slotButtonText_->getLocalBounds( );
        slotButtonText_->setPosition(
            { backButtonBounds_.position.x +
                  ( K_BACK_BUTTON_WIDTH - tb.size.x ) * 0.5f - tb.position.x,
              backButtonBounds_.position.y +
                  ( K_BACK_BUTTON_HEIGHT - tb.size.y ) * 0.5f - tb.position.y } );
        window_.draw( *slotButtonText_ );
    }
}

void SfmlArmySetupView::drawPickerOverlay( ArmySetupPresenter& presenter ) {
    sf::RectangleShape backdrop( { screenWidth_, screenHeight_ } );
    backdrop.setPosition( { 0.0f, 0.0f } );
    backdrop.setFillColor( sf::Color( 0, 0, 0, 170 ) );
    window_.draw( backdrop );

    sf::RectangleShape panel( pickerPanelBounds_.size );
    panel.setPosition( pickerPanelBounds_.position );
    panel.setFillColor( sf::Color( 22, 25, 38, 240 ) );
    panel.setOutlineColor( sf::Color( 140, 170, 210 ) );
    panel.setOutlineThickness( 2.0f );
    window_.draw( panel );

    if ( headerText_ ) {
        std::ostringstream os;
        os << "Select unit  --  " << ( presenter.pickerSide( ) == 0 ? "Left" : "Right" )
           << " army  --  slot " << ( presenter.pickerSlot( ) + 1 );
        headerText_->setString( os.str( ) );
        headerText_->setPosition( { K_PICKER_PANEL_X + 20.0f, K_PICKER_HEADER_Y - 5.0f } );
        window_.draw( *headerText_ );
    }

    // Empty cell.
    {
        sf::RectangleShape cell( pickerEmptyCellBounds_.size );
        cell.setPosition( pickerEmptyCellBounds_.position );
        const bool hovered = pickerHoveredCell_ == K_EMPTY_CELL_INDEX;
        cell.setFillColor( hovered ? sf::Color( 60, 60, 90 ) : sf::Color( 35, 35, 55 ) );
        cell.setOutlineColor( sf::Color( 160, 170, 200 ) );
        cell.setOutlineThickness( 1.5f );
        window_.draw( cell );
        if ( pickerLabelText_ ) {
            pickerLabelText_->setString( "(empty slot)" );
            pickerLabelText_->setCharacterSize( K_NAME_FONT_SIZE + 4 );
            const sf::FloatRect tb = pickerLabelText_->getLocalBounds( );
            pickerLabelText_->setPosition(
                { pickerEmptyCellBounds_.position.x +
                      ( pickerEmptyCellBounds_.size.x - tb.size.x ) * 0.5f - tb.position.x,
                  pickerEmptyCellBounds_.position.y +
                      ( pickerEmptyCellBounds_.size.y - tb.size.y ) * 0.5f - tb.position.y } );
            window_.draw( *pickerLabelText_ );
            pickerLabelText_->setCharacterSize( K_NAME_FONT_SIZE );
        }
    }

    // 42 unit cells.
    for ( int i = 0; i < static_cast<int>( pickerCellBounds_.size( ) ); ++i ) {
        const sf::FloatRect& bounds = pickerCellBounds_[i];
        const bool hovered = pickerHoveredCell_ == i;

        sf::RectangleShape cell( bounds.size );
        cell.setPosition( bounds.position );
        cell.setFillColor( hovered ? sf::Color( 60, 60, 90 ) : sf::Color( 35, 35, 55 ) );
        cell.setOutlineColor( sf::Color( 110, 130, 170 ) );
        cell.setOutlineThickness( 1.0f );
        window_.draw( cell );

        const sf::FloatRect icon_rect( { bounds.position.x + 6.0f, bounds.position.y + 6.0f },
                                              { 60.0f, 60.0f } );
        if ( i < static_cast<int>( unitCatalog_.size( ) ) ) {
            drawUnitIcon( unitCatalog_[i], icon_rect );
            if ( pickerLabelText_ ) {
                pickerLabelText_->setString( unitCatalog_[i].displayName_ );
                pickerLabelText_->setCharacterSize( K_NAME_FONT_SIZE );
                pickerLabelText_->setPosition(
                    { bounds.position.x + 72.0f, bounds.position.y + 20.0f } );
                window_.draw( *pickerLabelText_ );
            }
        }
    }
}

void SfmlArmySetupView::drawUnitIcon( const UnitEntry& entry,
                                            const sf::FloatRect& target,
                                            sf::Color tint ) {
    // Draw the portrait from the shared atlas. When the unit lacks
    // portrait coordinates in units.json we deliberately render nothing
    // (no .def sprite fallback) -- the empty slot stays blank so the
    // missing-data case is visually obvious.
    if ( ! entry.hasPortrait_ || ! portraitAtlasLoaded_ ) {
        return;
    }

    const float src_w = static_cast<float>( entry.portraitRect_.size.x );
    const float src_h = static_cast<float>( entry.portraitRect_.size.y );
    if ( src_w <= 0.0f || src_h <= 0.0f ) {
        return;
    }

    sf::Sprite icon( portraitAtlasTexture_ );
    icon.setTextureRect( entry.portraitRect_ );
    const float scale_x = target.size.x / src_w;
    const float scale_y = target.size.y / src_h;
    const float scale = std::min( scale_x, scale_y );
    icon.setScale( { scale, scale } );
    const float rendered_w = src_w * scale;
    const float rendered_h = src_h * scale;
    icon.setPosition( { target.position.x + ( target.size.x - rendered_w ) * 0.5f,
                            target.position.y + ( target.size.y - rendered_h ) * 0.5f } );
    icon.setColor( tint );
    window_.draw( icon );
}

void SfmlArmySetupView::drawMessage( ) {
    if ( latestMessage_.empty( ) || ! messageText_ ) {
        return;
    }
    messageText_->setString( latestMessage_ );
    messageText_->setPosition( { 16.0f, screenHeight_ - 32.0f } );
    window_.draw( *messageText_ );
}

void SfmlArmySetupView::showMessage( const std::string& msg ) {
    latestMessage_ = msg;
}

} // namespace views
