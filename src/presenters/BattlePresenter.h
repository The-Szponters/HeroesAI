/**
 * @file BattlePresenter.h
 * @brief Mediator between the GameManager model and an IBattleView.
 */
#pragma once

#include "../core/GameManager.h"
#include "../views/IBattleView.h"
#include <optional>
#include <SFML/System/Vector2.hpp>
#include <unordered_map>
#include <unordered_set>

namespace presenters {

/**
 * @brief Translates UI events into model commands and pushes render
 *        snapshots back to the view.
 *
 * Holds no game state of its own; instead it queries GameManager for
 * legal moves/attacks, picks the right approach hex from the cursor
 * position, dispatches the command, and queues the matching visual
 * event sequence on the IBattleView.
 */
class BattlePresenter {
public:
    BattlePresenter( core::GameManager& model, views::IBattleView& view );

    void startBattle( );
    void onHexClicked( int q, int r, bool shift_held = false );
    void onMouseHover( int pixel_x, int pixel_y, bool shift_held );
    void onRightClickPressed( int pixel_x, int pixel_y );
    void onRightClickReleased( );
    void onDefendClicked( );
    void onWaitClicked( );

private:
    void refreshUiForActiveUnit( );
    void pushRenderDataToView( );
    std::vector<views::UnitRenderData> buildRenderDataSnapshot( ) const;
    std::pair<int, int> pixelToHex( float x, float y ) const;
    views::CursorStyle directionToCursor( float angle_deg ) const;
    void showUnitRangePreview( const models::Unit& unit );
    static std::uint64_t makeUnitId( const models::Unit* unit );
    static std::optional<views::UnitRenderData>
    findUnit( const std::vector<views::UnitRenderData>& units, std::uint64_t id );
    void queueMoveVisualIfNeeded( std::uint64_t unit_id,
                                      const std::vector<views::UnitRenderData>& before,
                                      const std::vector<views::UnitRenderData>& after );

    void queueMoveVisualAlongPath( std::uint64_t unit_id,
                                       const std::vector<std::pair<int, int>>& precomputed_path );

    void finalizeActionVisuals( std::uint64_t actor_id, bool had_morale_bonus );
    std::vector<views::IBattleView::AttackOriginHex> buildAttackOriginsForTarget(
        const models::Unit& attacker,
        const models::Unit& target,
        const std::vector<models::Hex*>& destinations,
        const std::vector<views::IBattleView::PredictedFacing>& predictions ) const;
    static std::vector<views::IBattleView::AttackOriginHex>
    dedupeAttackOrigins( const std::vector<views::IBattleView::AttackOriginHex>& origins );
    const std::vector<views::IBattleView::AttackOriginHex>*
    getCachedAttackOriginsForTarget( const models::Unit& target ) const;
    const models::Hex*
    resolveMoveHeadDestination( const models::Unit& unit,
                                   const models::Hex& clicked_or_hovered_hex ) const;
    void highlightUnitBody( const models::Unit& unit, views::HighlightType type ) const;

    models::Hex* findAttackApproach( const models::Unit& attacker,
                                       const models::Hex& target_hex,
                                       float pixel_x,
                                       float pixel_y ) const;

    // Single source of truth for "where will the attack land from?" used by
    // both hover (cursor + highlight preview) and click (model.attack) so the
    // two never disagree.  Builds the attack-origin set (cached per-target +
    // self-origin when the attacker is already adjacent) and snaps to the
    // origin whose strike side best matches the cursor's compass angle from
    // the defender.  Out-params expose the picked origin metadata so callers
    // can render highlights without rerunning the snap.
    /**
     * @brief Captures the chosen melee approach hex and cached metadata.
     */
    struct PickedApproach {
        models::Hex* approach_ = nullptr;
        views::IBattleView::AttackOriginHex origin_{ };
        std::vector<views::IBattleView::AttackOriginHex> allOrigins_;
        bool directlyAdjacent_ = false;
    };
    PickedApproach pickAttackApproachForCursor( const models::Unit& attacker,
                                                    const models::Hex& hovered_hex,
                                                    float pixel_x,
                                                    float pixel_y ) const;

    sf::Vector2f hexToPixel( int q, int r ) const;

    core::GameManager& model_;
    views::IBattleView& view_;
    bool rangePreviewActive_ = false;
    bool infoPanelVisible_ = false;

    int lastCursorPx_ = 0;
    int lastCursorPy_ = 0;
    std::unordered_map<std::uint64_t, std::vector<views::IBattleView::AttackOriginHex>>
        cachedAttackOriginsByTarget_;

    std::vector<models::Hex*> cachedDestinations_;
    std::unordered_set<std::int64_t> cachedDestinationsSet_;
    bool isDestinationCached( int q, int r ) const;
};

} // namespace presenters