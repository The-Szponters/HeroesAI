/**
 * @file SfmlBattleView.h
 * @brief Concrete SFML-backed battle view.
 * @author Dominik Śledziewski & Łukasz Szydlik
 */
#pragma once

#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>

#include <deque>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "AnimationController.h"
#include "DefManager.h"
#include "IBattleView.h"

namespace presenters {
class BattlePresenter;
}

namespace views {

/**
 * @brief SFML-based renderer for the battle view.
 *
 * Owns a single window, sprite caches, animation controllers and a
 * queue of pending visual events (movement slides, attack frames,
 * projectiles, deaths) that are advanced from the main render loop.
 */
class SfmlBattleView : public IBattleView {
public:
    explicit SfmlBattleView( sf::RenderWindow& window );

    bool isOpen( ) const;
    void onMouseHover( int pixel_x, int pixel_y, presenters::BattlePresenter& presenter );
    void processEvents( presenters::BattlePresenter& presenter );
    void render( );

    void clearAllHighlights( ) override;
    void highlightHex( int q, int r, HighlightType type ) override;
    void updateHud( const std::string& unit_name, int count, int hp_left ) override;
    void updateTurnOrder( const std::vector<TurnQueueSlot>& slots ) override;
    void showMessage( const std::string& msg ) override;
    void setActiveUnitHighlight( int q, int r, int size, bool is_facing_left ) override;
    void clearActiveUnitHighlight( ) override;
    void
    setHoverDestinationHighlight( int q, int r, bool has_tail, int tail_q, int tail_r ) override;
    void clearHoverDestinationHighlight( ) override;
    void setAttackOriginHighlights( const std::vector<AttackOriginHex>& origins ) override;
    void clearAttackOriginHighlights( ) override;
    void setShiftPreviewActive( bool active ) override;
    void setPredictedFacings( const std::vector<PredictedFacing>& predictions ) override;
    void syncUnitPositions( ) override;
    void updateRenderData( const std::vector<UnitRenderData>& units ) override;
    void queueMoveAnimation( std::uint64_t unit_id,
                               int from_q,
                               int from_r,
                               int to_q,
                               int to_r,
                               float duration_seconds ) override;
    void queueAttackAnimation( std::uint64_t attacker_id, float duration_seconds ) override;
    void
    queueAttackAnimationFacing( std::uint64_t attacker_id, int target_q, int target_r ) override;
    void queueProjectileAnimation( std::uint64_t attacker_id,
                                     int target_q,
                                     int target_r,
                                     const std::string& projectile_asset,
                                     float duration_seconds ) override;
    void queueMoraleAnimation( std::uint64_t unit_id ) override;
    void queueHitAnimation( std::uint64_t defender_id ) override;
    void queueDeathAnimation( std::uint64_t unit_id ) override;
    void queueRenderDataCommit( const std::vector<UnitRenderData>& units ) override;
    void clearVisualEvents( ) override;
    bool hasPendingVisualEvents( ) const override;
    void setIdleCallback( std::function<void( )> cb ) override;
    void setCursorStyle( CursorStyle style, int pixel_x, int pixel_y ) override;
    void showUnitInfoPanel( const UnitRenderData& unit_data ) override;
    void hideUnitInfoPanel( ) override;

private:
    void drawBattlefieldBackground( );
    void drawHexGrid( );
    void drawUnits( );
    void drawUnitStackUi( );
    void drawHud( );
    void drawTurnQueue( );
    void drawInfoPanel( );
    void drawCursor( );

    sf::Vector2f hexToPixel( int q, int r ) const;
    std::pair<int, int> pixelToHex( float x, float y ) const;
    sf::ConvexShape makeHexShape( int q, int r ) const;

    sf::Vector2f unitRenderCenter( const UnitRenderData& unit ) const;
    sf::Vector2f unitRenderCenter( const UnitRenderData& unit, int q, int r ) const;

    static std::int64_t makeHexKey( int q, int r );
    void updateHoverFromMouse( );
    void updateVisualEvents( sf::Time dt );
    void processVisualEventStart( );
    void processVisualEventFinish( );
    const UnitRenderData* findUnitRenderData( std::uint64_t id ) const;
    void applyCurrentRenderDataToControllers( bool reset_standing_anim );
    void refreshExpandedHighlights( );
    void handleCorpseStateTransition( const UnitRenderData& unit,
                                         AnimationController& controller );

    bool isPointInBattlefield( float x, float y ) const;

    /**
     * @brief Runtime state for a queued movement animation.
     */
    struct MoveVisualEvent {
        enum class Phase {
            SLIDE,
            TELEPORT_FADE_OUT,
            TELEPORT_HOLD,
            TELEPORT_FADE_IN
        } phase_ = Phase::SLIDE;

        std::uint64_t unitId_ = 0;
        sf::Vector2f from_ = { 0.0f, 0.0f };
        sf::Vector2f to_ = { 0.0f, 0.0f };
        float durationSeconds_ = 0.5f;
        float elapsedSeconds_ = 0.0f;
        bool isTeleporter_ = false;
    };

    /**
     * @brief Runtime state for a queued melee/ranged attack animation.
     */
    struct AttackVisualEvent {
        std::uint64_t attackerId_ = 0;

        bool hasTargetHex_ = false;
        int targetQ_ = 0;
        int targetR_ = 0;
        float safetyTimeout_ = 5.0f;
        float elapsedSeconds_ = 0.0f;
    };

    /**
     * @brief Runtime state for a queued hit animation.
     */
    struct HitVisualEvent {
        std::uint64_t defenderId_ = 0;
        float safetyTimeout_ = 5.0f;
        float elapsedSeconds_ = 0.0f;
    };

