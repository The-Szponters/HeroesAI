/**
 * @file IBattleView.h
 * @brief Abstract view interface used by the battle presenter.
 *
 * Defines the data and animation hooks required to render and animate
 * a battle, decoupling presenter logic from any concrete renderer.
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
    std::uint64_t id = 0;
    int q = 0;
    int r = 0;
    std::string name;
    std::string asset_filename;
    std::string description;
    int count = 0;
    int hp_left = 0;
    int max_hp_per_unit = 0;
    int current_top_unit_hp = 0;
    int owner_id = -1;
    int base_attack = 0;
    int total_attack = 0;
    int base_defense = 0;
    int total_defense = 0;
    int base_speed = 0;
    int total_speed = 0;
    int base_damage_min = 0;
    int total_damage_min = 0;
    int base_damage_max = 0;
    int total_damage_max = 0;
    bool is_facing_left = false;
    bool visual_facing_left = false;
    bool is_ranged = false;
    int ammo = 0;
    int max_ammo = 0;
    bool is_corpse = false;
    int size = 1;
    bool is_teleporter = false;
    bool is_flying = false;
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

    virtual void clear_all_highlights( ) = 0;
    virtual void highlight_hex( int q, int r, HighlightType type ) = 0;
    virtual void update_hud( const std::string& unit_name, int count, int hp_left ) = 0;

    /**
     * @brief Render info for a single item in the turn queue UI.
     */
    struct TurnQueueSlot {
        bool is_divider = false;
        int round_number = 0;
        std::string unit_name;
        bool is_active = false;
    };
    virtual void update_turn_order( const std::vector<TurnQueueSlot>& slots ) = 0;
    virtual void show_message( const std::string& msg ) = 0;
    virtual void set_active_unit_highlight( int q, int r, int size, bool is_facing_left ) = 0;
    virtual void clear_active_unit_highlight( ) = 0;
    virtual void
    set_hover_destination_highlight( int q, int r, bool has_tail, int tail_q, int tail_r ) = 0;
    virtual void clear_hover_destination_highlight( ) = 0;
    /**
     * @brief Attack origin hex with optional tail location.
     */
    struct AttackOriginHex {
        int q = 0;
        int r = 0;
        bool has_tail = false;
        int tail_q = 0;
        int tail_r = 0;
    };
    virtual void set_attack_origin_highlights( const std::vector<AttackOriginHex>& origins ) = 0;
    virtual void clear_attack_origin_highlights( ) = 0;

    virtual void set_shift_preview_active( bool active ) = 0;

    /**
     * @brief Predicted facing for a unit standing on a hex.
     */
    struct PredictedFacing {
        int q = 0;
        int r = 0;
        bool facing_left = false;
    };
    virtual void set_predicted_facings( const std::vector<PredictedFacing>& predictions ) = 0;
    virtual void sync_unit_positions( ) = 0;
    virtual void update_render_data( const std::vector<UnitRenderData>& units ) = 0;
    virtual void queue_move_animation( std::uint64_t unit_id,
                                       int from_q,
                                       int from_r,
                                       int to_q,
                                       int to_r,
                                       float duration_seconds ) = 0;
    virtual void queue_attack_animation( std::uint64_t attacker_id, float duration_seconds ) = 0;

    virtual void
    queue_attack_animation_facing( std::uint64_t attacker_id, int target_q, int target_r ) = 0;

    virtual void queue_projectile_animation( std::uint64_t attacker_id,
                                             int target_q,
                                             int target_r,
                                             const std::string& projectile_asset,
                                             float duration_seconds ) = 0;

    virtual void queue_morale_animation( std::uint64_t unit_id ) = 0;

    virtual void queue_hit_animation( std::uint64_t defender_id ) = 0;

    virtual void queue_death_animation( std::uint64_t unit_id ) = 0;
    virtual void queue_render_data_commit( const std::vector<UnitRenderData>& units ) = 0;
    virtual void clear_visual_events( ) = 0;
    virtual bool has_pending_visual_events( ) const = 0;

    virtual void set_idle_callback( std::function<void( )> cb ) = 0;
    virtual void set_cursor_style( CursorStyle style, int pixel_x, int pixel_y ) = 0;
    virtual void show_unit_info_panel( const UnitRenderData& unit_data ) = 0;
    virtual void hide_unit_info_panel( ) = 0;
};

} // namespace views