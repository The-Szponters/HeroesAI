/**
 * @file SfmlBattleView.cc
 * @brief Implementation of the SFML-backed battle renderer.
 * @author Dominik Śledziewski & Łukasz Szydlik
 */
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <unordered_set>

#include "../models/Board.h"
#include "../presenters/BattlePresenter.h"
#include "BattleLayout.h"
#include "SfmlBattleView.h"
#include "ViewportUtils.h"

namespace views {

using models::Board;
using models::Hex;
using models::Unit;
using presenters::BattlePresenter;

namespace {
constexpr float K_PI = 3.14159265358979323846f;

std::pair<int, int> cubeRoundToAxial( float fq, float fr, float fs ) {
    int rq = static_cast<int>( std::round( fq ) );
    int rr = static_cast<int>( std::round( fr ) );
    int rs = static_cast<int>( std::round( fs ) );

    const float dq = std::fabs( static_cast<float>( rq ) - fq );
    const float dr = std::fabs( static_cast<float>( rr ) - fr );
    const float ds = std::fabs( static_cast<float>( rs ) - fs );

    if ( dq > dr && dq > ds ) {
        rq = -rr - rs;
    } else if ( dr > ds ) {
        rr = -rq - rs;
    } else {
        rs = -rq - rr;
    }

    (void) rs;
    return { rq, rr };
}

} // namespace

SfmlBattleView::SfmlBattleView( sf::RenderWindow& window )
    : window_( window ),
      screenWidth_( window.getView( ).getSize( ).x ),
      screenHeight_( window.getView( ).getSize( ).y ),
      battlefieldHeight_( screenHeight_ * 0.8f ),
      hexRadius_( K_HEX_RADIUS ),

      gridOrigin_( K_GRID_ORIGIN_X, K_GRID_ORIGIN_Y ),
      hudCount_( 0 ),
      hudHpLeft_( 0 ) {
    const std::array<const char*, 11> font_candidates = {
        "assets/font.ttf",
        // Linux
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
        "/usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        // macOS
        "/System/Library/Fonts/Helvetica.ttc",
        "/Library/Fonts/Arial.ttf",
        "/System/Library/Fonts/SFNS.ttf",
        // Windows
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/segoeui.ttf",
    };
    bool font_loaded = false;
    for ( const char* path : font_candidates ) {
        if ( font_.openFromFile( path ) ) {
            font_loaded = true;
            break;
        }
    }
    if ( ! font_loaded ) {
        latestMessage_ = "Warning: no font found, HUD text will not render";
    }

    hudText_ = std::make_unique<sf::Text>( font_ );
    queueText_ = std::make_unique<sf::Text>( font_ );
    logText_ = std::make_unique<sf::Text>( font_ );
    infoPanelText_ = std::make_unique<sf::Text>( font_ );
    unitStackCountText_ = std::make_unique<sf::Text>( font_ );

    hudText_->setCharacterSize( 18 );
    hudText_->setFillColor( sf::Color::White );
    hudText_->setPosition( { 16.0f, battlefieldHeight_ + 8.0f } );

    queueText_->setCharacterSize( 16 );
    queueText_->setFillColor( sf::Color( 230, 230, 230 ) );
    queueText_->setPosition( { 16.0f, battlefieldHeight_ + 30.0f } );

    logText_->setCharacterSize( 17 );
    logText_->setFillColor( sf::Color( 220, 220, 220 ) );
    logText_->setPosition( { 16.0f, battlefieldHeight_ + 52.0f } );

    defManager_.setSearchRoots( {
        "assets/units",
        "assets/ui",
    } );

    if ( battlefieldTexture_.loadFromFile( "assets/backgrounds/CmBkGrTr.bmp" ) ) {
        const sf::Vector2u ts = battlefieldTexture_.getSize( );
        battlefieldSprite_ = std::make_unique<sf::Sprite>( battlefieldTexture_ );
        battlefieldSprite_->setScale( { screenWidth_ / static_cast<float>( ts.x ),
                                        battlefieldHeight_ / static_cast<float>( ts.y ) } );
        battlefieldSprite_->setPosition( { 0.0f, 0.0f } );
    }

    window_.setMouseCursorVisible( false );
    osCursorVisible_ = false;

    infoPanelText_->setCharacterSize( 15 );
    infoPanelText_->setFillColor( sf::Color::White );

    unitStackCountText_->setCharacterSize( 8 );
    unitStackCountText_->setFillColor( sf::Color( 20, 20, 20 ) );

    unitStackTeamBacker_.setFillColor( sf::Color::Transparent );
    unitStackTeamBacker_.setOutlineThickness( 0.0f );
    unitStackHpBack_.setFillColor( sf::Color::Black );
    unitStackHpFill_.setFillColor( sf::Color::Green );

    loadActionBarAssets( );
    loadInfoPanelAssets( );
    loadUnitStackAssets( );
    loadSpellbookAssets( );

    if ( font_loaded ) {
        spellbookText_ = std::make_unique<sf::Text>( font_ );
        spellbookText_->setCharacterSize( 14 );
        spellbookText_->setFillColor( sf::Color::White );
    }
}

void SfmlBattleView::loadActionBarAssets( ) {
    constexpr float K_ICON_W = 48.0f;
    constexpr float K_ICON_H = 36.0f;
    constexpr float K_PAD = 8.0f;
    const float icon_y = screenHeight_ - K_ICON_H - 8.0f;

    auto add = [&]( ActionKind kind, const std::string& def, float x ) {
        actionSlots_.push_back(
            { kind, def, sf::FloatRect( { x, icon_y }, { K_ICON_W, K_ICON_H } ), 0.0f } );
    };

    float x = screenWidth_ - K_ICON_W - K_PAD;
    add( ActionKind::SURRENDER, "surrender_icon.def", x );
    x -= K_ICON_W + K_PAD;
    add( ActionKind::AUTO_COMBAT, "autocombat_icon.def", x );
    x -= K_ICON_W + K_PAD;
    add( ActionKind::DEFEND, "defend_icon.def", x );
    x -= K_ICON_W + K_PAD;
    add( ActionKind::WAIT, "wait_icon.def", x );
    x -= K_ICON_W + K_PAD;
    add( ActionKind::SPELLBOOK, "spellbook.def", x );
}

void SfmlBattleView::loadInfoPanelAssets( ) {
    if ( infoPanelTexture_.loadFromFile( "assets/ui/unit_stats.bmp" ) ) {
        infoPanelSprite_ = std::make_unique<sf::Sprite>( infoPanelTexture_ );
    }
}

void SfmlBattleView::loadUnitStackAssets( ) {
    if ( unitStackBoxTexture_.loadFromFile( "assets/ui/num_units.bmp" ) ) {
        unitStackBoxSprite_ = std::make_unique<sf::Sprite>( unitStackBoxTexture_ );
    }
}

bool SfmlBattleView::isOpen( ) const {
    return window_.isOpen( );
}

void SfmlBattleView::onMouseHover( int pixel_x, int pixel_y, BattlePresenter& presenter ) {
    if ( ! isPointInBattlefield( static_cast<float>( pixel_x ),
                                    static_cast<float>( pixel_y ) ) ) {
        if ( sf::Mouse::isButtonPressed( sf::Mouse::Button::Right ) ) {
            presenter.onRightClickReleased( );
        }
        setCursorStyle( CursorStyle::STANDARD_POINTER, pixel_x, pixel_y );
        return;
    }

    if ( sf::Mouse::isButtonPressed( sf::Mouse::Button::Right ) ) {
        presenter.onRightClickPressed( pixel_x, pixel_y );
        setCursorStyle( CursorStyle::STANDARD_POINTER, pixel_x, pixel_y );
        return;
    }

    const bool shift_held = sf::Keyboard::isKeyPressed( sf::Keyboard::Key::LShift ) ||
                            sf::Keyboard::isKeyPressed( sf::Keyboard::Key::RShift );
    presenter.onMouseHover( pixel_x, pixel_y, shift_held );
}

void SfmlBattleView::processEvents( BattlePresenter& presenter ) {
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
            if ( spellbookOpen_ ) {
                spellbookHoveredCell_ = -1;
                for ( int i = 0; i < static_cast<int>( spellCellBounds_.size( ) ); ++i ) {
                    if ( spellCellBounds_[i].contains( { world.x, world.y } ) ) {
                        spellbookHoveredCell_ = i;
                        break;
                    }
                }
                continue;
            }
            onMouseHover( static_cast<int>( world.x ),
                              static_cast<int>( world.y ),
                              presenter );
            continue;
        }

        if ( const auto* mouse_press = event->getIf<sf::Event::MouseButtonPressed>( ) ) {
            const sf::Vector2f world = window_.mapPixelToCoords( mouse_press->position );
            const float mx = world.x;
            const float my = world.y;

            if ( mouse_press->button == sf::Mouse::Button::Right ) {
                presenter.onRightClickPressed( static_cast<int>( mx ),
                                                   static_cast<int>( my ) );
                continue;
            }

            if ( mouse_press->button != sf::Mouse::Button::Left ) {
                continue;
            }

            // While the spellbook is open it absorbs all left-clicks --
            // either picking a spell or dismissing the overlay.
            if ( spellbookOpen_ ) {
                routeSpellbookClick( mx, my, presenter );
                continue;
            }

            if ( routeActionClick( mx, my, presenter ) ) {
                continue;
            }

            if ( isPointInBattlefield( mx, my ) ) {
                const auto [q, r] = pixelToHex( mx, my );
                const bool shift_held = sf::Keyboard::isKeyPressed( sf::Keyboard::Key::LShift ) ||
                                        sf::Keyboard::isKeyPressed( sf::Keyboard::Key::RShift );
                presenter.onHexClicked( q, r, shift_held );
            } else {
                presenter.onRightClickReleased( );
            }
            continue;
        }

        if ( const auto* mouse_release = event->getIf<sf::Event::MouseButtonReleased>( ) ) {
            if ( mouse_release->button == sf::Mouse::Button::Right ) {
                presenter.onRightClickReleased( );
            }
            continue;
        }
    }
}

