/**
 * @file BattlePresenter.h
 * @brief Mediator between the GameManager model and an IBattleView.
 * @author Dominik Śledziewski & Łukasz Szydlik
 */
#pragma once

#include <optional>
#include <SFML/System/Vector2.hpp>
#include <unordered_map>
#include <unordered_set>

#include "../core/ActionCommand.h"
#include "../core/ActionGenerator.h"
#include "../core/GameManager.h"
#include "../core/RandomBotService.h"
#include "../core/SpellResolver.h"
#include "../models/Spell.h"
#include "../views/IBattleView.h"

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
    BattlePresenter( core::GameManager& model,
                          views::IBattleView& view,
                          bool blue_is_bot = false,
                          bool red_is_bot = false );

    /**
     * @brief Initialises the view with the starting board state and highlights.
     */
    void startBattle( );

    /**
     * @brief Per-frame hook driving the AI.
     *
     * Called once each frame by BattleScene. When the battlefield is idle
     * (no pending animations, not mid spell-targeting) and the active
     * unit is bot-controlled, asks the bot for one action and executes it.
     */
    void update( );

    /**
     * @brief Handles a player click on a battlefield hex.
     *
     * Routes the click to move, melee attack, or ranged attack depending on
     * what occupies the clicked hex and the current unit's capabilities.
     * @param q          Axial column coordinate of the clicked hex.
     * @param r          Axial row coordinate of the clicked hex.
     * @param shift_held True when the player holds Shift (unit range preview mode).
     */
    void onHexClicked( int q, int r, bool shift_held = false );

    /**
     * @brief Updates cursor style, destination highlight and attack-origin hints on hover.
     * @param pixel_x    Horizontal cursor position in window pixels.
     * @param pixel_y    Vertical cursor position in window pixels.
     * @param shift_held True when the player holds Shift (unit info preview mode).
     */
    void onMouseHover( int pixel_x, int pixel_y, bool shift_held );

    /**
     * @brief Shows the unit info panel for the unit occupying the right-clicked hex.
     * @param pixel_x Horizontal cursor position in window pixels.
     * @param pixel_y Vertical cursor position in window pixels.
     */
    void onRightClickPressed( int pixel_x, int pixel_y );

    void onRightClickReleased( );
    void onDefendClicked( );
    void onWaitClicked( );

    void onSpellbookClicked( );
    void onSpellChosen( models::SpellId id );
    void onSpellbookCancelled( );

private:
    void cancelSpellTargeting( );
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

    void handleSpellTargetClick( int q, int r );

    // Cursor-independent action execution shared by player clicks and the
    // bot. Each applies the model mutation and queues the matching visual
    // event sequence, then advances the turn.
    void executeMove( models::Unit& unit, models::Hex& move_head );
    void executeMeleeAttack( models::Unit& attacker,
                                  models::Unit& target,
                                  models::Hex& approach );
    void executeRangedAttack( models::Unit& attacker, models::Unit& target );
    void executeCastSpell( models::SpellId id, models::Unit& target );
    void executeWait( );
    void executeDefend( );

    // AI driving.
    bool isUnitBotControlled( const models::Unit& unit ) const;
    bool isActiveUnitBotControlled( ) const;
    void runBotTurn( );
    void executeCommand( const core::ActionCommand& command );

    core::GameManager& model_;
    views::IBattleView& view_;
    core::SpellResolver spellResolver_;
    core::ActionGenerator actionGenerator_;
    core::RandomBotService randomBot_;
    bool blueIsBot_ = false;
    bool redIsBot_ = false;
    bool rangePreviewActive_ = false;
    bool infoPanelVisible_ = false;
    bool isCastingSpell_ = false;
    models::SpellId pendingSpellId_ = models::SpellId::MAGIC_ARROW;

    int lastCursorPx_ = 0;
    int lastCursorPy_ = 0;
    std::unordered_map<std::uint64_t, std::vector<views::IBattleView::AttackOriginHex>>
        cachedAttackOriginsByTarget_;

    std::vector<models::Hex*> cachedDestinations_;
    std::unordered_set<std::int64_t> cachedDestinationsSet_;
    bool isDestinationCached( int q, int r ) const;
};

} // namespace presenters
