/**
 * @file IBattleView.h
 * @brief Abstract view interface used by the battle presenter.
 *
 * Defines the data and animation hooks required to render and animate
 * a battle, decoupling presenter logic from any concrete renderer.
 * @author Łukasz Szydlik
 */
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace views {

/**
 * @brief Animation state enumeration forward declaration.
 */
enum class AnimState : int;

enum class HighlightType {
    NONE,
    ACTIVE_UNIT,
    WALKABLE,
    ATTACKABLE,
    ATTACK_ORIGIN,
    HOVER_DESTINATION
};

enum class CursorStyle {
    DEFAULT = -1,
    NOT_AVAILABLE = 0,
    NORMAL_MOVE = 1,
    FLY_MOVE = 2,
    RANGE_SHOOT = 3,
    SKIP = 4,
    QUESTION_MARK = 5,
    STANDARD_POINTER = 6,
    SWORD_NE = 7,
    SWORD_E = 8,
    SWORD_SE = 9,
    SWORD_SW = 10,
    SWORD_W = 11,
    SWORD_NW = 12,
    SWORD_N = 13,
    SWORD_S = 14,
    BROKEN_ARROW = 15
};

/**
 * @brief Snapshot of a unit used for rendering and UI panels.
 */
struct UnitRenderData {
    std::uint64_t id_ = 0;
    int q_ = 0;
    int r_ = 0;
    std::string name_;
    std::string assetFilename_;
    std::string description_;
    int count_ = 0;
    int hpLeft_ = 0;
    int maxHpPerUnit_ = 0;
    int currentTopUnitHp_ = 0;
    int ownerId_ = -1;
    int baseAttack_ = 0;
    int totalAttack_ = 0;
    int baseDefense_ = 0;
    int totalDefense_ = 0;
    int baseSpeed_ = 0;
    int totalSpeed_ = 0;
    int baseDamageMin_ = 0;
    int totalDamageMin_ = 0;
    int baseDamageMax_ = 0;
    int totalDamageMax_ = 0;
    bool isFacingLeft_ = false;
    bool visualFacingLeft_ = false;
    bool isRanged_ = false;
    int ammo_ = 0;
    int maxAmmo_ = 0;
    bool isCorpse_ = false;
    int size_ = 1;
    bool isTeleporter_ = false;
    bool isFlying_ = false;
};

/**
 * @brief Renderer-agnostic interface implemented by every battle view.
 *
 * The presenter owns this interface and never depends on SFML directly,
 * which makes it possible to substitute a headless test view for
 * deterministic unit testing of presenter logic.
 */
class IBattleView {
public:
    virtual ~IBattleView( ) = default;

    virtual void clearAllHighlights( ) = 0;
    virtual void highlightHex( int q, int r, HighlightType type ) = 0;
    virtual void updateHud( const std::string& unit_name, int count, int hp_left ) = 0;

    /**
     * @brief Render info for a single item in the turn queue UI.
     */
    struct TurnQueueSlot {
        bool isDivider_ = false;
        int roundNumber_ = 0;
        std::string unitName_;
        bool isActive_ = false;
    };
    virtual void updateTurnOrder( const std::vector<TurnQueueSlot>& slots ) = 0;
    virtual void showMessage( const std::string& msg ) = 0;
    virtual void setActiveUnitHighlight( int q, int r, int size, bool is_facing_left ) = 0;
    virtual void clearActiveUnitHighlight( ) = 0;
    virtual void
    setHoverDestinationHighlight( int q, int r, bool has_tail, int tail_q, int tail_r ) = 0;
    virtual void clearHoverDestinationHighlight( ) = 0;
    /**
     * @brief Attack origin hex with optional tail location.
     */
    struct AttackOriginHex {
        int q_ = 0;
        int r_ = 0;
        bool hasTail_ = false;
        int tailQ_ = 0;
        int tailR_ = 0;
    };
    virtual void setAttackOriginHighlights( const std::vector<AttackOriginHex>& origins ) = 0;
    virtual void clearAttackOriginHighlights( ) = 0;

    virtual void setShiftPreviewActive( bool active ) = 0;

    /**
     * @brief Predicted facing for a unit standing on a hex.
     */
    struct PredictedFacing {
        int q_ = 0;
        int r_ = 0;
        bool facingLeft_ = false;
    };
    virtual void setPredictedFacings( const std::vector<PredictedFacing>& predictions ) = 0;
    virtual void syncUnitPositions( ) = 0;
    virtual void updateRenderData( const std::vector<UnitRenderData>& units ) = 0;
    virtual void queueMoveAnimation( std::uint64_t unit_id,
                                       int from_q,
                                       int from_r,
                                       int to_q,
                                       int to_r,
                                       float duration_seconds ) = 0;
    virtual void queueAttackAnimation( std::uint64_t attacker_id, float duration_seconds ) = 0;

    virtual void
    queueAttackAnimationFacing( std::uint64_t attacker_id, int target_q, int target_r ) = 0;

    virtual void queueProjectileAnimation( std::uint64_t attacker_id,
                                             int target_q,
                                             int target_r,
                                             const std::string& projectile_asset,
                                             float duration_seconds ) = 0;

    virtual void queueMoraleAnimation( std::uint64_t unit_id ) = 0;

    virtual void queueHitAnimation( std::uint64_t defender_id ) = 0;

    virtual void queueDeathAnimation( std::uint64_t unit_id ) = 0;
    virtual void queueRenderDataCommit( const std::vector<UnitRenderData>& units ) = 0;
    virtual void clearVisualEvents( ) = 0;
    virtual bool hasPendingVisualEvents( ) const = 0;

    virtual void setIdleCallback( std::function<void( )> cb ) = 0;
    virtual void setCursorStyle( CursorStyle style, int pixel_x, int pixel_y ) = 0;
    virtual void showUnitInfoPanel( const UnitRenderData& unit_data ) = 0;
    virtual void hideUnitInfoPanel( ) = 0;
};

} // namespace views