void SfmlBattleView::render( ) {
    window_.clear( sf::Color( 23, 23, 27 ) );

    const sf::Time dt = animationClock_.restart( );
    updateVisualEvents( dt );
    for ( auto& [key, controller] : animationControllers_ ) {
        (void) key;
        controller.update( dt );
    }
    pulsePhaseSeconds_ += dt.asSeconds( );

    for ( ActionSlot& slot : actionSlots_ ) {
        if ( slot.pressedSecondsLeft_ > 0.0f ) {
            slot.pressedSecondsLeft_ =
                std::max( 0.0f, slot.pressedSecondsLeft_ - dt.asSeconds( ) );
        }
    }

    updateHoverFromMouse( );

    cursorPosition_ = window_.mapPixelToCoords( sf::Mouse::getPosition( window_ ) );

    drawBattlefieldBackground( );
    drawHexGrid( );
    drawUnits( );
    drawSpellCastOverlay( dt );
    drawHud( );
    drawTurnQueue( );
    drawUnitStackUi( );
    drawInfoPanel( );
    if ( spellbookOpen_ ) {
        drawSpellbookOverlay( );
    }
    drawCursor( );

    window_.display( );
}

void SfmlBattleView::drawBattlefieldBackground( ) {
    if ( battlefieldSprite_ ) {
        window_.draw( *battlefieldSprite_ );
        return;
    }
    sf::RectangleShape battlefield_bg( { screenWidth_, battlefieldHeight_ } );
    battlefield_bg.setPosition( { 0.0f, 0.0f } );
    battlefield_bg.setFillColor( sf::Color( 54, 64, 51 ) );
    window_.draw( battlefield_bg );
}

void SfmlBattleView::drawHexGrid( ) {
    for ( int row = 0; row < Board::HEIGHT; ++row ) {
        for ( int col = 0; col < Board::WIDTH; ++col ) {
            const int q = col - ( row - ( row & 1 ) ) / 2;
            const int r = row;

            sf::ConvexShape hex = makeHexShape( q, r );
            hex.setFillColor( sf::Color::Transparent );
            hex.setOutlineColor( sf::Color( 200, 200, 220, 70 ) );
            hex.setOutlineThickness( 1.0f );

            const auto it = expandedHighlights_.find( makeHexKey( q, r ) );
            if ( it != expandedHighlights_.end( ) ) {
                switch ( it->second ) {
                case HighlightType::ACTIVE_UNIT:
                    hex.setOutlineColor( sf::Color( 70, 130, 255, 220 ) );
                    hex.setOutlineThickness( 2.5f );
                    break;
                case HighlightType::WALKABLE:
                    hex.setFillColor( sf::Color( 150, 150, 150, 90 ) );
                    break;
                case HighlightType::ATTACKABLE:
                    hex.setFillColor( sf::Color( 220, 70, 70, 90 ) );
                    break;
                case HighlightType::ATTACK_ORIGIN:
                    hex.setFillColor( sf::Color( 95, 95, 95, 150 ) );
                    break;
                case HighlightType::HOVER_DESTINATION:

                    hex.setFillColor( sf::Color( 15, 15, 15, 200 ) );
                    break;
                case HighlightType::NONE:
                    break;
                }
            }
            window_.draw( hex );
        }
    }
}

void SfmlBattleView::drawUnits( ) {
    std::uint64_t active_unit_id_for_glow = 0;
    if ( activeUnitHighlight_.has_value( ) ) {
        for ( const UnitRenderData& u : unitsToDraw_ ) {
            if ( ! u.isCorpse_ && u.q_ == activeUnitHighlight_->q_ &&
                 u.r_ == activeUnitHighlight_->r_ ) {
                active_unit_id_for_glow = u.id_;
                break;
            }
        }
    }

    auto draw_active_glow = [this]( const UnitRenderData& unit ) {
        const auto ctrl_it = animationControllers_.find( unit.id_ );
        if ( ctrl_it == animationControllers_.end( ) || ! ctrl_it->second.isReady( ) ) {
            return;
}
        const sf::Sprite* base = ctrl_it->second.getSprite( );
        if ( base == nullptr ) {
            return;
}

        constexpr float K_PULSE_HZ = 1.0f;
        const float t = 0.5f + 0.5f * std::sin( pulsePhaseSeconds_ * 2.0f * K_PI * K_PULSE_HZ );
        const float alpha_norm = 0.30f + 0.45f * t;
        const auto alpha = static_cast<std::uint8_t>( std::round( 255.0f * alpha_norm ) );

        sf::Sprite glow = *base;
        glow.setColor( sf::Color( 255, 215, 0, alpha ) );
        const sf::Vector2f s = base->getScale( );
        constexpr float K_GLOW_GROW = 1.08f;
        glow.setScale( { s.x * K_GLOW_GROW, s.y * K_GLOW_GROW } );
        window_.draw( glow, sf::BlendAdd );
    };

    auto draw_one = [this]( const UnitRenderData& unit ) {
        sf::Vector2f center = unitRenderCenter( unit );
        const auto override_it = visualPositionOverrides_.find( unit.id_ );
        if ( override_it != visualPositionOverrides_.end( ) ) {
            center = override_it->second;
        }

        const auto ctrl_it = animationControllers_.find( unit.id_ );
        if ( ctrl_it != animationControllers_.end( ) && ctrl_it->second.isReady( ) ) {
            if ( const sf::Sprite* sprite = ctrl_it->second.getSprite( ) ) {
                window_.draw( *sprite );
                return;
            }
        }

        const float radius = hexRadius_ * 0.34f;
        sf::CircleShape body( radius );
        body.setOrigin( { radius, radius } );
        body.setPosition( center );

        if ( unit.isCorpse_ ) {
            body.setFillColor( sf::Color( 120, 120, 120, 190 ) );
            body.setOutlineColor( sf::Color( 190, 190, 190, 200 ) );
        } else {
            body.setFillColor( sf::Color( 240, 210, 90, 230 ) );
            body.setOutlineColor( sf::Color( 15, 15, 15, 220 ) );
        }
        body.setOutlineThickness( 1.5f );
        window_.draw( body );

        sf::ConvexShape facing_marker( 3 );
        const float dir = unit.isFacingLeft_ ? -1.0f : 1.0f;
        facing_marker.setPoint( 0, { center.x + dir * 14.0f, center.y } );
        facing_marker.setPoint( 1, { center.x + dir * 6.0f, center.y - 6.0f } );
        facing_marker.setPoint( 2, { center.x + dir * 6.0f, center.y + 6.0f } );
        facing_marker.setFillColor( unit.isCorpse_ ? sf::Color( 95, 95, 95 )
                                                   : sf::Color( 40, 40, 40 ) );
        window_.draw( facing_marker );
    };

    for ( const UnitRenderData& unit : unitsToDraw_ ) {
        if ( unit.isCorpse_ ) {
            draw_one( unit );
}
    }
    for ( const UnitRenderData& unit : unitsToDraw_ ) {
        if ( unit.isCorpse_ ) {
            continue;
}

        if ( unit.id_ == active_unit_id_for_glow ) {
            draw_active_glow( unit );
        }
        draw_one( unit );
    }

    if ( ! visualEvents_.empty( ) &&
         visualEvents_.front( ).type_ == VisualEvent::Type::PROJECTILE ) {
        const ProjectileVisualEvent& pe = visualEvents_.front( ).projectile_;
        const float t = std::clamp( pe.elapsedSeconds_ / pe.durationSeconds_, 0.0f, 1.0f );
        const sf::Vector2f pos{
            pe.from_.x + ( pe.to_.x - pe.from_.x ) * t,
            pe.from_.y + ( pe.to_.y - pe.from_.y ) * t,
        };

        std::shared_ptr<DefResource> proj =
            pe.projectileAsset_.empty( ) ? nullptr : defManager_.getOrLoad( pe.projectileAsset_ );
        const DefFrame* frame = nullptr;
        if ( proj ) {
            for ( const auto& [gid, frames] : proj->groups_ ) {
                (void) gid;
                for ( const DefFrame& f : frames ) {
                    if ( f.width_ > 0 && f.height_ > 0 ) {
                        frame = &f;
                        break;
                    }
                }
                if ( frame ) {
                    break;
}
            }
        }

        if ( frame ) {
            sf::Sprite spr( frame->texture_ );
            const auto sz = frame->texture_.getSize( );
            spr.setOrigin( { sz.x * 0.5f, sz.y * 0.5f } );

            const float dx = pe.to_.x - pe.from_.x;
            const float dy = pe.to_.y - pe.from_.y;
            const float angle_deg = std::atan2( dy, dx ) * ( 180.0f / K_PI );
            spr.setRotation( sf::degrees( angle_deg ) );
            spr.setPosition( pos );
            window_.draw( spr );
        } else {
            sf::CircleShape dot( 4.0f );
            dot.setOrigin( { 4.0f, 4.0f } );
            dot.setPosition( pos );
            dot.setFillColor( sf::Color( 255, 230, 100, 230 ) );
            window_.draw( dot );
        }
    }

    if ( ! visualEvents_.empty( ) && visualEvents_.front( ).type_ == VisualEvent::Type::MORALE ) {
        const MoraleVisualEvent& me = visualEvents_.front( ).morale_;

        std::shared_ptr<DefResource> aura = defManager_.getOrLoad( "morale.def" );
        const UnitRenderData* unit = findUnitRenderData( me.unitId_ );
        if ( aura && unit != nullptr ) {
            const auto group_it = aura->groups_.find( 0 );
            if ( group_it != aura->groups_.end( ) && ! group_it->second.empty( ) ) {
                const std::vector<DefFrame>& frames = group_it->second;
                const float t =
                    std::clamp( me.elapsedSeconds_ / me.durationSeconds_, 0.0f, 0.999f );
                const std::size_t idx = std::min<std::size_t>(
                    frames.size( ) - 1,
                    static_cast<std::size_t>( t * static_cast<float>( frames.size( ) ) ) );
                const DefFrame& frame = frames[idx];

                if ( frame.width_ > 0 && frame.height_ > 0 ) {
                    sf::Sprite spr( frame.texture_ );
                    const auto sz = frame.texture_.getSize( );
                    sf::Vector2f center = unitRenderCenter( *unit );
                    if ( const auto override_it = visualPositionOverrides_.find( unit->id_ );
                         override_it != visualPositionOverrides_.end( ) ) {
                        center = override_it->second;
                    }

                    spr.setOrigin( { sz.x * 0.5f, static_cast<float>( sz.y ) } );
                    spr.setPosition( { center.x, center.y - hexRadius_ * 1.2f } );
                    window_.draw( spr );
                }
            }
        }
    }
}

void SfmlBattleView::drawHud( ) {
    sf::RectangleShape hud_bg( { screenWidth_, screenHeight_ - battlefieldHeight_ } );
    hud_bg.setPosition( { 0.0f, battlefieldHeight_ } );
    hud_bg.setFillColor( sf::Color( 36, 36, 42 ) );
    window_.draw( hud_bg );

    hudText_->setString( "Unit: " + hudUnitName_ + " | Count: " + std::to_string( hudCount_ ) +
                         " | HP Left: " + std::to_string( hudHpLeft_ ) );
    logText_->setString( "Log: " + latestMessage_ );

    window_.draw( *hudText_ );
    window_.draw( *logText_ );

    drawActionBar( );
}

