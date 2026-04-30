/**
 * @file BattlePresenter.cc
 * @brief Implementation of the MVP presenter for the battle screen.
 * @author Dominik Śledziewski & Łukasz Szydlik
 */
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <unordered_set>
#include <vector>

#include "BattlePresenter.h"

namespace presenters {

using core::ActionManager;
using core::GameManager;
using models::Hero;
using models::Hex;
using models::Unit;
using views::CursorStyle;
using views::HighlightType;
using views::IBattleView;
using views::UnitRenderData;

namespace {
constexpr float K_HEX_RADIUS = 28.0f;
constexpr float K_GRID_ORIGIN_X = 300.0f;

constexpr float K_GRID_ORIGIN_Y = 70.0f + 28.0f * 1.5f;
constexpr float K_PI = 3.14159265358979323846f;

std::int64_t makeHexKey( int q, int r ) {
    return ( static_cast<std::int64_t>( q ) << 32 ) ^ ( static_cast<std::uint32_t>( r ) );
}

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

bool heroContainsUnit( const Hero& hero, const Unit& unit ) {
    for ( const auto& candidate : hero.getArmy( ).getUnits( ) ) {
        if ( candidate && candidate.get( ) == &unit ) {
            return true;
        }
    }
    return false;
}

int ownerIdForUnit( const GameManager& model, const Unit& unit ) {
    if ( heroContainsUnit( model.getRedHero( ), unit ) ) {
        return 0;
    }
    if ( heroContainsUnit( model.getBlueHero( ), unit ) ) {
        return 1;
    }
    return -1;
}

UnitRenderData
makeUnitRenderData( const GameManager& model, const Unit& unit, int q, int r, bool is_corpse ) {
    UnitRenderData data;
    data.id_ = static_cast<std::uint64_t>( reinterpret_cast<std::uintptr_t>( &unit ) );
    data.q_ = q;
    data.r_ = r;
    data.name_ = unit.getName( );
    data.assetFilename_ = unit.getAssetFilename( );
    data.description_ = unit.getDescription( );
    data.count_ = unit.getCount( );
    data.hpLeft_ = unit.getHealthLeft( );
    data.maxHpPerUnit_ = unit.getHealth( );
    data.currentTopUnitHp_ = unit.getHealthLeft( );
    data.ownerId_ = ownerIdForUnit( model, unit );
    data.baseAttack_ = unit.getBaseAttack( );
    data.totalAttack_ = unit.getAttack( );
    data.baseDefense_ = unit.getBaseDefense( );
    data.totalDefense_ = unit.getDefense( );
    data.baseSpeed_ = unit.getBaseSpeed( );
    data.totalSpeed_ = unit.getSpeed( );
    data.baseDamageMin_ = unit.getBaseDamageMin( );
    data.totalDamageMin_ = unit.getDamageMin( );
    data.baseDamageMax_ = unit.getBaseDamageMax( );
    data.totalDamageMax_ = unit.getDamageMax( );
    data.isFacingLeft_ = unit.isFacingLeft( );
    data.visualFacingLeft_ = unit.getVisualFacingLeft( );
    data.isRanged_ = unit.isRanged( );
    data.ammo_ = unit.getAmmo( );
    data.maxAmmo_ = unit.getMaxAmmo( );
    data.isCorpse_ = is_corpse;
    data.size_ = unit.getSize( );
    data.isTeleporter_ = unit.isTeleporterUnit( );
    data.isFlying_ = unit.isFlyingUnit( );
    return data;
}

BattlePresenter::BattlePresenter( GameManager& model, IBattleView& view )
    : model_( model ), view_( view ) {}

void BattlePresenter::startBattle( ) {
    pushRenderDataToView( );
    view_.syncUnitPositions( );
    refreshUiForActiveUnit( );
}

void BattlePresenter::onHexClicked( int q, int r, bool ) {
    if ( view_.hasPendingVisualEvents( ) ) {
        return;
    }

    view_.clearHoverDestinationHighlight( );
    view_.clearAttackOriginHighlights( );

    Unit* active_unit = model_.getCurrentUnit( );
    if ( active_unit == nullptr ) {
        return;
}

    const std::vector<UnitRenderData> before_units = buildRenderDataSnapshot( );

    const int s = -q - r;
    Hex* clicked_hex = nullptr;
    try {
        clicked_hex = &model_.getBoard( ).getHex( q, r, s );
    } catch ( const std::out_of_range& ) {
        view_.showMessage( "Invalid hex" );
        return;
    }

    if ( clicked_hex->hasUnit( ) && clicked_hex->getUnit( ).get( ) != active_unit ) {
        Unit* target = clicked_hex->getUnit( ).get( );
        if ( target == nullptr ) {
            return;
}

        if ( ! model_.areEnemies( *active_unit, *target ) ) {
            view_.showMessage( "Cannot attack allied unit" );
            return;
        }

        const bool will_shoot = model_.willShoot( *active_unit, *target );
        const bool had_morale = model_.activeUnitHasMoraleBonus( );

        std::vector<std::pair<int, int>> intended_path_for_attack;

        try {
            if ( will_shoot ) {
                Hex& attacker_hex = model_.getBoard( ).getHex(
                    active_unit->getQ( ), active_unit->getR( ), active_unit->getS( ) );
                model_.attack( *active_unit, *target, attacker_hex );
                view_.showMessage( "Shoot!" );
            } else {
                // Single source of truth: the same picker hover used to
                // place the directional sword cursor and the highlighted
                // approach hex.  Picking a different rule here (e.g.
                // euclidean-closest origin, or short-circuiting on
                // "directly adjacent") was the source of bug #3 -- preview
                // disagreed with the actual attack -- and bug #1, where a
                // current-position adjacent attacker could never reposition
                // before striking.
                const PickedApproach picked =
                    pickAttackApproachForCursor( *active_unit,
                                                     *clicked_hex,
                                                     static_cast<float>( lastCursorPx_ ),
                                                     static_cast<float>( lastCursorPy_ ) );
                Hex* approach = picked.approach_;
                if ( approach == nullptr ) {
                    view_.showMessage( "Cannot reach that enemy" );
                    return;
                }

                // Capture the BFS path BEFORE the model mutation so the View
                // can replay segment-by-segment movement.  When the picked
                // approach equals the attacker's current head (player chose
                // to strike in place) the path is size 1 and no slide event
                // is queued downstream.
                try {
                    const std::vector<const Hex*> chain =
                        model_.findPath( *active_unit, *approach );
                    intended_path_for_attack.reserve( chain.size( ) );
                    for ( const Hex* h : chain ) {
                        if ( h != nullptr ) {
                            intended_path_for_attack.emplace_back( h->getQ( ), h->getR( ) );
}
                    }
                } catch ( const std::exception& ) {}
                model_.attack( *active_unit, *target, *approach );
                view_.showMessage( intended_path_for_attack.size( ) >= 2 ? "Move + Attack!"
                                                                         : "Attack!" );
            }
        } catch ( const std::exception& e ) {
            view_.showMessage( std::string( "Attack failed: " ) + e.what( ) );
            return;
        }

        const std::vector<UnitRenderData> after_units = buildRenderDataSnapshot( );
        const std::uint64_t attacker_id = makeUnitId( active_unit );
        const std::uint64_t defender_id = makeUnitId( target );

        const auto before_att = findUnit( before_units, attacker_id );
        const auto after_att = findUnit( after_units, attacker_id );
        const auto before_def = findUnit( before_units, defender_id );
        const auto after_def = findUnit( after_units, defender_id );

        const bool defender_died = after_def.has_value( ) && after_def->isCorpse_ &&
                                   before_def.has_value( ) && ! before_def->isCorpse_;

        const bool attacker_took_damage = after_att.has_value( ) && before_att.has_value( ) &&
                                          ( after_att->hpLeft_ < before_att->hpLeft_ ||
                                            ( after_att->isCorpse_ && ! before_att->isCorpse_ ) );
        const bool retaliation_occurred = attacker_took_damage && ! defender_died;

        const bool attacker_died = after_att.has_value( ) && after_att->isCorpse_ &&
                                   before_att.has_value( ) && ! before_att->isCorpse_;

        view_.clearVisualEvents( );
        view_.updateRenderData( before_units );
        view_.syncUnitPositions( );

        if ( will_shoot ) {
            view_.queueAttackAnimationFacing( attacker_id, after_def->q_, after_def->r_ );
            view_.queueProjectileAnimation( attacker_id,
                                             after_def->q_,
                                             after_def->r_,
                                             active_unit->getProjectileAsset( ),
                                             0.4f );
            view_.queueHitAnimation( defender_id );

            if ( defender_died ) {
                view_.queueRenderDataCommit( after_units );
                view_.queueDeathAnimation( defender_id );
            } else {
                view_.queueRenderDataCommit( after_units );
            }
            model_.nextTurn( );
            finalizeActionVisuals( attacker_id, had_morale );
            return;
        }

        if ( intended_path_for_attack.size( ) >= 2 ) {
            queueMoveVisualAlongPath( attacker_id, intended_path_for_attack );
        } else {
            queueMoveVisualIfNeeded( attacker_id, before_units, after_units );
        }

        view_.queueAttackAnimationFacing( attacker_id, after_def->q_, after_def->r_ );

        view_.queueHitAnimation( defender_id );

        if ( defender_died ) {
            view_.queueRenderDataCommit( after_units );
            view_.queueDeathAnimation( defender_id );
        } else if ( retaliation_occurred ) {
            view_.queueAttackAnimationFacing( defender_id, after_att->q_, after_att->r_ );
            view_.queueHitAnimation( attacker_id );

            view_.queueRenderDataCommit( after_units );

            if ( attacker_died ) {
                view_.queueDeathAnimation( attacker_id );
            }
        } else {
            view_.queueRenderDataCommit( after_units );
        }

        model_.nextTurn( );
        finalizeActionVisuals( attacker_id, had_morale );
        return;
    }

    const Hex* move_head_hex = resolveMoveHeadDestination( *active_unit, *clicked_hex );
    if ( move_head_hex != nullptr ) {
        const bool had_morale = model_.activeUnitHasMoraleBonus( );

        std::vector<std::pair<int, int>> intended_path;
        try {
            const std::vector<const Hex*> chain = model_.findPath( *active_unit, *move_head_hex );
            intended_path.reserve( chain.size( ) );
            for ( const Hex* h : chain ) {
                if ( h != nullptr ) {
                    intended_path.emplace_back( h->getQ( ), h->getR( ) );
}
            }
        } catch ( const std::exception& ) {}

        try {
            Hex& move_head =
                model_.getBoard( ).getHex( move_head_hex->getQ( ),
                                            move_head_hex->getR( ),
                                            -move_head_hex->getQ( ) - move_head_hex->getR( ) );
            model_.move( *active_unit, move_head );
        } catch ( const std::exception& e ) {
            view_.showMessage( std::string( "Move failed: " ) + e.what( ) );
            return;
        }
        view_.showMessage( "Move executed" );

        const std::vector<UnitRenderData> after_units = buildRenderDataSnapshot( );
        const std::uint64_t mover_id = makeUnitId( active_unit );

        view_.clearVisualEvents( );
        view_.updateRenderData( before_units );
        view_.syncUnitPositions( );
        if ( intended_path.size( ) >= 2 ) {
            queueMoveVisualAlongPath( mover_id, intended_path );
        } else {
            queueMoveVisualIfNeeded( mover_id, before_units, after_units );
        }
        view_.queueRenderDataCommit( after_units );

        model_.nextTurn( );
        finalizeActionVisuals( mover_id, had_morale );
    }
}

void BattlePresenter::onMouseHover( int pixel_x, int pixel_y, bool shift_held ) {
    lastCursorPx_ = pixel_x;
    lastCursorPy_ = pixel_y;

    Unit* active_unit = model_.getCurrentUnit( );
    if ( active_unit == nullptr ) {
        view_.clearActiveUnitHighlight( );
        view_.clearHoverDestinationHighlight( );
        view_.clearAttackOriginHighlights( );
        view_.setCursorStyle( CursorStyle::DEFAULT, pixel_x, pixel_y );
        return;
    }

    const auto [q, r] =
        pixelToHex( static_cast<float>( pixel_x ), static_cast<float>( pixel_y ) );
    const int s = -q - r;

    Hex* hovered_hex = nullptr;
    try {
        hovered_hex = &model_.getBoard( ).getHex( q, r, s );
    } catch ( const std::out_of_range& ) {
        view_.clearHoverDestinationHighlight( );
        view_.clearAttackOriginHighlights( );
        if ( rangePreviewActive_ ) {
            refreshUiForActiveUnit( );
            rangePreviewActive_ = false;
        }
        view_.setCursorStyle( CursorStyle::DEFAULT, pixel_x, pixel_y );
        return;
    }

    if ( shift_held && hovered_hex->hasUnit( ) ) {
        view_.clearHoverDestinationHighlight( );
        view_.clearAttackOriginHighlights( );
        view_.setShiftPreviewActive( true );
        showUnitRangePreview( *hovered_hex->getUnit( ) );
        rangePreviewActive_ = true;
        view_.setCursorStyle( CursorStyle::QUESTION_MARK, pixel_x, pixel_y );
        return;
    }

    if ( rangePreviewActive_ ) {
        view_.setShiftPreviewActive( false );
        refreshUiForActiveUnit( );
        rangePreviewActive_ = false;
    }

    if ( const Hex* move_head_hex = resolveMoveHeadDestination( *active_unit, *hovered_hex );
         move_head_hex != nullptr ) {
        const sf::Vector2f start_px = hexToPixel( active_unit->getQ( ), active_unit->getR( ) );
        const sf::Vector2f hovered_px =
            hexToPixel( move_head_hex->getQ( ), move_head_hex->getR( ) );

        bool future_is_facing_left = active_unit->getVisualFacingLeft( );
        if ( hovered_px.x < start_px.x ) {
            future_is_facing_left = true;
        } else if ( hovered_px.x > start_px.x ) {
            future_is_facing_left = false;
        }

        if ( active_unit->getSize( ) == 2 ) {
            const int tail_dq = future_is_facing_left ? 1 : -1;
            view_.setHoverDestinationHighlight( move_head_hex->getQ( ),
                                                  move_head_hex->getR( ),
                                                  true,
                                                  move_head_hex->getQ( ) + tail_dq,
                                                  move_head_hex->getR( ) );
        } else {
            view_.setHoverDestinationHighlight(
                move_head_hex->getQ( ), move_head_hex->getR( ), false, 0, 0 );
        }
    } else {
        view_.clearHoverDestinationHighlight( );
    }

    const bool is_enemy = hovered_hex->hasUnit( ) &&
                          hovered_hex->getUnit( ).get( ) != active_unit &&
                          model_.areEnemies( *active_unit, *hovered_hex->getUnit( ) );

    if ( ! is_enemy ) {
        view_.clearAttackOriginHighlights( );

        const bool reachable =
            ( resolveMoveHeadDestination( *active_unit, *hovered_hex ) != nullptr );
        CursorStyle empty_style = CursorStyle::NOT_AVAILABLE;
        if ( reachable ) {
            empty_style = active_unit->isTeleporterUnit( ) ? CursorStyle::FLY_MOVE
                                                             : CursorStyle::NORMAL_MOVE;
        }
        view_.setCursorStyle( empty_style, pixel_x, pixel_y );
        return;
    }

    if ( active_unit->isRanged( ) && active_unit->getAmmo( ) > 0 ) {
        Unit* hovered_unit = hovered_hex->getUnit( ).get( );
        if ( hovered_unit != nullptr && model_.willShoot( *active_unit, *hovered_unit ) ) {
            const int dist = ActionManager::hexDistance( *active_unit, *hovered_unit );
            view_.setCursorStyle( dist > 10 ? CursorStyle::BROKEN_ARROW : CursorStyle::RANGE_SHOOT,
                                   pixel_x,
                                   pixel_y );

            for ( const Hex* hex : cachedDestinations_ ) {
                if ( hex == nullptr ) {
                    continue;
}
                view_.highlightHex( hex->getQ( ), hex->getR( ), HighlightType::WALKABLE );
                if ( active_unit->getSize( ) != 2 ) {
                    continue;
}
                bool facing_left = active_unit->getVisualFacingLeft( );
                const int predicted_tail_dq = facing_left ? 1 : -1;
                view_.highlightHex(
                    hex->getQ( ) + predicted_tail_dq, hex->getR( ), HighlightType::WALKABLE );
                view_.highlightHex(
                    hex->getQ( ) - predicted_tail_dq, hex->getR( ), HighlightType::WALKABLE );
            }
            return;
        }
    }

    const PickedApproach picked = pickAttackApproachForCursor(
        *active_unit, *hovered_hex, static_cast<float>( pixel_x ), static_cast<float>( pixel_y ) );
    Hex* approach_hex = picked.approach_;

    if ( approach_hex == nullptr ) {
        view_.clearAttackOriginHighlights( );
        view_.setCursorStyle( CursorStyle::NOT_AVAILABLE, pixel_x, pixel_y );
        return;
    }

    if ( ! picked.allOrigins_.empty( ) ) {
        view_.setAttackOriginHighlights( picked.allOrigins_ );
    } else {
        view_.clearAttackOriginHighlights( );
    }

    {
        bool has_tail = false;
        int tail_q = 0;
        int tail_r = 0;
        if ( active_unit->getSize( ) == 2 ) {
            // Tail offset is a function of the unit's LOGICAL army side
            // (set once at placement and never flipped).  Using
            // visual_facing_left here lied for left-army 2-hex units that
            // momentarily face right after a walk and put the highlighted
            // tail on the wrong column relative to where the model actually
            // executes the attack.
            const int tail_dq = active_unit->isFacingLeft( ) ? 1 : -1;
            has_tail = true;
            tail_q = approach_hex->getQ( ) + tail_dq;
            tail_r = approach_hex->getR( );
        }
        view_.setHoverDestinationHighlight(
            approach_hex->getQ( ), approach_hex->getR( ), has_tail, tail_q, tail_r );

        for ( const Hex* hex : cachedDestinations_ ) {
            if ( hex == nullptr ) {
                continue;
}
            view_.highlightHex( hex->getQ( ), hex->getR( ), HighlightType::WALKABLE );
            if ( active_unit->getSize( ) != 2 ) {
                continue;
}
            const int predicted_tail_dq = active_unit->isFacingLeft( ) ? 1 : -1;
            view_.highlightHex(
                hex->getQ( ) + predicted_tail_dq, hex->getR( ), HighlightType::WALKABLE );
            view_.highlightHex(
                hex->getQ( ) - predicted_tail_dq, hex->getR( ), HighlightType::WALKABLE );
        }
    }

    // -- Sword direction -------------------------------------------------
    // Find the (attacker_hex, target_hex) ADJACENT pair that corresponds to
    // the chosen approach and target.  We score by "minimum strike distance
    // -- both hexes adjacent" so 2-hex attackers get the diagonal pair when
    // one exists rather than a same-row head-to-head pair that yields a
    // sword-handle parallel to the target's body.
    int attack_head_q = approach_hex->getQ( );
    int attack_head_r = approach_hex->getR( );

    std::vector<std::pair<int, int>> attacker_body{ { attack_head_q, attack_head_r } };
    if ( active_unit->getSize( ) == 2 ) {
        const int tail_dq = active_unit->isFacingLeft( ) ? 1 : -1;
        attacker_body.emplace_back( attack_head_q + tail_dq, attack_head_r );
    }

    std::vector<std::pair<int, int>> target_body{
        { hovered_hex->getQ( ), hovered_hex->getR( ) } };
    if ( const auto& tu = hovered_hex->getUnit( ); tu && tu->getSize( ) == 2 ) {
        const int tdq = tu->isFacingLeft( ) ? 1 : -1;
        target_body.emplace_back( tu->getQ( ) + tdq, tu->getR( ) );
    }

    auto are_adj_hex = []( int aq, int ar, int bq, int br ) {
        const int as = -aq - ar;
        const int bs = -bq - br;
        return std::max( { std::abs( aq - bq ), std::abs( ar - br ), std::abs( as - bs ) } ) == 1;
    };

    const sf::Vector2f fpx{ static_cast<float>( pixel_x ), static_cast<float>( pixel_y ) };
    sf::Vector2f attacker_strike = hexToPixel( attack_head_q, attack_head_r );
    sf::Vector2f target_strike = hexToPixel( hovered_hex->getQ( ), hovered_hex->getR( ) );
    float best_target_d2 = std::numeric_limits<float>::max( );
    for ( const auto& [aq, ar] : attacker_body ) {
        for ( const auto& [tq, tr] : target_body ) {
            if ( ! are_adj_hex( aq, ar, tq, tr ) ) {
                continue;
}
            const sf::Vector2f tp = hexToPixel( tq, tr );
            const float d2 =
                ( tp.x - fpx.x ) * ( tp.x - fpx.x ) + ( tp.y - fpx.y ) * ( tp.y - fpx.y );
            if ( d2 < best_target_d2 ) {
                best_target_d2 = d2;
                attacker_strike = hexToPixel( aq, ar );
                target_strike = tp;
            }
        }
    }

    // Vector points ATTACKER -> TARGET -- the direction the blade swings.
    // The cursor frames are oriented such that this angle places the blade
    // tip on the target side (biting the target) and the handle on the
    // attacker side, matching player intuition.  Reversing the convention
    // (which previously computed attacker minus target) put the handle on
    // the target which is what the user reported as "rotated 180 degrees".
    const float dx = target_strike.x - attacker_strike.x;
    const float dy = target_strike.y - attacker_strike.y;
    const float angle_deg = std::atan2( dy, dx ) * ( 180.0f / K_PI );

    view_.setCursorStyle( directionToCursor( angle_deg ), pixel_x, pixel_y );
}

void BattlePresenter::onRightClickPressed( int pixel_x, int pixel_y ) {
    const auto [q, r] =
        pixelToHex( static_cast<float>( pixel_x ), static_cast<float>( pixel_y ) );
    const int s = -q - r;

    try {
        const Hex& hex = model_.getBoard( ).getHex( q, r, s );

        if ( hex.hasUnit( ) ) {
            const std::shared_ptr<Unit> unit = hex.getUnit( );
            UnitRenderData data = makeUnitRenderData( model_, *unit, q, r, false );
            view_.showUnitInfoPanel( data );
            infoPanelVisible_ = true;
            return;
        }

        const auto& dead_units = hex.getDeadUnits( );
        for ( auto it = dead_units.rbegin( ); it != dead_units.rend( ); ++it ) {
            if ( const std::shared_ptr<Unit> dead = it->lock( ) ) {
                UnitRenderData data = makeUnitRenderData( model_, *dead, q, r, true );
                view_.showUnitInfoPanel( data );
                infoPanelVisible_ = true;
                return;
            }
        }
    } catch ( const std::out_of_range& ) {}

    view_.hideUnitInfoPanel( );
    infoPanelVisible_ = false;
}

void BattlePresenter::onRightClickReleased( ) {
    view_.hideUnitInfoPanel( );
    infoPanelVisible_ = false;
}

void BattlePresenter::onDefendClicked( ) {
    view_.clearVisualEvents( );
    view_.setIdleCallback( nullptr );

    Unit* active_unit = model_.getCurrentUnit( );
    if ( active_unit == nullptr ) {
        return;
    }

    const bool had_morale = model_.activeUnitHasMoraleBonus( );
    const std::uint64_t actor_id = makeUnitId( active_unit );
    model_.defend( *active_unit );
    view_.clearHoverDestinationHighlight( );
    view_.showMessage( "Unit defends" );
    pushRenderDataToView( );
    view_.syncUnitPositions( );
    model_.nextTurn( );
    finalizeActionVisuals( actor_id, had_morale );
}

void BattlePresenter::onWaitClicked( ) {
    view_.clearVisualEvents( );
    view_.setIdleCallback( nullptr );

    Unit* active_unit = model_.getCurrentUnit( );
    if ( active_unit == nullptr ) {
        return;
    }

    const std::uint64_t actor_id = makeUnitId( active_unit );
    model_.wait( *active_unit );
    view_.clearHoverDestinationHighlight( );
    view_.showMessage( "Unit waits" );
    pushRenderDataToView( );
    view_.syncUnitPositions( );
    model_.nextTurn( );
    finalizeActionVisuals( actor_id, false );
}

void BattlePresenter::refreshUiForActiveUnit( ) {
    view_.clearAllHighlights( );
    view_.clearHoverDestinationHighlight( );
    view_.clearAttackOriginHighlights( );
    cachedAttackOriginsByTarget_.clear( );

    Unit* active_unit = model_.getCurrentUnit( );
    if ( active_unit == nullptr ) {
        view_.clearActiveUnitHighlight( );
        view_.showMessage( "Battle Over" );
        return;
    }

    view_.updateHud(
        active_unit->getName( ), active_unit->getCount( ), active_unit->getHealthLeft( ) );

    constexpr std::size_t K_LOOKAHEAD_CAPACITY = 12;
    std::vector<IBattleView::TurnQueueSlot> slots;
    slots.reserve( K_LOOKAHEAD_CAPACITY );

    bool first = true;
    for ( Unit* unit : model_.getUnitQueueInRound( ) ) {
        if ( slots.size( ) >= K_LOOKAHEAD_CAPACITY ) {
            break;
}
        if ( unit == nullptr || unit->getCount( ) <= 0 ) {
            continue;
}
        IBattleView::TurnQueueSlot slot;
        slot.unitName_ = unit->getName( );
        slot.isActive_ = first;
        slots.push_back( std::move( slot ) );
        first = false;
    }

    if ( slots.size( ) < K_LOOKAHEAD_CAPACITY ) {
        IBattleView::TurnQueueSlot divider;
        divider.isDivider_ = true;
        divider.roundNumber_ = model_.getRoundNumber( ) + 1;
        slots.push_back( std::move( divider ) );

        for ( Unit* unit : model_.peekNextRoundOrder( ) ) {
            if ( slots.size( ) >= K_LOOKAHEAD_CAPACITY ) {
                break;
}
            if ( unit == nullptr || unit->getCount( ) <= 0 ) {
                continue;
}
            IBattleView::TurnQueueSlot slot;
            slot.unitName_ = unit->getName( );
            slots.push_back( std::move( slot ) );
        }
    }
    view_.updateTurnOrder( slots );
    view_.setActiveUnitHighlight( active_unit->getQ( ),
                                    active_unit->getR( ),
                                    active_unit->getSize( ),
                                    active_unit->isFacingLeft( ) );

    const int active_tail_dq = active_unit->isFacingLeft( ) ? 1 : -1;
    view_.highlightHex( active_unit->getQ( ), active_unit->getR( ), HighlightType::ACTIVE_UNIT );
    if ( active_unit->getSize( ) == 2 ) {
        view_.highlightHex( active_unit->getQ( ) + active_tail_dq,
                            active_unit->getR( ),
                            HighlightType::ACTIVE_UNIT );
    }

    cachedDestinations_ = model_.getAvailableDestinations( *active_unit );
    cachedDestinationsSet_.clear( );
    cachedDestinationsSet_.reserve( cachedDestinations_.size( ) * 2 );
    for ( const Hex* h : cachedDestinations_ ) {
        if ( h != nullptr ) {
            cachedDestinationsSet_.insert( makeHexKey( h->getQ( ), h->getR( ) ) );
}
    }
    const std::vector<Hex*>& destinations = cachedDestinations_;
    std::vector<IBattleView::PredictedFacing> predictions;
    predictions.reserve( destinations.size( ) );
    for ( Hex* dest : destinations ) {
        if ( dest == nullptr ) {
            continue;
}
        const std::vector<const Hex*> path = model_.findPath( *active_unit, *dest );
        bool facing_left = active_unit->isFacingLeft( );
        if ( path.size( ) >= 2 ) {
            const Hex* penult = path[path.size( ) - 2];
            const sf::Vector2f penult_px = hexToPixel( penult->getQ( ), penult->getR( ) );
            const sf::Vector2f final_px = hexToPixel( dest->getQ( ), dest->getR( ) );
            constexpr float K_FLIP_DEAD_ZONE = 1.0f;
            if ( final_px.x < penult_px.x - K_FLIP_DEAD_ZONE ) {
                facing_left = true;
            } else if ( final_px.x > penult_px.x + K_FLIP_DEAD_ZONE ) {
                facing_left = false;
}
        }
        predictions.push_back( { dest->getQ( ), dest->getR( ), facing_left } );
    }

    for ( Hex* hex : destinations ) {
        if ( hex == nullptr ) {
            continue;
}
        view_.highlightHex( hex->getQ( ), hex->getR( ), HighlightType::WALKABLE );
        if ( active_unit->getSize( ) != 2 ) {
            continue;
}

        bool facing_left = active_unit->isFacingLeft( );
        for ( const IBattleView::PredictedFacing& p : predictions ) {
            if ( p.q_ == hex->getQ( ) && p.r_ == hex->getR( ) ) {
                facing_left = p.facingLeft_;
                break;
            }
        }
        const int predicted_tail_dq = facing_left ? 1 : -1;
        view_.highlightHex(
            hex->getQ( ) + predicted_tail_dq, hex->getR( ), HighlightType::WALKABLE );

        view_.highlightHex(
            hex->getQ( ) - predicted_tail_dq, hex->getR( ), HighlightType::WALKABLE );
    }

    view_.setPredictedFacings( predictions );

    const auto& grid = model_.getBoard( ).getGrid( );
    for ( const Hex& hex : grid ) {
        if ( ! hex.hasUnit( ) ) {
            continue;
}
        const std::shared_ptr<Unit> maybe_target = hex.getUnit( );
        if ( ! maybe_target ) {
            continue;
}
        Unit* target = maybe_target.get( );
        if ( target == active_unit ) {
            continue;
}
        if ( hex.getQ( ) != target->getQ( ) || hex.getR( ) != target->getR( ) ) {
            continue;
}
        if ( ! model_.areEnemies( *active_unit, *target ) ) {
            continue;
}

        const std::vector<IBattleView::AttackOriginHex> origins =
            buildAttackOriginsForTarget( *active_unit, *target, destinations, predictions );
        if ( ! origins.empty( ) ) {
            cachedAttackOriginsByTarget_[makeUnitId( target )] = origins;
        }
    }

    const std::vector<std::pair<Unit*, Hex*>> attacks = model_.getAvailableAttacks( *active_unit );
    for ( const auto& [target, hex] : attacks ) {
        if ( target != nullptr && hex != nullptr ) {
            highlightUnitBody( *target, HighlightType::ATTACKABLE );
        }
    }

    for ( Unit* candidate : model_.peekNextRoundOrder( ) ) {
        if ( candidate == nullptr || candidate == active_unit ) {
            continue;
}
        if ( candidate->getCount( ) <= 0 ) {
            continue;
}
        if ( ! model_.areEnemies( *active_unit, *candidate ) ) {
            continue;
}
        if ( ! model_.willShoot( *active_unit, *candidate ) ) {
            continue;
}
        highlightUnitBody( *candidate, HighlightType::ATTACKABLE );
    }

    static constexpr int K_DQ[] = { 1, 1, 0, -1, -1, 0 };
    static constexpr int K_DR[] = { 0, -1, -1, 0, 1, 1 };
    const int active_tail_dq_for_attacks = active_unit->isFacingLeft( ) ? 1 : -1;
    for ( const Hex* dest : destinations ) {
        if ( dest == nullptr ) {
            continue;
}
        std::vector<std::pair<int, int>> origins{ { dest->getQ( ), dest->getR( ) } };
        if ( active_unit->getSize( ) == 2 ) {
            origins.emplace_back( dest->getQ( ) + active_tail_dq_for_attacks, dest->getR( ) );
        }
        for ( const auto& [oq, orr] : origins ) {
            for ( int i = 0; i < 6; ++i ) {
                const int nq = oq + K_DQ[i];
                const int nr = orr + K_DR[i];
                try {
                    const Hex& nhex = model_.getBoard( ).getHex( nq, nr, -nq - nr );
                    if ( ! nhex.hasUnit( ) ) {
                        continue;
}
                    if ( nhex.getUnit( ).get( ) == active_unit ) {
                        continue;
}
                    if ( ! model_.areEnemies( *active_unit, *nhex.getUnit( ) ) ) {
                        continue;
}
                    highlightUnitBody( *nhex.getUnit( ), HighlightType::ATTACKABLE );
                } catch ( const std::out_of_range& ) {}
            }
        }
    }
}

void BattlePresenter::showUnitRangePreview( const Unit& unit ) {
    view_.clearAllHighlights( );
    view_.clearHoverDestinationHighlight( );
    view_.clearAttackOriginHighlights( );
    highlightUnitBody( unit, HighlightType::ACTIVE_UNIT );

    auto are_adjacent = []( int aq, int ar, int bq, int br ) {
        const int as = -aq - ar;
        const int bs = -bq - br;
        return std::max( { std::abs( aq - bq ), std::abs( ar - br ), std::abs( as - bs ) } ) == 1;
    };

    const std::vector<Hex*> destinations = model_.getAvailableDestinations( unit );

    const int unit_tail_dq = unit.isFacingLeft( ) ? 1 : -1;
    for ( Hex* dest : destinations ) {
        if ( dest == nullptr ) {
            continue;
}
        view_.highlightHex( dest->getQ( ), dest->getR( ), HighlightType::WALKABLE );
        if ( unit.getSize( ) == 2 ) {
            view_.highlightHex(
                dest->getQ( ) + unit_tail_dq, dest->getR( ), HighlightType::WALKABLE );
        }
    }

    for ( Unit* candidate : model_.peekNextRoundOrder( ) ) {
        if ( candidate == nullptr || candidate == &unit ) {
            continue;
}
        if ( candidate->getCount( ) <= 0 ) {
            continue;
}
        if ( ! model_.areEnemies( unit, *candidate ) ) {
            continue;
}

        bool can_hit = false;

        for ( const auto& [target, hex] : model_.getAvailableAttacks( unit ) ) {
            if ( target == candidate ) {
                can_hit = true;
                break;
            }
        }

        if ( ! can_hit && model_.willShoot( unit, *candidate ) ) {
            can_hit = true;
}

        if ( ! can_hit ) {
            std::vector<std::pair<int, int>> body;
            body.emplace_back( candidate->getQ( ), candidate->getR( ) );
            if ( candidate->getSize( ) == 2 ) {
                const int tail_dq = candidate->isFacingLeft( ) ? 1 : -1;
                body.emplace_back( candidate->getQ( ) + tail_dq, candidate->getR( ) );
            }
            const int attacker_tail_dq = unit.isFacingLeft( ) ? 1 : -1;
            for ( Hex* dest : destinations ) {
                if ( dest == nullptr ) {
                    continue;
}
                std::vector<std::pair<int, int>> attacker_body{
                    { dest->getQ( ), dest->getR( ) } };
                if ( unit.getSize( ) == 2 ) {
                    attacker_body.emplace_back( dest->getQ( ) + attacker_tail_dq, dest->getR( ) );
                }
                for ( const auto& [aq, ar] : attacker_body ) {
                    for ( const auto& [bq, br] : body ) {
                        if ( are_adjacent( aq, ar, bq, br ) ) {
                            can_hit = true;
                            break;
                        }
                    }
                    if ( can_hit ) {
                        break;
}
                }
                if ( can_hit ) {
                    break;
}
            }
        }

        if ( can_hit ) {
            highlightUnitBody( *candidate, HighlightType::ATTACKABLE );
}
    }
}

void BattlePresenter::pushRenderDataToView( ) {
    view_.updateRenderData( buildRenderDataSnapshot( ) );
}

std::vector<IBattleView::AttackOriginHex> BattlePresenter::buildAttackOriginsForTarget(
    const Unit& attacker,
    const Unit& target,
    const std::vector<Hex*>& destinations,
    const std::vector<IBattleView::PredictedFacing>& ) const {
    auto are_adjacent = []( int aq, int ar, int bq, int br ) {
        const int as = -aq - ar;
        const int bs = -bq - br;
        const int d = std::max( { std::abs( aq - bq ), std::abs( ar - br ), std::abs( as - bs ) } );
        return d == 1;
    };

    std::vector<std::pair<int, int>> target_body;
    target_body.emplace_back( target.getQ( ), target.getR( ) );
    if ( target.getSize( ) == 2 ) {
        const int tail_dq = target.isFacingLeft( ) ? 1 : -1;
        target_body.emplace_back( target.getQ( ) + tail_dq, target.getR( ) );
    }

    const int attacker_tail_dq = attacker.isFacingLeft( ) ? 1 : -1;

    std::vector<IBattleView::AttackOriginHex> out;
    out.reserve( destinations.size( ) );
    for ( Hex* dest : destinations ) {
        if ( dest == nullptr ) {
            continue;
}

        std::vector<std::pair<int, int>> attacker_body;
        attacker_body.emplace_back( dest->getQ( ), dest->getR( ) );

        IBattleView::AttackOriginHex origin;
        origin.q_ = dest->getQ( );
        origin.r_ = dest->getR( );
        if ( attacker.getSize( ) == 2 ) {
            attacker_body.emplace_back( dest->getQ( ) + attacker_tail_dq, dest->getR( ) );
            origin.hasTail_ = true;
            origin.tailQ_ = dest->getQ( ) + attacker_tail_dq;
            origin.tailR_ = dest->getR( );
        }

        bool can_strike = false;
        for ( const auto& [aq, ar] : attacker_body ) {
            for ( const auto& [tq, tr] : target_body ) {
                if ( are_adjacent( aq, ar, tq, tr ) ) {
                    can_strike = true;
                    break;
                }
            }
            if ( can_strike ) {
                break;
}
        }

        if ( can_strike ) {
            out.push_back( origin );
        }
    }
    return dedupeAttackOrigins( out );
}

std::vector<IBattleView::AttackOriginHex>
BattlePresenter::dedupeAttackOrigins( const std::vector<IBattleView::AttackOriginHex>& origins ) {
    std::vector<IBattleView::AttackOriginHex> out;
    out.reserve( origins.size( ) );
    std::unordered_set<std::int64_t> seen_heads;
    for ( const IBattleView::AttackOriginHex& o : origins ) {
        const std::int64_t key = makeHexKey( o.q_, o.r_ );
        if ( ! seen_heads.insert( key ).second ) {
            continue;
}
        out.push_back( o );
    }
    return out;
}

const std::vector<IBattleView::AttackOriginHex>*
BattlePresenter::getCachedAttackOriginsForTarget( const Unit& target ) const {
    const auto it = cachedAttackOriginsByTarget_.find( makeUnitId( &target ) );
    if ( it == cachedAttackOriginsByTarget_.end( ) ) {
        return nullptr;
    }
    return &it->second;
}

bool BattlePresenter::isDestinationCached( int q, int r ) const {
    return cachedDestinationsSet_.find( makeHexKey( q, r ) ) != cachedDestinationsSet_.end( );
}

const Hex*
BattlePresenter::resolveMoveHeadDestination( const Unit& unit,
                                                const Hex& clicked_or_hovered_hex ) const {
    if ( isDestinationCached( clicked_or_hovered_hex.getQ( ),
                                clicked_or_hovered_hex.getR( ) ) ) {
        return &clicked_or_hovered_hex;
    }
    if ( unit.getSize( ) != 2 ) {
        return nullptr;
    }

    for ( const int tail_dq : { -1, 1 } ) {
        const int head_q = clicked_or_hovered_hex.getQ( ) - tail_dq;
        const int head_r = clicked_or_hovered_hex.getR( );
        const int head_s = -head_q - head_r;
        if ( ! isDestinationCached( head_q, head_r ) ) {
            continue;
}
        try {
            return &model_.getBoard( ).getHex( head_q, head_r, head_s );
        } catch ( const std::out_of_range& ) {}
    }
    return nullptr;
}

void BattlePresenter::highlightUnitBody( const Unit& unit, HighlightType type ) const {
    view_.highlightHex( unit.getQ( ), unit.getR( ), type );
    if ( unit.getSize( ) == 2 ) {
        const int tail_dq = unit.isFacingLeft( ) ? 1 : -1;
        view_.highlightHex( unit.getQ( ) + tail_dq, unit.getR( ), type );
    }
}

std::vector<UnitRenderData> BattlePresenter::buildRenderDataSnapshot( ) const {
    std::vector<UnitRenderData> units;
    const auto& grid = model_.getBoard( ).getGrid( );
    std::unordered_set<std::uint64_t> emitted_corpse_ids;

    for ( const Hex& hex : grid ) {
        if ( hex.hasUnit( ) ) {
            auto unit = hex.getUnit( );

            if ( hex.getQ( ) != unit->getQ( ) || hex.getR( ) != unit->getR( ) ) {
                continue;
}
            units.push_back(
                makeUnitRenderData( model_, *unit, hex.getQ( ), hex.getR( ), false ) );
        }

        for ( const std::weak_ptr<Unit>& dead_weak : hex.getDeadUnits( ) ) {
            if ( const std::shared_ptr<Unit> dead = dead_weak.lock( ) ) {
                const std::uint64_t dead_id =
                    static_cast<std::uint64_t>( reinterpret_cast<std::uintptr_t>( dead.get( ) ) );

                if ( ! emitted_corpse_ids.insert( dead_id ).second ) {
                    continue;
                }
                units.push_back(
                    makeUnitRenderData( model_, *dead, dead->getQ( ), dead->getR( ), true ) );
            }
        }
    }

    return units;
}

std::uint64_t BattlePresenter::makeUnitId( const Unit* unit ) {
    return static_cast<std::uint64_t>( reinterpret_cast<std::uintptr_t>( unit ) );
}

std::optional<UnitRenderData> BattlePresenter::findUnit( const std::vector<UnitRenderData>& units,
                                                          std::uint64_t id ) {
    for ( const UnitRenderData& unit : units ) {
        if ( unit.id_ == id ) {
            return unit;
        }
    }
    return std::nullopt;
}

void BattlePresenter::finalizeActionVisuals( std::uint64_t actor_id, bool had_morale_bonus ) {
    view_.clearAllHighlights( );
    view_.clearActiveUnitHighlight( );
    view_.clearHoverDestinationHighlight( );
    view_.clearAttackOriginHighlights( );
    view_.setPredictedFacings( { } );
    cachedAttackOriginsByTarget_.clear( );
    cachedDestinations_.clear( );
    cachedDestinationsSet_.clear( );

    if ( had_morale_bonus ) {
        view_.queueMoraleAnimation( actor_id );
        view_.showMessage( "Good morale! Bonus action." );
    }

    view_.setIdleCallback( [this] { refreshUiForActiveUnit( ); } );

    if ( ! view_.hasPendingVisualEvents( ) ) {
        view_.setIdleCallback( nullptr );
        refreshUiForActiveUnit( );
    }
}

void BattlePresenter::queueMoveVisualAlongPath( std::uint64_t unit_id,
                                                    const std::vector<std::pair<int, int>>& path ) {
    if ( path.size( ) < 2 ) {
        return;
}

    constexpr float K_SECONDS_PER_HEX = 0.08f;

    for ( Unit* u : model_.getUnitQueueInRound( ) ) {
        if ( u == nullptr || makeUnitId( u ) != unit_id ) {
            continue;
}
        if ( ! u->ignoresPathBlockers( ) ) {
            break;
}
        const auto& [from_q, from_r] = path.front( );
        const auto& [to_q, to_r] = path.back( );
        const int dq = to_q - from_q;
        const int dr = to_r - from_r;
        const int ds = -dq - dr;
        const int hex_dist = std::max( { std::abs( dq ), std::abs( dr ), std::abs( ds ) } );
        const float duration_seconds =
            K_SECONDS_PER_HEX * static_cast<float>( std::max( 1, hex_dist ) );
        view_.queueMoveAnimation( unit_id, from_q, from_r, to_q, to_r, duration_seconds );
        return;
    }

    for ( std::size_t i = 1; i < path.size( ); ++i ) {
        const auto& [from_q, from_r] = path[i - 1];
        const auto& [to_q, to_r] = path[i];
        const int dq = to_q - from_q;
        const int dr = to_r - from_r;
        const int ds = -dq - dr;
        const int hex_dist = std::max( { std::abs( dq ), std::abs( dr ), std::abs( ds ) } );
        const float duration_seconds =
            K_SECONDS_PER_HEX * static_cast<float>( std::max( 1, hex_dist ) );
        view_.queueMoveAnimation( unit_id, from_q, from_r, to_q, to_r, duration_seconds );
    }
}

void BattlePresenter::queueMoveVisualIfNeeded( std::uint64_t unit_id,
                                                   const std::vector<UnitRenderData>& before,
                                                   const std::vector<UnitRenderData>& after ) {
    const std::optional<UnitRenderData> before_unit = findUnit( before, unit_id );
    const std::optional<UnitRenderData> after_unit = findUnit( after, unit_id );
    if ( ! before_unit.has_value( ) || ! after_unit.has_value( ) ) {
        return;
}
    if ( before_unit->q_ == after_unit->q_ && before_unit->r_ == after_unit->r_ ) {
        return;
}

    Unit* unit_ptr = nullptr;
    for ( Unit* u : model_.getUnitQueueInRound( ) ) {
        if ( u != nullptr && makeUnitId( u ) == unit_id ) {
            unit_ptr = u;
            break;
        }
    }

    auto queue_single_segment = [&]( int from_q, int from_r, int to_q, int to_r ) {
        const int dq = to_q - from_q;
        const int dr = to_r - from_r;
        const int ds = -dq - dr;
        const int hex_distance = std::max( { std::abs( dq ), std::abs( dr ), std::abs( ds ) } );

        constexpr float K_SECONDS_PER_HEX = 0.08f;
        const float duration_seconds =
            K_SECONDS_PER_HEX * static_cast<float>( std::max( 1, hex_distance ) );
        view_.queueMoveAnimation( unit_id, from_q, from_r, to_q, to_r, duration_seconds );
    };

    if ( unit_ptr != nullptr ) {
        try {
            const int s = -after_unit->q_ - after_unit->r_;
            const Hex& dest_hex = model_.getBoard( ).getHex( after_unit->q_, after_unit->r_, s );
            const std::vector<const Hex*> chain = model_.findPath( *unit_ptr, dest_hex );
            if ( chain.size( ) >= 2 ) {
                for ( std::size_t i = 1; i < chain.size( ); ++i ) {
                    queue_single_segment( chain[i - 1]->getQ( ),
                                          chain[i - 1]->getR( ),
                                          chain[i]->getQ( ),
                                          chain[i]->getR( ) );
                }
                return;
            }
        } catch ( const std::out_of_range& ) {}
    }

    queue_single_segment( before_unit->q_, before_unit->r_, after_unit->q_, after_unit->r_ );
}

std::pair<int, int> BattlePresenter::pixelToHex( float x, float y ) const {
    const float px = x - K_GRID_ORIGIN_X;
    const float py = y - K_GRID_ORIGIN_Y;

    const float fq = ( std::sqrt( 3.0f ) / 3.0f * px - 1.0f / 3.0f * py ) / K_HEX_RADIUS;
    const float fr = ( 2.0f / 3.0f * py ) / K_HEX_RADIUS;
    const float fs = -fq - fr;

    return cubeRoundToAxial( fq, fr, fs );
}

sf::Vector2f BattlePresenter::hexToPixel( int q, int r ) const {
    const float px = K_GRID_ORIGIN_X +
                     K_HEX_RADIUS * ( std::sqrt( 3.0f ) *
                                    ( static_cast<float>( q ) + static_cast<float>( r ) * 0.5f ) );
    const float py = K_GRID_ORIGIN_Y + K_HEX_RADIUS * ( 1.5f * static_cast<float>( r ) );
    return { px, py };
}

Hex* BattlePresenter::findAttackApproach( const Unit& attacker,
                                            const Hex& target_hex,
                                            float pixel_x,
                                            float pixel_y ) const {
    const std::vector<Hex*>& reachable =
        cachedDestinations_.empty( ) ? ( const_cast<BattlePresenter*>( this )->cachedDestinations_ =
                                             model_.getAvailableDestinations( attacker ) )
                                     : cachedDestinations_;
    if ( reachable.empty( ) ) {
        return nullptr;
}

    auto are_adjacent = []( int aq, int ar, int bq, int br ) {
        const int as = -aq - ar;
        const int bs = -bq - br;
        const int d = std::max( { std::abs( aq - bq ), std::abs( ar - br ), std::abs( as - bs ) } );
        return d == 1;
    };

    std::vector<std::pair<int, int>> body_hexes;
    body_hexes.emplace_back( target_hex.getQ( ), target_hex.getR( ) );
    if ( target_hex.hasUnit( ) ) {
        const Unit* t = target_hex.getUnit( ).get( );
        if ( t && t->getSize( ) == 2 ) {
            const int tdq = t->isFacingLeft( ) ? 1 : -1;
            body_hexes.emplace_back( t->getQ( ) + tdq, t->getR( ) );
        }
    }

    const int attacker_tail_dq = attacker.isFacingLeft( ) ? 1 : -1;

    std::vector<Hex*> candidates;
    candidates.reserve( reachable.size( ) );
    for ( Hex* candidate : reachable ) {
        if ( candidate == nullptr ) {
            continue;
}

        std::vector<std::pair<int, int>> attacker_body;
        attacker_body.emplace_back( candidate->getQ( ), candidate->getR( ) );
        if ( attacker.getSize( ) == 2 ) {
            attacker_body.emplace_back( candidate->getQ( ) + attacker_tail_dq,
                                        candidate->getR( ) );
        }

        bool can_strike = false;
        for ( const auto& [aq, ar] : attacker_body ) {
            for ( const auto& [tq, tr] : body_hexes ) {
                if ( are_adjacent( aq, ar, tq, tr ) ) {
                    can_strike = true;
                    break;
                }
            }
            if ( can_strike ) {
                break;
}
        }
        if ( can_strike ) {
            candidates.push_back( candidate );
}
    }

    if ( candidates.empty( ) ) {
        return nullptr;
}

    Hex* best = nullptr;
    float best_d2 = std::numeric_limits<float>::max( );
    for ( Hex* c : candidates ) {
        const sf::Vector2f cp = hexToPixel( c->getQ( ), c->getR( ) );
        const float dx = cp.x - pixel_x;
        const float dy = cp.y - pixel_y;
        if ( const float d2 = dx * dx + dy * dy; d2 < best_d2 ) {
            best_d2 = d2;
            best = c;
        }
    }
    return best;
}

BattlePresenter::PickedApproach BattlePresenter::pickAttackApproachForCursor(
    const Unit& attacker, const Hex& hovered_hex, float pixel_x, float pixel_y ) const {
    PickedApproach picked;
    if ( ! hovered_hex.hasUnit( ) ) {
        return picked;
}
    Unit* target = hovered_hex.getUnit( ).get( );
    if ( target == nullptr || target == &attacker ) {
        return picked;
}
    if ( ! model_.areEnemies( attacker, *target ) ) {
        return picked;
}

    picked.directlyAdjacent_ = model_.canAttack( attacker, hovered_hex );

    if ( const auto* cached = getCachedAttackOriginsForTarget( *target ); cached != nullptr ) {
        picked.allOrigins_ = *cached;
    }

    // The attacker's CURRENT body hex (head, plus tail for 2-hex) is a valid
    // origin too whenever it can already strike -- but the destination cache
    // doesn't list it (the unit isn't "moving" to itself).  Inject it so the
    // angle snap considers it alongside the move-and-attack origins; this is
    // what lets the player choose between "attack in place" and "reposition
    // first" when both are reachable.
    if ( picked.directlyAdjacent_ ) {
        IBattleView::AttackOriginHex self_origin;
        self_origin.q_ = attacker.getQ( );
        self_origin.r_ = attacker.getR( );
        if ( attacker.getSize( ) == 2 ) {
            const int tail_dq = attacker.isFacingLeft( ) ? 1 : -1;
            self_origin.hasTail_ = true;
            self_origin.tailQ_ = self_origin.q_ + tail_dq;
            self_origin.tailR_ = self_origin.r_;
        }
        const bool present = std::any_of(
            picked.allOrigins_.begin( ), picked.allOrigins_.end( ), [&]( const auto& o ) {
                return o.q_ == self_origin.q_ && o.r_ == self_origin.r_;
            } );
        if ( ! present ) {
            picked.allOrigins_.push_back( self_origin );
}
    }

    if ( picked.allOrigins_.empty( ) ) {
        if ( ! picked.directlyAdjacent_ ) {
            picked.approach_ = findAttackApproach( attacker, hovered_hex, pixel_x, pixel_y );
        }
        return picked;
    }

    // Defender body centre -- head + tail averaged for 2-hex creatures so
    // angles are measured from the real centroid of a large defender.
    sf::Vector2f defender_center = hexToPixel( hovered_hex.getQ( ), hovered_hex.getR( ) );
    if ( target->getSize( ) == 2 ) {
        const int tdq = target->isFacingLeft( ) ? 1 : -1;
        const sf::Vector2f tail = hexToPixel( target->getQ( ) + tdq, target->getR( ) );
        defender_center = { ( defender_center.x + tail.x ) * 0.5f,
                            ( defender_center.y + tail.y ) * 0.5f };
    }

    constexpr float K_PI = 3.14159265358979323846f;
    const sf::Vector2f fpx{ pixel_x, pixel_y };
    const float mouse_angle = std::atan2( fpx.y - defender_center.y, fpx.x - defender_center.x );

    // For each origin, measure angle from defender_center to the closer of
    // {head, tail} -- for 2-hex attackers attacking via tail, this anchors
    // the snap to the actual strike side rather than the trailing head.
    const IBattleView::AttackOriginHex* best = nullptr;
    float best_diff = std::numeric_limits<float>::max( );
    for ( const IBattleView::AttackOriginHex& origin : picked.allOrigins_ ) {
        sf::Vector2f op = hexToPixel( origin.q_, origin.r_ );
        if ( origin.hasTail_ ) {
            const sf::Vector2f tp = hexToPixel( origin.tailQ_, origin.tailR_ );
            const float dh = ( op.x - defender_center.x ) * ( op.x - defender_center.x ) +
                             ( op.y - defender_center.y ) * ( op.y - defender_center.y );
            const float dt = ( tp.x - defender_center.x ) * ( tp.x - defender_center.x ) +
                             ( tp.y - defender_center.y ) * ( tp.y - defender_center.y );
            if ( dt < dh ) {
                op = tp;
}
        }
        const float origin_angle = std::atan2( op.y - defender_center.y, op.x - defender_center.x );
        float diff = std::fabs( mouse_angle - origin_angle );
        if ( diff > K_PI ) {
            diff = 2.0f * K_PI - diff;
}
        if ( diff < best_diff ) {
            best_diff = diff;
            best = &origin;
        }
    }

    if ( best != nullptr ) {
        picked.origin_ = *best;
        try {
            picked.approach_ = &model_.getBoard( ).getHex( best->q_, best->r_, -best->q_ - best->r_ );
        } catch ( const std::out_of_range& ) {}
    }
    return picked;
}

CursorStyle BattlePresenter::directionToCursor( float angle_deg ) const {
    float a = angle_deg;
    if ( a < 0.0f ) {
        a += 360.0f;
}

    if ( a < 30.0f || a >= 330.0f ) {
        return CursorStyle::SWORD_E;
}
    if ( a < 90.0f ) {
        return CursorStyle::SWORD_SE;
}
    if ( a < 150.0f ) {
        return CursorStyle::SWORD_SW;
}
    if ( a < 210.0f ) {
        return CursorStyle::SWORD_W;
}
    if ( a < 270.0f ) {
        return CursorStyle::SWORD_NW;
}
    return CursorStyle::SWORD_NE;
}

} // namespace presenters