    /**
     * @brief Runtime state for a queued death animation.
     */
    struct DeathVisualEvent {
        std::uint64_t unitId_ = 0;
        float safetyTimeout_ = 8.0f;
        float elapsedSeconds_ = 0.0f;
    };

    /**
     * @brief Runtime state for a queued projectile animation.
     */
    struct ProjectileVisualEvent {
        std::uint64_t attackerId_ = 0;
        sf::Vector2f from_ = { 0.0f, 0.0f };
        sf::Vector2f to_ = { 0.0f, 0.0f };
        std::string projectileAsset_;
        float durationSeconds_ = 0.4f;
        float elapsedSeconds_ = 0.0f;
    };

    /**
     * @brief Runtime state for a queued morale animation.
     */
    struct MoraleVisualEvent {
        std::uint64_t unitId_ = 0;
        float durationSeconds_ = 0.85f;
        float elapsedSeconds_ = 0.0f;
    };

    /**
     * @brief Runtime state for applying a new render snapshot.
     */
    struct CommitRenderDataVisualEvent {
        std::vector<UnitRenderData> units_;
    };

    /**
     * @brief Union of queued visual event types.
     */
    struct VisualEvent {
        enum class Type {
            MOVE,
            ATTACK,
            PROJECTILE,
            MORALE,
            HIT,
            DEATH,
            COMMIT_RENDER_DATA
        } type_ = Type::MOVE;

        bool started_ = false;
        MoveVisualEvent move_;
        AttackVisualEvent attack_;
        ProjectileVisualEvent projectile_;
        MoraleVisualEvent morale_;
        HitVisualEvent hit_;
        DeathVisualEvent death_;
        CommitRenderDataVisualEvent commit_;
    };

    /**
     * @brief Cached active-unit highlight geometry.
     */
    struct ActiveUnitHighlight {
        int q_ = 0;
        int r_ = 0;
        int size_ = 1;
        bool isFacingLeft_ = false;
    };

    /**
     * @brief Cached hover destination highlight geometry.
     */
    struct HoverDestinationHighlight {
        int q_ = 0;
        int r_ = 0;
        bool hasTail_ = false;
        int tailQ_ = 0;
        int tailR_ = 0;
    };

    sf::RenderWindow& window_;

    float screenWidth_;
    float screenHeight_;
    float battlefieldHeight_;
    float hexRadius_;
    sf::Vector2f gridOrigin_;

    std::string hudUnitName_;
    int hudCount_;
    int hudHpLeft_;
    std::string latestMessage_;

    std::vector<UnitRenderData> unitsToDraw_;
    std::vector<UnitRenderData> modelUnitsLatest_;
    std::unordered_map<std::uint64_t, sf::Vector2f> visualPositionOverrides_;
    std::unordered_set<std::uint64_t> corpseFrozenIds_;
    std::unordered_map<std::int64_t, HighlightType> expandedHighlights_;
    std::vector<TurnQueueSlot> turnQueueSlots_;

    DefManager defManager_;
    std::unordered_map<std::uint64_t, AnimationController> animationControllers_;
    std::unordered_map<std::uint64_t, std::string> controllerAssetFiles_;
    std::deque<VisualEvent> visualEvents_;
    sf::Clock animationClock_;
    std::function<void( )> idleCallback_;

    float pulsePhaseSeconds_ = 0.0f;

    sf::Texture battlefieldTexture_;
    std::unique_ptr<sf::Sprite> battlefieldSprite_;

    sf::Texture unitStackBoxTexture_;
    std::unique_ptr<sf::Sprite> unitStackBoxSprite_;
    std::unique_ptr<sf::Text> unitStackCountText_;
    sf::RectangleShape unitStackTeamBacker_;
    sf::RectangleShape unitStackHpBack_;
    sf::RectangleShape unitStackHpFill_;

    std::unordered_map<std::int64_t, HighlightType> highlights_;
    std::optional<ActiveUnitHighlight> activeUnitHighlight_;
    std::optional<HoverDestinationHighlight> hoverDestinationHighlight_;
    std::vector<AttackOriginHex> attackOriginHighlights_;
    bool shiftPreviewActive_ = false;

    std::unordered_map<std::int64_t, bool> predictedFacingByHex_;

    enum class ActionKind { SPELLBOOK, WAIT, DEFEND, AUTO_COMBAT, SURRENDER };

    /**
     * @brief Data for an action bar button and its press state.
     */
    struct ActionSlot {
        ActionKind kind_ = ActionKind::WAIT;
        std::string defFilename_;
        sf::FloatRect bounds_;

        float pressedSecondsLeft_ = 0.0f;
    };
    std::vector<ActionSlot> actionSlots_;

    sf::Font font_;
    std::unique_ptr<sf::Text> hudText_;
    std::unique_ptr<sf::Text> queueText_;
    std::unique_ptr<sf::Text> logText_;

    CursorStyle cursorStyle_ = CursorStyle::DEFAULT;
    sf::Vector2f cursorPosition_ = { 0.0f, 0.0f };
    bool osCursorVisible_ = true;

    bool infoPanelVisible_ = false;
    std::optional<UnitRenderData> infoPanelUnit_;
    sf::Texture infoPanelTexture_;
    std::unique_ptr<sf::Sprite> infoPanelSprite_;
    std::unique_ptr<sf::Text> infoPanelText_;

    void loadActionBarAssets( );
    void loadInfoPanelAssets( );
    void loadUnitStackAssets( );
    bool routeActionClick( float x, float y, presenters::BattlePresenter& presenter );
    void drawActionBar( );
};

} // namespace views