void SfmlBattleView::drawActionBar( ) {
    for ( const ActionSlot& slot : actionSlots_ ) {
        std::shared_ptr<DefResource> res = defManager_.getOrLoad( slot.defFilename_ );
        if ( ! res ) {
            continue;
}
        const auto group_it = res->groups_.find( 0 );
        if ( group_it == res->groups_.end( ) || group_it->second.empty( ) ) {
            continue;
}

        const std::vector<DefFrame>& frames = group_it->second;
        const std::size_t want_idx =
            ( slot.pressedSecondsLeft_ > 0.0f && frames.size( ) > 2 ) ? 2u : 0u;
        const DefFrame& frame = frames[want_idx];
        if ( frame.width_ <= 0 || frame.height_ <= 0 ) {
            continue;
}

        sf::Sprite icon( frame.texture_ );
        icon.setPosition( { slot.bounds_.position.x, slot.bounds_.position.y } );
        window_.draw( icon );
    }
}

void SfmlBattleView::drawTurnQueue( ) {
    constexpr float BOX_W = 56.0f;
    constexpr float BOX_H = 56.0f;
    constexpr float DIVIDER_W = 36.0f;
    constexpr float GAP = 4.0f;
    constexpr float START_X = 16.0f;
    constexpr float K_BAR_HEIGHT = 44.0f;

    const float queue_y = screenHeight_ - K_BAR_HEIGHT - BOX_H - 6.0f;
    const float right_limit = screenWidth_ - 320.0f;
    constexpr std::size_t K_VISIBLE_CAPACITY = 12;

    float cursor_x = START_X;
    std::size_t painted = 0;

    for ( const TurnQueueSlot& slot : turnQueueSlots_ ) {
        if ( painted >= K_VISIBLE_CAPACITY ) {
            break;
}
        const float w = slot.isDivider_ ? DIVIDER_W : BOX_W;
        if ( cursor_x + w > right_limit ) {
            break;
}

        if ( slot.isDivider_ ) {
            const float divider_h = BOX_H + 14.0f;
            const float divider_y = queue_y - 7.0f;

            sf::RectangleShape divider( { DIVIDER_W, divider_h } );
            divider.setPosition( { cursor_x, divider_y } );
            divider.setFillColor( sf::Color( 110, 70, 30 ) );
            divider.setOutlineColor( sf::Color( 255, 200, 100 ) );
            divider.setOutlineThickness( 2.5f );
            window_.draw( divider );

            sf::Text round_label( font_ );
            round_label.setCharacterSize( 11 );
            round_label.setFillColor( sf::Color( 255, 235, 180 ) );
            round_label.setString( "Round\n  " + std::to_string( slot.roundNumber_ ) );
            round_label.setPosition( { cursor_x + 3.0f, divider_y + 10.0f } );
            window_.draw( round_label );
        } else {
            sf::RectangleShape box( { BOX_W, BOX_H } );
            box.setPosition( { cursor_x, queue_y } );
            if ( slot.isActive_ ) {
                box.setFillColor( sf::Color( 45, 75, 130 ) );
                box.setOutlineColor( sf::Color( 255, 215, 0 ) );
                box.setOutlineThickness( 3.5f );
            } else {
                box.setFillColor( sf::Color( 55, 55, 65 ) );
                box.setOutlineColor( sf::Color( 140, 140, 160 ) );
                box.setOutlineThickness( 1.0f );
            }
            window_.draw( box );

            sf::Text name_text( font_ );
            name_text.setCharacterSize( 12 );
            name_text.setFillColor( sf::Color::White );
            std::string short_name = slot.unitName_;
            if ( short_name.size( ) > 8 ) {
                short_name.resize( 8 );
}
            name_text.setString( short_name );
            name_text.setPosition( { cursor_x + 4.0f, queue_y + 6.0f } );
            window_.draw( name_text );

            if ( slot.isActive_ ) {
                sf::Text active_marker( font_ );
                active_marker.setCharacterSize( 11 );
                active_marker.setFillColor( sf::Color( 255, 215, 0 ) );
                active_marker.setString( "ACTIVE" );
                active_marker.setPosition( { cursor_x + 4.0f, queue_y + BOX_H - 18.0f } );
                window_.draw( active_marker );
            }
        }

        cursor_x += w + GAP;
        ++painted;
    }
}

void SfmlBattleView::drawUnitStackUi( ) {
    constexpr float K_BOX_Y_OFFSET_BELOW = 12.0f;
    constexpr float K_RIM_PAD = 1.0f;
    constexpr float K_BAR_HEIGHT = 3.0f;
    constexpr float K_BAR_GAP = 0.0f;

    const sf::Vector2u box_size_u = unitStackBoxTexture_.getSize( );
    const sf::Vector2f box_size{
        static_cast<float>( box_size_u.x > 0 ? box_size_u.x : 30u ),
        static_cast<float>( box_size_u.y > 0 ? box_size_u.y : 11u ),
    };

    for ( const UnitRenderData& unit : unitsToDraw_ ) {
        if ( unit.isCorpse_ || unit.count_ <= 0 ) {
            continue;
        }

        sf::Vector2f center = unitRenderCenter( unit );
        if ( const auto override_it = visualPositionOverrides_.find( unit.id_ );
             override_it != visualPositionOverrides_.end( ) ) {
            center = override_it->second;
        }
        if ( const auto ctrl_it = animationControllers_.find( unit.id_ );
             ctrl_it != animationControllers_.end( ) && ctrl_it->second.isReady( ) ) {
            if ( const sf::Sprite* spr = ctrl_it->second.getSprite( ) ) {
                center = spr->getPosition( );
            }
        }

        const sf::Vector2f box_pos{
            std::round( center.x - box_size.x * 0.5f ),
            std::round( center.y + K_BOX_Y_OFFSET_BELOW ),
        };

        const sf::Vector2f box_center{
            box_pos.x + box_size.x * 0.5f,
            box_pos.y + box_size.y * 0.5f,
        };

        const sf::Color rim_color = ( unit.ownerId_ == 1 )
                                        ? sf::Color( 30, 60, 200, 255 )
                                        : ( unit.ownerId_ == 0 ? sf::Color( 190, 20, 20, 255 )
                                                               : sf::Color( 90, 90, 90, 255 ) );
        const sf::Color tint_color = ( unit.ownerId_ == 1 )
                                         ? sf::Color( 0, 0, 255, 110 )
                                         : ( unit.ownerId_ == 0 ? sf::Color( 255, 0, 0, 110 )
                                                                : sf::Color( 120, 120, 120, 110 ) );

        unitStackTeamBacker_.setSize(
            { box_size.x + K_RIM_PAD * 2.0f, box_size.y + K_RIM_PAD * 2.0f } );
        unitStackTeamBacker_.setPosition( { box_pos.x - K_RIM_PAD, box_pos.y - K_RIM_PAD } );
        unitStackTeamBacker_.setFillColor( rim_color );
        unitStackTeamBacker_.setOutlineThickness( 0.0f );
        window_.draw( unitStackTeamBacker_ );

        const float hp_ratio = ( unit.maxHpPerUnit_ > 0 )
                                   ? std::clamp( static_cast<float>( unit.currentTopUnitHp_ ) /
                                                     static_cast<float>( unit.maxHpPerUnit_ ),
                                                 0.0f,
                                                 1.0f )
                                   : 0.0f;
        sf::Color hp_color = sf::Color::Red;
        if ( hp_ratio > 0.5f ) {
            hp_color = sf::Color::Green;
        } else if ( hp_ratio > 0.2f ) {
            hp_color = sf::Color::Yellow;
}

        unitStackHpBack_.setSize( { box_size.x, K_BAR_HEIGHT } );
        unitStackHpBack_.setPosition( { box_pos.x, box_pos.y - K_BAR_HEIGHT - K_BAR_GAP } );
        window_.draw( unitStackHpBack_ );

        unitStackHpFill_.setSize( { box_size.x * hp_ratio, K_BAR_HEIGHT } );
        unitStackHpFill_.setPosition( { box_pos.x, box_pos.y - K_BAR_HEIGHT - K_BAR_GAP } );
        unitStackHpFill_.setFillColor( hp_color );
        window_.draw( unitStackHpFill_ );

        if ( unitStackBoxSprite_ ) {
            unitStackBoxSprite_->setPosition( box_pos );
            window_.draw( *unitStackBoxSprite_ );
        } else {
            sf::RectangleShape fallback( box_size );
            fallback.setPosition( box_pos );
            fallback.setFillColor( sf::Color::Transparent );
            fallback.setOutlineColor( sf::Color::White );
            fallback.setOutlineThickness( 1.0f );
            window_.draw( fallback );
        }

        sf::RectangleShape tint( box_size );
        tint.setPosition( box_pos );
        tint.setFillColor( tint_color );
        window_.draw( tint );

        unitStackCountText_->setString( std::to_string( unit.count_ ) );
        const sf::FloatRect text_bounds = unitStackCountText_->getLocalBounds( );
        unitStackCountText_->setOrigin( {
            std::floor( text_bounds.position.x + text_bounds.size.x * 0.5f ),
            std::floor( text_bounds.position.y + text_bounds.size.y * 0.5f ),
        } );
        unitStackCountText_->setPosition( box_center );
        unitStackCountText_->setFillColor( sf::Color::White );
        window_.draw( *unitStackCountText_ );
    }
}

void SfmlBattleView::drawInfoPanel( ) {
    if ( ! infoPanelVisible_ || ! infoPanelUnit_.has_value( ) ) {
        return;
}

    const UnitRenderData& u = *infoPanelUnit_;

    const sf::Vector2u panel_size =
        infoPanelSprite_ ? infoPanelTexture_.getSize( ) : sf::Vector2u{ 300u, 311u };
    const sf::Vector2f panel_pos{
        ( screenWidth_ - static_cast<float>( panel_size.x ) ) * 0.5f,
        ( screenHeight_ - static_cast<float>( panel_size.y ) ) * 0.5f,
    };

    if ( infoPanelSprite_ ) {
        infoPanelSprite_->setPosition( panel_pos );
        window_.draw( *infoPanelSprite_ );
    } else {
        sf::RectangleShape fallback(
            { static_cast<float>( panel_size.x ), static_cast<float>( panel_size.y ) } );
        fallback.setPosition( panel_pos );
        fallback.setFillColor( sf::Color( 20, 20, 24, 240 ) );
        fallback.setOutlineColor( sf::Color( 175, 175, 195 ) );
        fallback.setOutlineThickness( 2.0f );
        window_.draw( fallback );
    }

    constexpr float K_PORTRAIT_W = 130.0f;
    constexpr float K_TEXT_LEFT_PAD = K_PORTRAIT_W + 12.0f;
    constexpr float K_TEXT_TOP_PAD = 24.0f;

    std::ostringstream panel;
    panel << u.name_ << ( u.isCorpse_ ? " [Corpse]" : "" ) << "\n"
          << "Attack: " << u.totalAttack_ << "\n"
          << "Defense: " << u.totalDefense_ << "\n"

          << "Shoots left: " << ( u.isRanged_ ? std::to_string( u.ammo_ ) : "" ) << "\n"
          << "Damage: " << u.totalDamageMin_ << "-" << u.totalDamageMax_ << "\n"
          << "Health: " << u.maxHpPerUnit_ << "\n"
          << "Health left: " << u.currentTopUnitHp_ << "\n"
          << "Speed: " << u.totalSpeed_;
    infoPanelText_->setString( panel.str( ) );
    infoPanelText_->setPosition( { panel_pos.x + K_TEXT_LEFT_PAD, panel_pos.y + K_TEXT_TOP_PAD } );
    window_.draw( *infoPanelText_ );
}

void SfmlBattleView::drawCursor( ) {
    // Spell-targeting mode swaps in dedicated cursor sprites:
    //   valid target  -> spellcasting_icon.def (cast cursor)
    //   anything else -> combat_icons.def first frame ("no target")
    if ( spellTargetingActive_ ) {
        const std::string asset = spellCursorIsValid_
                                       ? std::string( "spellcasting_icon.def" )
                                       : std::string( "spell_invalid_cursor.def" );
        std::shared_ptr<DefResource> res = defManager_.getOrLoad( asset );
        if ( ! res ) {
            return;
        }
        const auto group_it = res->groups_.find( 0 );
        if ( group_it == res->groups_.end( ) || group_it->second.empty( ) ) {
            return;
        }
        const DefFrame& frame = group_it->second.front( );
        if ( frame.width_ <= 0 || frame.height_ <= 0 ) {
            return;
        }
        sf::Sprite sprite( frame.texture_ );
        sprite.setOrigin( { 0.0f, 0.0f } );
        sprite.setPosition( cursorPosition_ );
        window_.draw( sprite );
        return;
    }

    if ( cursorStyle_ == CursorStyle::DEFAULT ) {
        return;
}

    std::shared_ptr<DefResource> res = defManager_.getOrLoad( "combat_icons.def" );
    if ( ! res ) {
        return;
}

    const auto group_it = res->groups_.find( 0 );
    if ( group_it == res->groups_.end( ) ) {
        return;
}
    const std::vector<DefFrame>& frames = group_it->second;

    const int frame_index = static_cast<int>( cursorStyle_ );
    if ( frame_index < 0 || frame_index >= static_cast<int>( frames.size( ) ) ) {
        return;
}

    const DefFrame& frame = frames[frame_index];
    if ( frame.width_ <= 0 || frame.height_ <= 0 ) {
        return;
}

    sf::Sprite sprite( frame.texture_ );
    const sf::Vector2u tex_size = frame.texture_.getSize( );
    sprite.setOrigin( { static_cast<float>( tex_size.x ) * 0.5f,
                          static_cast<float>( tex_size.y ) * 0.5f } );
    sprite.setPosition( cursorPosition_ );
    window_.draw( sprite );
}

void SfmlBattleView::clearAllHighlights( ) {
    highlights_.clear( );
    refreshExpandedHighlights( );
}

void SfmlBattleView::highlightHex( int q, int r, HighlightType type ) {
    highlights_[makeHexKey( q, r )] = type;
    refreshExpandedHighlights( );
}

void SfmlBattleView::updateHud( const std::string& unit_name, int count, int hp_left ) {
    hudUnitName_ = unit_name;
    hudCount_ = count;
    hudHpLeft_ = hp_left;
}

void SfmlBattleView::updateTurnOrder( const std::vector<TurnQueueSlot>& slots ) {
    turnQueueSlots_ = slots;
}

void SfmlBattleView::showMessage( const std::string& msg ) {
    latestMessage_ = msg;
}

void SfmlBattleView::setActiveUnitHighlight( int q, int r, int size, bool is_facing_left ) {
    activeUnitHighlight_ = ActiveUnitHighlight{ q, r, size, is_facing_left };
    refreshExpandedHighlights( );
}

void SfmlBattleView::clearActiveUnitHighlight( ) {
    activeUnitHighlight_.reset( );
    refreshExpandedHighlights( );
}

void SfmlBattleView::setHoverDestinationHighlight(
    int q, int r, bool has_tail, int tail_q, int tail_r ) {
    hoverDestinationHighlight_ = HoverDestinationHighlight{ q, r, has_tail, tail_q, tail_r };
    refreshExpandedHighlights( );
}

void SfmlBattleView::clearHoverDestinationHighlight( ) {
    hoverDestinationHighlight_.reset( );
    refreshExpandedHighlights( );
}

void SfmlBattleView::setAttackOriginHighlights( const std::vector<AttackOriginHex>& origins ) {
    attackOriginHighlights_ = origins;
    refreshExpandedHighlights( );
}

void SfmlBattleView::clearAttackOriginHighlights( ) {
    attackOriginHighlights_.clear( );
    refreshExpandedHighlights( );
}

void SfmlBattleView::setShiftPreviewActive( bool active ) {
    shiftPreviewActive_ = active;
    if ( active && hoverDestinationHighlight_.has_value( ) ) {
        hoverDestinationHighlight_.reset( );
        refreshExpandedHighlights( );
    }
}

void SfmlBattleView::setPredictedFacings( const std::vector<PredictedFacing>& predictions ) {
    predictedFacingByHex_.clear( );
    predictedFacingByHex_.reserve( predictions.size( ) );
    for ( const PredictedFacing& p : predictions ) {
        predictedFacingByHex_[makeHexKey( p.q_, p.r_ )] = p.facingLeft_;
    }
}

static float computeScale( const DefResource& res,
                            int unit_size,
                            float hex_radius,
                            const std::string& asset_filename ) {
    auto pick_height = []( const std::vector<DefFrame>& frames ) -> float {
        for ( const DefFrame& f : frames ) {
            if ( f.height_ > 0 ) {
                return static_cast<float>( f.height_ );
}
        }
        return 0.0f;
    };

    float content_h = 0.0f;
    if ( auto it = res.groups_.find( 1 ); it != res.groups_.end( ) ) {
        content_h = pick_height( it->second );
    }
    if ( content_h <= 0.0f ) {
        for ( const auto& [gid, frames] : res.groups_ ) {
            content_h = pick_height( frames );
            if ( content_h > 0.0f ) {
                break;
}
        }
    }
    if ( content_h <= 0.0f ) {
        return 1.0f;
}
    const float target_h = ( unit_size == 2 ) ? hex_radius * 3.5f : hex_radius * 3.0f;
    float scale = ( target_h / content_h ) * 0.8f;

    auto iequals = []( const std::string& a, const char* b ) {
        if ( a.size( ) != std::strlen( b ) ) {
            return false;
}
        for ( std::size_t i = 0; i < a.size( ); ++i ) {
            if ( std::tolower( static_cast<unsigned char>( a[i] ) ) !=
                 std::tolower( static_cast<unsigned char>( b[i] ) ) ) {
                return false;
}
}
        return true;
    };
    if ( ! iequals( asset_filename, "CHHOUN.def" ) ) {
        scale *= 1.25f;
    }
    return scale;
}

void SfmlBattleView::syncUnitPositions( ) {
    applyCurrentRenderDataToControllers( false );
}

void SfmlBattleView::updateRenderData( const std::vector<UnitRenderData>& units ) {
    modelUnitsLatest_ = units;
    if ( visualEvents_.empty( ) ) {
        unitsToDraw_ = modelUnitsLatest_;
        visualPositionOverrides_.clear( );
        applyCurrentRenderDataToControllers( false );
    }
    refreshExpandedHighlights( );
}

void SfmlBattleView::queueMoveAnimation(
    std::uint64_t unit_id, int from_q, int from_r, int to_q, int to_r, float duration_seconds ) {
    const UnitRenderData* unit = findUnitRenderData( unit_id );
    if ( unit == nullptr ) {
        return;
    }

    VisualEvent event;
    event.type_ = VisualEvent::Type::MOVE;
    event.move_.unitId_ = unit_id;
    event.move_.from_ = unitRenderCenter( *unit, from_q, from_r );
    event.move_.to_ = unitRenderCenter( *unit, to_q, to_r );
    event.move_.durationSeconds_ = std::max( 0.001f, duration_seconds );
    event.move_.isTeleporter_ = unit->isTeleporter_;
    visualEvents_.push_back( event );
}

void SfmlBattleView::queueAttackAnimation( std::uint64_t attacker_id, float ) {
    VisualEvent event;
    event.type_ = VisualEvent::Type::ATTACK;
    event.attack_.attackerId_ = attacker_id;
    visualEvents_.push_back( event );
}

void SfmlBattleView::queueAttackAnimationFacing( std::uint64_t attacker_id,
                                                    int target_q,
                                                    int target_r ) {
    VisualEvent event;
    event.type_ = VisualEvent::Type::ATTACK;
    event.attack_.attackerId_ = attacker_id;
    event.attack_.hasTargetHex_ = true;
    event.attack_.targetQ_ = target_q;
    event.attack_.targetR_ = target_r;
    visualEvents_.push_back( event );
}

void SfmlBattleView::queueProjectileAnimation( std::uint64_t attacker_id,
                                                 int target_q,
                                                 int target_r,
                                                 const std::string& projectile_asset,
                                                 float duration_seconds ) {
    VisualEvent event;
    event.type_ = VisualEvent::Type::PROJECTILE;
    event.projectile_.attackerId_ = attacker_id;
    event.projectile_.projectileAsset_ = projectile_asset;
    event.projectile_.durationSeconds_ = std::max( 0.05f, duration_seconds );

    sf::Vector2f from{ 0.0f, 0.0f };
    if ( const auto it = animationControllers_.find( attacker_id );
         it != animationControllers_.end( ) ) {
        if ( const sf::Sprite* s = it->second.getSprite( ) ) {
            from = s->getPosition( );
        }
    }
    if ( const UnitRenderData* unit = findUnitRenderData( attacker_id );
         unit && from == sf::Vector2f{ 0.0f, 0.0f } ) {
        from = unitRenderCenter( *unit );
    }

    event.projectile_.from_ = from;
    event.projectile_.to_ = hexToPixel( target_q, target_r );
    visualEvents_.push_back( std::move( event ) );
}

void SfmlBattleView::queueMoraleAnimation( std::uint64_t unit_id ) {
    VisualEvent event;
    event.type_ = VisualEvent::Type::MORALE;
    event.morale_.unitId_ = unit_id;
    visualEvents_.push_back( std::move( event ) );
}

void SfmlBattleView::queueHitAnimation( std::uint64_t defender_id ) {
    VisualEvent event;
    event.type_ = VisualEvent::Type::HIT;
    event.hit_.defenderId_ = defender_id;
    visualEvents_.push_back( event );
}

void SfmlBattleView::queueDeathAnimation( std::uint64_t unit_id ) {
    VisualEvent event;
    event.type_ = VisualEvent::Type::DEATH;
    event.death_.unitId_ = unit_id;
    visualEvents_.push_back( event );
}

void SfmlBattleView::queueRenderDataCommit( const std::vector<UnitRenderData>& units ) {
    VisualEvent event;
    event.type_ = VisualEvent::Type::COMMIT_RENDER_DATA;
    event.commit_.units_ = units;
    visualEvents_.push_back( std::move( event ) );
}

void SfmlBattleView::clearVisualEvents( ) {
    visualEvents_.clear( );
    visualPositionOverrides_.clear( );
}

bool SfmlBattleView::hasPendingVisualEvents( ) const {
    return ! visualEvents_.empty( );
}

void SfmlBattleView::setIdleCallback( std::function<void( )> cb ) {
    idleCallback_ = std::move( cb );
}

bool SfmlBattleView::routeActionClick( float x, float y, BattlePresenter& presenter ) {
    constexpr float K_PRESSED_FLASH_SECONDS = 0.15f;

    for ( ActionSlot& slot : actionSlots_ ) {
        if ( ! slot.bounds_.contains( { x, y } ) ) {
            continue;
}
        slot.pressedSecondsLeft_ = K_PRESSED_FLASH_SECONDS;
        switch ( slot.kind_ ) {
        case ActionKind::WAIT:
            presenter.onWaitClicked( );
            break;
        case ActionKind::DEFEND:
            presenter.onDefendClicked( );
            break;

        case ActionKind::SPELLBOOK:
            presenter.onSpellbookClicked( );
            break;
        case ActionKind::AUTO_COMBAT:
        case ActionKind::SURRENDER:
            showMessage( "Action not yet implemented" );
            break;
        }
        return true;
    }
    return false;
}

void SfmlBattleView::setCursorStyle( CursorStyle style, int pixel_x, int pixel_y ) {
    cursorStyle_ = style;
    cursorPosition_ = { static_cast<float>( pixel_x ), static_cast<float>( pixel_y ) };

    const bool want_visible = ( style == CursorStyle::DEFAULT );
    if ( want_visible != osCursorVisible_ ) {
        window_.setMouseCursorVisible( want_visible );
        osCursorVisible_ = want_visible;
    }
}

void SfmlBattleView::showUnitInfoPanel( const UnitRenderData& unit_data ) {
    infoPanelVisible_ = true;
    infoPanelUnit_ = unit_data;
}

void SfmlBattleView::hideUnitInfoPanel( ) {
    infoPanelVisible_ = false;
    infoPanelUnit_.reset( );
}

sf::Vector2f SfmlBattleView::unitRenderCenter( const UnitRenderData& unit ) const {
    return unitRenderCenter( unit, unit.q_, unit.r_ );
}

sf::Vector2f SfmlBattleView::unitRenderCenter( const UnitRenderData& unit, int q, int r ) const {
    const sf::Vector2f head = hexToPixel( q, r );

    const float vertical_offset = 15.0f;

    if ( unit.size_ != 2 ) {
        return { head.x, head.y + vertical_offset };
}

    const int tail_dq = unit.isFacingLeft_ ? 1 : -1;
    const sf::Vector2f tail = hexToPixel( q + tail_dq, r );
    return { ( head.x + tail.x ) * 0.5f, ( head.y + tail.y ) * 0.5f + vertical_offset };
}

const UnitRenderData* SfmlBattleView::findUnitRenderData( std::uint64_t id ) const {
    for ( const UnitRenderData& unit : unitsToDraw_ ) {
        if ( unit.id_ == id ) {
            return &unit;
}
    }
    for ( const UnitRenderData& unit : modelUnitsLatest_ ) {
        if ( unit.id_ == id ) {
            return &unit;
}
    }
    return nullptr;
}

void SfmlBattleView::handleCorpseStateTransition( const UnitRenderData& unit,
                                                     AnimationController& controller ) {
    if ( ! unit.isCorpse_ ) {
        corpseFrozenIds_.erase( unit.id_ );
        return;
    }

    if ( corpseFrozenIds_.count( unit.id_ ) == 0 ) {
        controller.setAnimationState( AnimState::DEATH, false, true );
        corpseFrozenIds_.insert( unit.id_ );
    }
}

void SfmlBattleView::applyCurrentRenderDataToControllers( bool reset_standing_anim ) {
    std::unordered_set<std::uint64_t> present_ids;

    for ( const UnitRenderData& unit : unitsToDraw_ ) {
        present_ids.insert( unit.id_ );
        if ( unit.assetFilename_.empty( ) ) {
            continue;
        }

        std::shared_ptr<DefResource> resource = defManager_.getOrLoad( unit.assetFilename_ );
        if ( ! resource ) {
            showMessage( "DEF not found or failed to parse: " + unit.assetFilename_ );
            continue;
        }

        sf::Vector2f center = unitRenderCenter( unit );
        if ( const auto it = visualPositionOverrides_.find( unit.id_ );
             it != visualPositionOverrides_.end( ) ) {
            center = it->second;
        }
        const float scale = computeScale( *resource, unit.size_, hexRadius_, unit.assetFilename_ );

        auto ctrl_it = animationControllers_.find( unit.id_ );
        if ( ctrl_it == animationControllers_.end( ) ) {
            AnimationController controller( resource, static_cast<int>( AnimState::STAND ) );
            controller.setHexCenter( center );
            controller.setFacingLeft( unit.visualFacingLeft_ );
            controller.setScale( scale );
            if ( unit.isCorpse_ ) {
                controller.setAnimationState( AnimState::DEATH, false, true );
                corpseFrozenIds_.insert( unit.id_ );
            } else {
                controller.setAnimationState( AnimState::STAND, true, true );
            }
            animationControllers_.emplace( unit.id_, std::move( controller ) );
            controllerAssetFiles_[unit.id_] = unit.assetFilename_;
            continue;
        }

        AnimationController& controller = ctrl_it->second;
        const auto asset_it = controllerAssetFiles_.find( unit.id_ );
        if ( asset_it == controllerAssetFiles_.end( ) ||
             asset_it->second != unit.assetFilename_ ) {
            controller.setResource( resource );
            controllerAssetFiles_[unit.id_] = unit.assetFilename_;
        }
        controller.setHexCenter( center );
        controller.setFacingLeft( unit.visualFacingLeft_ );
        controller.setScale( scale );

        if ( unit.isCorpse_ ) {
            handleCorpseStateTransition( unit, controller );
        } else if ( reset_standing_anim || controller.getAnimationState( ) == AnimState::DEATH ) {
            controller.setAnimationState( AnimState::STAND, true, true );
        }
    }

    for ( auto it = animationControllers_.begin( ); it != animationControllers_.end( ); ) {
        if ( present_ids.count( it->first ) == 0 ) {
            corpseFrozenIds_.erase( it->first );
            visualPositionOverrides_.erase( it->first );
            controllerAssetFiles_.erase( it->first );
            it = animationControllers_.erase( it );
        } else {
            ++it;
        }
    }
}

void SfmlBattleView::refreshExpandedHighlights( ) {
    expandedHighlights_ = highlights_;

    for ( const UnitRenderData& unit : unitsToDraw_ ) {
        if ( unit.size_ != 2 ) {
            continue;
}
        const std::int64_t head_key = makeHexKey( unit.q_, unit.r_ );
        const auto hit = highlights_.find( head_key );
        if ( hit == highlights_.end( ) ) {
            continue;
}
        const int tail_dq = unit.isFacingLeft_ ? 1 : -1;
        expandedHighlights_[makeHexKey( unit.q_ + tail_dq, unit.r_ )] = hit->second;
    }

    for ( const AttackOriginHex& origin : attackOriginHighlights_ ) {
        expandedHighlights_[makeHexKey( origin.q_, origin.r_ )] = HighlightType::ATTACK_ORIGIN;
        if ( origin.hasTail_ ) {
            expandedHighlights_[makeHexKey( origin.tailQ_, origin.tailR_ )] =
                HighlightType::ATTACK_ORIGIN;
        }
    }

    if ( hoverDestinationHighlight_.has_value( ) ) {
        const HoverDestinationHighlight& hover = *hoverDestinationHighlight_;
        expandedHighlights_[makeHexKey( hover.q_, hover.r_ )] = HighlightType::HOVER_DESTINATION;
        if ( hover.hasTail_ ) {
            expandedHighlights_[makeHexKey( hover.tailQ_, hover.tailR_ )] =
                HighlightType::HOVER_DESTINATION;
        }
    }

    if ( activeUnitHighlight_.has_value( ) ) {
        const ActiveUnitHighlight& active = *activeUnitHighlight_;
        expandedHighlights_[makeHexKey( active.q_, active.r_ )] = HighlightType::ACTIVE_UNIT;
        if ( active.size_ == 2 ) {
            const int tail_dq = active.isFacingLeft_ ? 1 : -1;
            expandedHighlights_[makeHexKey( active.q_ + tail_dq, active.r_ )] =
                HighlightType::ACTIVE_UNIT;
        }
    }
}

void SfmlBattleView::updateHoverFromMouse( ) {
    if ( ! attackOriginHighlights_.empty( ) ) {
        return;
    }

    if ( shiftPreviewActive_ ) {
        if ( hoverDestinationHighlight_.has_value( ) ) {
            hoverDestinationHighlight_.reset( );
            refreshExpandedHighlights( );
        }
        return;
    }

    if ( hasPendingVisualEvents( ) || ! activeUnitHighlight_.has_value( ) ) {
        if ( hoverDestinationHighlight_.has_value( ) ) {
            hoverDestinationHighlight_.reset( );
            refreshExpandedHighlights( );
        }
        return;
    }

    const auto [hover_q, hover_r] = pixelToHex( cursorPosition_.x, cursorPosition_.y );
    const std::int64_t hover_key = makeHexKey( hover_q, hover_r );

    const auto hit = highlights_.find( hover_key );
    if ( hit == highlights_.end( ) || hit->second != HighlightType::WALKABLE ) {
        if ( hoverDestinationHighlight_.has_value( ) ) {
            hoverDestinationHighlight_.reset( );
            refreshExpandedHighlights( );
        }
        return;
    }

    bool future_facing_left = activeUnitHighlight_->isFacingLeft_;
    if ( const auto pf = predictedFacingByHex_.find( hover_key );
         pf != predictedFacingByHex_.end( ) ) {
        future_facing_left = pf->second;
    } else {
        const sf::Vector2f active_px =
            hexToPixel( activeUnitHighlight_->q_, activeUnitHighlight_->r_ );
        const sf::Vector2f dest_px = hexToPixel( hover_q, hover_r );
        if ( dest_px.x < active_px.x - 1.0f ) {
            future_facing_left = true;
        } else if ( dest_px.x > active_px.x + 1.0f ) {
            future_facing_left = false;
}
    }

    HoverDestinationHighlight new_hover;
    new_hover.q_ = hover_q;
    new_hover.r_ = hover_r;
    if ( activeUnitHighlight_->size_ == 2 ) {
        new_hover.hasTail_ = true;
        new_hover.tailQ_ = hover_q + ( future_facing_left ? 1 : -1 );
        new_hover.tailR_ = hover_r;
    }

    if ( hoverDestinationHighlight_.has_value( ) &&
         hoverDestinationHighlight_->q_ == new_hover.q_ &&
         hoverDestinationHighlight_->r_ == new_hover.r_ &&
         hoverDestinationHighlight_->hasTail_ == new_hover.hasTail_ &&
         hoverDestinationHighlight_->tailQ_ == new_hover.tailQ_ &&
         hoverDestinationHighlight_->tailR_ == new_hover.tailR_ ) {
        return;
    }

    hoverDestinationHighlight_ = new_hover;
    refreshExpandedHighlights( );
}

void SfmlBattleView::updateVisualEvents( sf::Time dt ) {
    if ( visualEvents_.empty( ) ) {
        return;
    }

    VisualEvent& event = visualEvents_.front( );
    if ( ! event.started_ ) {
        processVisualEventStart( );
        event.started_ = true;
    }

    bool finished = false;
    switch ( event.type_ ) {
    case VisualEvent::Type::MOVE: {
        event.move_.elapsedSeconds_ += dt.asSeconds( );
        if ( ! event.move_.isTeleporter_ ) {
            const float t =
                std::clamp( event.move_.elapsedSeconds_ / event.move_.durationSeconds_, 0.0f, 1.0f );
            const sf::Vector2f pos = {
                event.move_.from_.x + ( event.move_.to_.x - event.move_.from_.x ) * t,
                event.move_.from_.y + ( event.move_.to_.y - event.move_.from_.y ) * t,
            };
            visualPositionOverrides_[event.move_.unitId_] = pos;

            if ( auto it = animationControllers_.find( event.move_.unitId_ );
                 it != animationControllers_.end( ) ) {
                it->second.setHexCenter( pos );
            }

            finished = t >= 1.0f;
            break;
        }

        const float fade_out_seconds = std::max( 0.08f, event.move_.durationSeconds_ * 0.25f );
        const float hold_seconds = std::max( 0.05f, event.move_.durationSeconds_ * 0.15f );
        const float fade_in_seconds = std::max( 0.08f, event.move_.durationSeconds_ * 0.25f );

        auto ctrl_it = animationControllers_.find( event.move_.unitId_ );
        AnimationController* ctrl =
            ( ctrl_it != animationControllers_.end( ) ) ? &ctrl_it->second : nullptr;

        switch ( event.move_.phase_ ) {
        case MoveVisualEvent::Phase::TELEPORT_FADE_OUT: {
            const float t = std::clamp( event.move_.elapsedSeconds_ / fade_out_seconds, 0.0f, 1.0f );
            if ( ctrl ) {
                ctrl->setOpacity( 1.0f - t );
                ctrl->setHexCenter( event.move_.from_ );
            }
            visualPositionOverrides_[event.move_.unitId_] = event.move_.from_;
            if ( t >= 1.0f ) {
                event.move_.phase_ = MoveVisualEvent::Phase::TELEPORT_HOLD;
                event.move_.elapsedSeconds_ = 0.0f;
            }
            break;
        }
        case MoveVisualEvent::Phase::TELEPORT_HOLD: {
            if ( ctrl ) {
                ctrl->setOpacity( 0.0f );
}
            if ( event.move_.elapsedSeconds_ >= hold_seconds ) {
                event.move_.phase_ = MoveVisualEvent::Phase::TELEPORT_FADE_IN;
                event.move_.elapsedSeconds_ = 0.0f;
                visualPositionOverrides_[event.move_.unitId_] = event.move_.to_;
                if ( ctrl ) {
                    ctrl->setHexCenter( event.move_.to_ );
}
            }
            break;
        }
        case MoveVisualEvent::Phase::TELEPORT_FADE_IN: {
            const float t = std::clamp( event.move_.elapsedSeconds_ / fade_in_seconds, 0.0f, 1.0f );
            if ( ctrl ) {
                ctrl->setOpacity( t );
                ctrl->setHexCenter( event.move_.to_ );
            }
            visualPositionOverrides_[event.move_.unitId_] = event.move_.to_;
            finished = t >= 1.0f;
            break;
        }
        case MoveVisualEvent::Phase::SLIDE:
            break;
        }
        break;
    }

    case VisualEvent::Type::ATTACK: {
        event.attack_.elapsedSeconds_ += dt.asSeconds( );
        const auto it = animationControllers_.find( event.attack_.attackerId_ );
        if ( it != animationControllers_.end( ) ) {
            finished = it->second.isFinished( ) ||
                       event.attack_.elapsedSeconds_ >= event.attack_.safetyTimeout_;
        } else {
            finished = true;
        }
        break;
    }

    case VisualEvent::Type::PROJECTILE: {
        event.projectile_.elapsedSeconds_ += dt.asSeconds( );
        finished = event.projectile_.elapsedSeconds_ >= event.projectile_.durationSeconds_;
        break;
    }

    case VisualEvent::Type::MORALE: {
        event.morale_.elapsedSeconds_ += dt.asSeconds( );
        finished = event.morale_.elapsedSeconds_ >= event.morale_.durationSeconds_;
        break;
    }

    case VisualEvent::Type::HIT: {
        event.hit_.elapsedSeconds_ += dt.asSeconds( );
        const auto it = animationControllers_.find( event.hit_.defenderId_ );
        if ( it != animationControllers_.end( ) ) {
            finished =
                it->second.isFinished( ) || event.hit_.elapsedSeconds_ >= event.hit_.safetyTimeout_;
        } else {
            finished = true;
        }
        break;
    }

    case VisualEvent::Type::DEATH: {
        event.death_.elapsedSeconds_ += dt.asSeconds( );
        const auto it = animationControllers_.find( event.death_.unitId_ );
        if ( it != animationControllers_.end( ) ) {
            finished = it->second.isFinished( ) ||
                       event.death_.elapsedSeconds_ >= event.death_.safetyTimeout_;
        } else {
            finished = true;
        }
        break;
    }

    case VisualEvent::Type::COMMIT_RENDER_DATA: {
        finished = true;
        break;
    }

    case VisualEvent::Type::SPELL_CAST: {
        // Time-advance + drawing happen in drawSpellCastOverlay() so
        // the animation renders ABOVE the unit sprites. Skip here.
        break;
    }
    }

    if ( finished ) {
        processVisualEventFinish( );
        visualEvents_.pop_front( );

        while ( ! visualEvents_.empty( ) &&
                visualEvents_.front( ).type_ == VisualEvent::Type::COMMIT_RENDER_DATA ) {
            processVisualEventStart( );
            processVisualEventFinish( );
            visualEvents_.pop_front( );
        }

        if ( visualEvents_.empty( ) && idleCallback_ ) {
            std::function<void( )> cb = std::move( idleCallback_ );
            idleCallback_ = nullptr;
            cb( );
        }
    }
}

void SfmlBattleView::processVisualEventStart( ) {
    if ( visualEvents_.empty( ) ) {
        return;
}

    VisualEvent& event = visualEvents_.front( );
    switch ( event.type_ ) {
    case VisualEvent::Type::MOVE: {
        event.move_.elapsedSeconds_ = 0.0f;
        if ( auto it = animationControllers_.find( event.move_.unitId_ );
             it != animationControllers_.end( ) ) {
            if ( event.move_.isTeleporter_ ) {
                event.move_.phase_ = MoveVisualEvent::Phase::TELEPORT_FADE_OUT;
                it->second.setAnimationState( AnimState::STAND, true, true );
                it->second.setOpacity( 1.0f );
            } else {
                event.move_.phase_ = MoveVisualEvent::Phase::SLIDE;
                it->second.setAnimationState( AnimState::MOVE, true, true );

                constexpr float K_FLIP_DEAD_ZONE = 1.0f;
                if ( event.move_.to_.x < event.move_.from_.x - K_FLIP_DEAD_ZONE ) {
                    it->second.setFacingLeft( true );
                } else if ( event.move_.to_.x > event.move_.from_.x + K_FLIP_DEAD_ZONE ) {
                    it->second.setFacingLeft( false );
                }
            }
        }
        visualPositionOverrides_[event.move_.unitId_] = event.move_.from_;
        break;
    }
    case VisualEvent::Type::ATTACK: {
        if ( auto it = animationControllers_.find( event.attack_.attackerId_ );
             it != animationControllers_.end( ) ) {
            if ( event.attack_.hasTargetHex_ ) {
                const sf::Vector2f tgt_px =
                    hexToPixel( event.attack_.targetQ_, event.attack_.targetR_ );
                const sf::Vector2f own_px =
                    it->second.getSprite( ) ? it->second.getSprite( )->getPosition( ) : tgt_px;
                constexpr float K_FLIP_DEAD_ZONE = 1.0f;
                if ( tgt_px.x < own_px.x - K_FLIP_DEAD_ZONE ) {
                    it->second.setFacingLeft( true );
                } else if ( tgt_px.x > own_px.x + K_FLIP_DEAD_ZONE ) {
                    it->second.setFacingLeft( false );
}
            }
            it->second.setAnimationState( AnimState::ATTACK, false, true );
        }
        break;
    }
    case VisualEvent::Type::PROJECTILE: {
        if ( auto it = animationControllers_.find( event.projectile_.attackerId_ );
             it != animationControllers_.end( ) ) {
            if ( const sf::Sprite* s = it->second.getSprite( ) ) {
                event.projectile_.from_ = s->getPosition( );
            }
        }
        break;
    }
    case VisualEvent::Type::MORALE: {
        break;
    }
    case VisualEvent::Type::HIT: {
        if ( auto it = animationControllers_.find( event.hit_.defenderId_ );
             it != animationControllers_.end( ) ) {
            it->second.setAnimationState( AnimState::TAKE_DAMAGE, false, true );
        }
        break;
    }
    case VisualEvent::Type::DEATH: {
        break;
    }
    case VisualEvent::Type::COMMIT_RENDER_DATA: {
        break;
    }
    case VisualEvent::Type::SPELL_CAST: {
        break;
    }
    }
}

void SfmlBattleView::processVisualEventFinish( ) {
    if ( visualEvents_.empty( ) ) {
        return;
}

    VisualEvent& event = visualEvents_.front( );
    switch ( event.type_ ) {
    case VisualEvent::Type::MOVE: {
        visualPositionOverrides_[event.move_.unitId_] = event.move_.to_;
        if ( auto it = animationControllers_.find( event.move_.unitId_ );
             it != animationControllers_.end( ) ) {
            it->second.setOpacity( 1.0f );
            it->second.setHexCenter( event.move_.to_ );
            it->second.setAnimationState( AnimState::STAND, true, true );
        }
        break;
    }
    case VisualEvent::Type::ATTACK: {
        if ( auto it = animationControllers_.find( event.attack_.attackerId_ );
             it != animationControllers_.end( ) ) {
            it->second.setAnimationState( AnimState::STAND, true, true );
        }
        break;
    }
    case VisualEvent::Type::PROJECTILE: {
        break;
    }
    case VisualEvent::Type::MORALE: {
        break;
    }
    case VisualEvent::Type::HIT: {
        if ( auto it = animationControllers_.find( event.hit_.defenderId_ );
             it != animationControllers_.end( ) ) {
            it->second.setAnimationState( AnimState::STAND, true, true );
        }
        break;
    }
    case VisualEvent::Type::DEATH: {
        break;
    }
    case VisualEvent::Type::COMMIT_RENDER_DATA: {
        unitsToDraw_ = event.commit_.units_;
        modelUnitsLatest_ = event.commit_.units_;
        visualPositionOverrides_.clear( );
        applyCurrentRenderDataToControllers( false );
        refreshExpandedHighlights( );
        break;
    }
    case VisualEvent::Type::SPELL_CAST: {
        break;
    }
    }
}

sf::Vector2f SfmlBattleView::hexToPixel( int q, int r ) const {
    const float x = hexRadius_ * ( std::sqrt( 3.0f ) *
                                   ( static_cast<float>( q ) + static_cast<float>( r ) * 0.5f ) );
    const float y = hexRadius_ * ( 1.5f * static_cast<float>( r ) );
    return { gridOrigin_.x + x, gridOrigin_.y + y };
}

std::pair<int, int> SfmlBattleView::pixelToHex( float x, float y ) const {
    const float px = x - gridOrigin_.x;
    const float py = y - gridOrigin_.y;

    const float fq = ( std::sqrt( 3.0f ) / 3.0f * px - 1.0f / 3.0f * py ) / hexRadius_;
    const float fr = ( 2.0f / 3.0f * py ) / hexRadius_;
    const float fs = -fq - fr;

    return cubeRoundToAxial( fq, fr, fs );
}

sf::ConvexShape SfmlBattleView::makeHexShape( int q, int r ) const {
    sf::ConvexShape shape( 6 );

    const sf::Vector2f center = hexToPixel( q, r );
    for ( int i = 0; i < 6; ++i ) {
        const float angle = ( 60.0f * static_cast<float>( i ) - 30.0f ) * ( K_PI / 180.0f );
        const float vx = center.x + hexRadius_ * std::cos( angle );
        const float vy = center.y + hexRadius_ * std::sin( angle );
        shape.setPoint( i, { vx, vy } );
    }

    return shape;
}

std::int64_t SfmlBattleView::makeHexKey( int q, int r ) {
    return ( static_cast<std::int64_t>( q ) << 32 ) ^
           ( static_cast<std::int64_t>( r ) & 0xffffffffLL );
}

bool SfmlBattleView::isPointInBattlefield( float x, float y ) const {
    return x >= 0.0f && x <= screenWidth_ && y >= 0.0f && y < battlefieldHeight_;
}

// =========================================================================
// Spellbook overlay + spell targeting + SPELL_CAST visual event
// =========================================================================

namespace {

constexpr float K_SPELLBOOK_PANEL_W = 960.0f;
constexpr float K_SPELLBOOK_PANEL_H = 720.0f;
constexpr int K_SPELLBOOK_COLS = 4;
constexpr float K_SPELLBOOK_CELL_W = 200.0f;
constexpr float K_SPELLBOOK_CELL_H = 130.0f;
constexpr float K_SPELLBOOK_CELL_GAP = 12.0f;
constexpr float K_SPELLBOOK_HEADER_PAD = 60.0f;
constexpr float K_SPELLBOOK_ICON_SIZE = 58.0f;
constexpr float K_SPELL_TARGET_FLASH_SECONDS = 0.4f;

} // namespace

void SfmlBattleView::loadSpellbookAssets( ) {
    if ( spellbookBgTexture_.loadFromFile( "assets/ui/spellbook/spellbook_bg.png" ) ) {
        spellbookBgSprite_ = std::make_unique<sf::Sprite>( spellbookBgTexture_ );
        const sf::Vector2u ts = spellbookBgTexture_.getSize( );
        if ( ts.x > 0 && ts.y > 0 ) {
            spellbookBgSprite_->setScale(
                { K_SPELLBOOK_PANEL_W / static_cast<float>( ts.x ),
                  K_SPELLBOOK_PANEL_H / static_cast<float>( ts.y ) } );
        }
        spellbookBgLoaded_ = true;
    }
    if ( spellIconAtlasTexture_.loadFromFile( "assets/ui/spellbook/spell_icons.png" ) ) {
        spellIconAtlasLoaded_ = true;
    }
}

void SfmlBattleView::showSpellbook( const std::vector<SpellbookSpellRender>& spells,
                                              int caster_mana,
                                              int caster_max_mana ) {
    spellbookOpen_ = true;
    spellbookCasterMana_ = caster_mana;
    spellbookCasterMaxMana_ = caster_max_mana;
    spellbookSpells_ = spells;
    spellbookHoveredCell_ = -1;

    const float panel_x = ( screenWidth_ - K_SPELLBOOK_PANEL_W ) * 0.5f;
    const float panel_y = ( screenHeight_ - K_SPELLBOOK_PANEL_H ) * 0.5f;
    if ( spellbookBgSprite_ ) {
        spellbookBgSprite_->setPosition( { panel_x, panel_y } );
    }

    spellCellBounds_.clear( );
    spellCellBounds_.reserve( spellbookSpells_.size( ) );
    const float grid_x = panel_x +
        ( K_SPELLBOOK_PANEL_W -
          K_SPELLBOOK_COLS * ( K_SPELLBOOK_CELL_W + K_SPELLBOOK_CELL_GAP ) +
          K_SPELLBOOK_CELL_GAP ) *
            0.5f;
    const float grid_y = panel_y + K_SPELLBOOK_HEADER_PAD;
    for ( int i = 0; i < static_cast<int>( spellbookSpells_.size( ) ); ++i ) {
        const int col = i % K_SPELLBOOK_COLS;
        const int row = i / K_SPELLBOOK_COLS;
        const float x =
            grid_x + static_cast<float>( col ) * ( K_SPELLBOOK_CELL_W + K_SPELLBOOK_CELL_GAP );
        const float y =
            grid_y + static_cast<float>( row ) * ( K_SPELLBOOK_CELL_H + K_SPELLBOOK_CELL_GAP );
        spellCellBounds_.emplace_back( sf::Vector2f{ x, y },
                                              sf::Vector2f{ K_SPELLBOOK_CELL_W,
                                                                K_SPELLBOOK_CELL_H } );
    }

    spellbookCloseBounds_ = sf::FloatRect(
        { panel_x + K_SPELLBOOK_PANEL_W - 90.0f, panel_y + 20.0f }, { 70.0f, 28.0f } );
}

void SfmlBattleView::hideSpellbook( ) {
    spellbookOpen_ = false;
    spellbookSpells_.clear( );
    spellCellBounds_.clear( );
    spellbookHoveredCell_ = -1;
}

void SfmlBattleView::setSpellTargetingActive( bool active,
                                                        models::SpellAlignment alignment ) {
    spellTargetingActive_ = active;
    spellTargetingAlignment_ = alignment;
    spellCursorIsValid_ = false;
    if ( ! active ) {
        // Restore default cursor when targeting ends.
        setCursorStyle( CursorStyle::DEFAULT, 0, 0 );
    }
}

void SfmlBattleView::setSpellCursorValid( bool valid ) {
    spellCursorIsValid_ = valid;
}

void SfmlBattleView::queueSpellAnimation( int target_q,
                                                  int target_r,
                                                  const std::string& def_asset,
                                                  float duration_seconds ) {
    VisualEvent ev;
    ev.type_ = VisualEvent::Type::SPELL_CAST;
    ev.spell_.targetQ_ = target_q;
    ev.spell_.targetR_ = target_r;
    ev.spell_.defAsset_ = def_asset;
    ev.spell_.durationSeconds_ = duration_seconds > 0.0f ? duration_seconds
                                                          : K_SPELL_TARGET_FLASH_SECONDS;
    visualEvents_.push_back( ev );
}

void SfmlBattleView::drawSpellCastOverlay( sf::Time dt ) {
    if ( visualEvents_.empty( ) ) {
        return;
    }
    VisualEvent& event = visualEvents_.front( );
    if ( event.type_ != VisualEvent::Type::SPELL_CAST ) {
        return;
    }

    SpellCastVisualEvent& spe = event.spell_;
    spe.elapsedSeconds_ += dt.asSeconds( );

    const sf::Vector2f center = hexToPixel( spe.targetQ_, spe.targetR_ );
    const float radius = hexRadius_ * 1.1f;

    bool drew_from_def = false;
    if ( ! spe.defAsset_.empty( ) ) {
        std::shared_ptr<DefResource> res = defManager_.getOrLoad( spe.defAsset_ );
        if ( res ) {
            const auto group_it = res->groups_.find( 0 );
            if ( group_it != res->groups_.end( ) && ! group_it->second.empty( ) ) {
                const auto& frames = group_it->second;
                const float progress = std::clamp(
                    spe.elapsedSeconds_ / spe.durationSeconds_, 0.0f, 0.9999f );
                const std::size_t frame_index = static_cast<std::size_t>(
                    progress * static_cast<float>( frames.size( ) ) );
                const DefFrame& frame = frames[frame_index];
                if ( frame.width_ > 0 && frame.height_ > 0 ) {
                    sf::Sprite spr( frame.texture_ );
                    const float scale = std::min(
                        ( radius * 2.0f ) / static_cast<float>( frame.width_ ),
                        ( radius * 2.0f ) / static_cast<float>( frame.height_ ) );
                    spr.setScale( { scale, scale } );
                    spr.setOrigin(
                        { static_cast<float>( frame.width_ ) * 0.5f,
                          static_cast<float>( frame.height_ ) * 0.5f } );
                    spr.setPosition( center );
                    window_.draw( spr );
                    drew_from_def = true;
                }
            }
        }
    }

    if ( ! drew_from_def ) {
        const float t =
            std::clamp( spe.elapsedSeconds_ / spe.durationSeconds_, 0.0f, 1.0f );
        const float alpha_norm = ( t < 0.5f ) ? t * 2.0f : ( 1.0f - t ) * 2.0f;
        const std::uint8_t alpha =
            static_cast<std::uint8_t>( std::round( alpha_norm * 220.0f ) );
        sf::CircleShape flash( radius );
        flash.setOrigin( { radius, radius } );
        flash.setPosition( center );
        flash.setFillColor( sf::Color( 255, 230, 120, alpha ) );
        flash.setOutlineColor( sf::Color( 255, 200, 80, alpha ) );
        flash.setOutlineThickness( 2.0f );
        window_.draw( flash );
    }

    if ( spe.elapsedSeconds_ >= spe.durationSeconds_ ) {
        processVisualEventFinish( );
        visualEvents_.pop_front( );

        // Drain any COMMIT_RENDER_DATA events that were queued right
        // after this spell so the post-cast state shows up this frame.
        while ( ! visualEvents_.empty( ) &&
                visualEvents_.front( ).type_ == VisualEvent::Type::COMMIT_RENDER_DATA ) {
            processVisualEventStart( );
            processVisualEventFinish( );
            visualEvents_.pop_front( );
        }

        if ( visualEvents_.empty( ) && idleCallback_ ) {
            std::function<void( )> cb = std::move( idleCallback_ );
            idleCallback_ = nullptr;
            cb( );
        }
    }
}

bool SfmlBattleView::routeSpellbookClick( float x, float y, presenters::BattlePresenter& presenter ) {
    if ( spellbookCloseBounds_.contains( { x, y } ) ) {
        presenter.onSpellbookCancelled( );
        return true;
    }
    for ( int i = 0; i < static_cast<int>( spellCellBounds_.size( ) ); ++i ) {
        if ( spellCellBounds_[i].contains( { x, y } ) ) {
            const SpellbookSpellRender& cell = spellbookSpells_[i];
            if ( cell.affordable_ ) {
                presenter.onSpellChosen( cell.id_ );
            } else {
                showMessage( "Cannot cast that spell right now." );
            }
            return true;
        }
    }
    presenter.onSpellbookCancelled( );
    return true;
}

void SfmlBattleView::drawSpellbookOverlay( ) {
    sf::RectangleShape backdrop( { screenWidth_, screenHeight_ } );
    backdrop.setPosition( { 0.0f, 0.0f } );
    backdrop.setFillColor( sf::Color( 0, 0, 0, 170 ) );
    window_.draw( backdrop );

    const float panel_x = ( screenWidth_ - K_SPELLBOOK_PANEL_W ) * 0.5f;
    const float panel_y = ( screenHeight_ - K_SPELLBOOK_PANEL_H ) * 0.5f;

    if ( spellbookBgLoaded_ && spellbookBgSprite_ ) {
        spellbookBgSprite_->setPosition( { panel_x, panel_y } );
        window_.draw( *spellbookBgSprite_ );
    } else {
        sf::RectangleShape panel( { K_SPELLBOOK_PANEL_W, K_SPELLBOOK_PANEL_H } );
        panel.setPosition( { panel_x, panel_y } );
        panel.setFillColor( sf::Color( 22, 22, 36, 240 ) );
        panel.setOutlineColor( sf::Color( 140, 170, 220 ) );
        panel.setOutlineThickness( 2.0f );
        window_.draw( panel );
    }

    if ( spellbookText_ ) {
        spellbookText_->setCharacterSize( 22 );
        spellbookText_->setFillColor( sf::Color( 240, 235, 200 ) );
        spellbookText_->setStyle( sf::Text::Bold );
        spellbookText_->setString( "Spellbook  --  Mana " +
                                       std::to_string( spellbookCasterMana_ ) + " / " +
                                       std::to_string( spellbookCasterMaxMana_ ) );
        spellbookText_->setPosition( { panel_x + 20.0f, panel_y + 18.0f } );
        window_.draw( *spellbookText_ );

        // Close button
        sf::RectangleShape close_bg( spellbookCloseBounds_.size );
        close_bg.setPosition( spellbookCloseBounds_.position );
        close_bg.setFillColor( sf::Color( 70, 35, 35, 220 ) );
        close_bg.setOutlineColor( sf::Color( 200, 120, 120 ) );
        close_bg.setOutlineThickness( 1.5f );
        window_.draw( close_bg );
        spellbookText_->setCharacterSize( 16 );
        spellbookText_->setStyle( sf::Text::Regular );
        spellbookText_->setFillColor( sf::Color::White );
        spellbookText_->setString( "Close" );
        const sf::FloatRect tb = spellbookText_->getLocalBounds( );
        spellbookText_->setPosition(
            { spellbookCloseBounds_.position.x +
                  ( spellbookCloseBounds_.size.x - tb.size.x ) * 0.5f - tb.position.x,
              spellbookCloseBounds_.position.y +
                  ( spellbookCloseBounds_.size.y - tb.size.y ) * 0.5f - tb.position.y } );
        window_.draw( *spellbookText_ );
    }

    for ( int i = 0; i < static_cast<int>( spellCellBounds_.size( ) ); ++i ) {
        const sf::FloatRect& bounds = spellCellBounds_[i];
        const SpellbookSpellRender& cell = spellbookSpells_[i];
        const bool hovered = ( spellbookHoveredCell_ == i );

        sf::RectangleShape frame( bounds.size );
        frame.setPosition( bounds.position );
        if ( ! cell.affordable_ ) {
            frame.setFillColor( sf::Color( 18, 18, 26, 220 ) );
            frame.setOutlineColor( sf::Color( 70, 70, 90 ) );
        } else if ( hovered ) {
            frame.setFillColor( sf::Color( 50, 60, 100, 230 ) );
            frame.setOutlineColor( sf::Color( 200, 220, 255 ) );
        } else {
            frame.setFillColor( sf::Color( 28, 32, 50, 220 ) );
            frame.setOutlineColor( sf::Color( 130, 150, 190 ) );
        }
        frame.setOutlineThickness( 1.5f );
        window_.draw( frame );

        // Icon (prefer DEF frames; fall back to PNG atlas)
        bool drew_def_icon = false;
        if ( cell.defIconFrame_ >= 0 ) {
            std::shared_ptr<DefResource> res = defManager_.getOrLoad( "spells_icons.def" );
            if ( res ) {
                const auto group_it = res->groups_.find( 0 );
                if ( group_it != res->groups_.end( ) && ! group_it->second.empty( ) ) {
                    const auto& frames = group_it->second;
                    const std::size_t frame_index =
                        static_cast<std::size_t>( cell.defIconFrame_ );
                    if ( frame_index < frames.size( ) ) {
                        const DefFrame& frame = frames[frame_index];
                        if ( frame.width_ > 0 && frame.height_ > 0 ) {
                            sf::Sprite icon( frame.texture_ );
                            const float scale =
                                std::min( K_SPELLBOOK_ICON_SIZE / static_cast<float>( frame.width_ ),
                                          K_SPELLBOOK_ICON_SIZE / static_cast<float>( frame.height_ ) );
                            icon.setScale( { scale, scale } );
                            icon.setPosition( { bounds.position.x + 12.0f,
                                                   bounds.position.y + 12.0f } );
                            if ( ! cell.affordable_ ) {
                                icon.setColor( sf::Color( 120, 120, 120 ) );
                            }
                            window_.draw( icon );
                            drew_def_icon = true;
                        }
                    }
                }
            }
        }
        if ( ! drew_def_icon && spellIconAtlasLoaded_ && cell.iconRect_.w_ > 0 &&
             cell.iconRect_.h_ > 0 ) {
            sf::Sprite icon( spellIconAtlasTexture_ );
            icon.setTextureRect( sf::IntRect(
                { cell.iconRect_.x_, cell.iconRect_.y_ },
                { cell.iconRect_.w_, cell.iconRect_.h_ } ) );
            const float scale =
                std::min( K_SPELLBOOK_ICON_SIZE / static_cast<float>( cell.iconRect_.w_ ),
                          K_SPELLBOOK_ICON_SIZE / static_cast<float>( cell.iconRect_.h_ ) );
            icon.setScale( { scale, scale } );
            icon.setPosition( { bounds.position.x + 12.0f, bounds.position.y + 12.0f } );
            if ( ! cell.affordable_ ) {
                icon.setColor( sf::Color( 120, 120, 120 ) );
            }
            window_.draw( icon );
        }

        // Text
        if ( spellbookText_ ) {
            spellbookText_->setCharacterSize( 16 );
            spellbookText_->setStyle( sf::Text::Bold );
            spellbookText_->setFillColor( cell.affordable_ ? sf::Color::White
                                                            : sf::Color( 130, 130, 130 ) );
            spellbookText_->setString( cell.name_ );
            spellbookText_->setPosition(
                { bounds.position.x + 12.0f + K_SPELLBOOK_ICON_SIZE + 10.0f,
                  bounds.position.y + 14.0f } );
            window_.draw( *spellbookText_ );

            spellbookText_->setCharacterSize( 14 );
            spellbookText_->setStyle( sf::Text::Regular );
            spellbookText_->setFillColor( cell.affordable_ ? sf::Color( 200, 220, 255 )
                                                            : sf::Color( 100, 100, 100 ) );
            spellbookText_->setString( "Lv " + std::to_string( cell.level_ ) +
                                           "  --  " +
                                           std::to_string( cell.manaCost_ ) + " mana" );
            spellbookText_->setPosition(
                { bounds.position.x + 12.0f + K_SPELLBOOK_ICON_SIZE + 10.0f,
                  bounds.position.y + 38.0f } );
            window_.draw( *spellbookText_ );
        }
    }
}

} // namespace views
