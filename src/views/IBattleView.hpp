#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <functional>

enum class AnimState : int;

enum class HighlightType {
    None,
    ActiveUnit,
    Walkable,
    Attackable,
    AttackOrigin,
    HoverDestination   // dark grey/black tint shown on hex(es) the unit will occupy
};

// Combat cursor states.  Each value (except Default) maps 1:1 to a frame
// index in `assets/ui/combat_icons.def`, so the View can blit the correct
// sprite without any switch logic of its own.  Default keeps the OS cursor.
enum class CursorStyle {
    Default        = -1,   // OS cursor, no DEF frame drawn
    NotAvailable   = 0,    // O-with-slash (cannot interact)
    NormalMove     = 1,    // boot — empty reachable hex
    FlyMove        = 2,    // wings — fliers / teleporters
    RangeShoot     = 3,    // arrow — clear shot
    Skip           = 4,    // hourglass — skip turn
    QuestionMark   = 5,    // ? — info / shift-hover
    StandardPointer= 6,    // plain pointer (over UI)
    SwordNE        = 7,
    SwordE         = 8,
    SwordSE        = 9,
    SwordSW        = 10,
    SwordW         = 11,
    SwordNW        = 12,
    SwordN         = 13,
    SwordS         = 14,
    BrokenArrow    = 15,   // shoot/melee with damage penalty
};

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
    bool is_facing_left = false;        // logical — determines tail hex for render center
    bool visual_facing_left = false;    // sprite mirror — updates with movement direction
    bool is_ranged = false;
    int ammo = 0;
    int max_ammo = 0;
    bool is_corpse = false;
    int size = 1;   // 1 = single-hex, 2 = large creature spanning two hexes
    bool is_teleporter = false;
};

class IBattleView {
public:
    virtual ~IBattleView() = default;

    virtual void clear_all_highlights() = 0;
    virtual void highlight_hex(int q, int r, HighlightType type) = 0;
    virtual void update_hud(const std::string& unit_name, int count, int hp_left) = 0;
    // Slot for the HoMM3-style turn queue UI.  A slot is either a unit (showing
    // its name and whether it's the current actor) or a "Round N" divider that
    // separates the remaining current-round units from the next-round preview.
    struct TurnQueueSlot {
        bool is_divider = false;
        int  round_number = 0;        // valid when is_divider == true
        std::string unit_name;        // valid when is_divider == false
        bool is_active = false;       // current actor (only true for slot[0])
    };
    virtual void update_turn_order(const std::vector<TurnQueueSlot>& slots) = 0;
    virtual void show_message(const std::string& msg) = 0;
    virtual void set_active_unit_highlight(int q, int r, int size, bool is_facing_left) = 0;
    virtual void clear_active_unit_highlight() = 0;
    virtual void set_hover_destination_highlight(int q, int r, bool has_tail, int tail_q, int tail_r) = 0;
    virtual void clear_hover_destination_highlight() = 0;
    struct AttackOriginHex {
        int q = 0;
        int r = 0;
        bool has_tail = false;
        int tail_q = 0;
        int tail_r = 0;
    };
    virtual void set_attack_origin_highlights(const std::vector<AttackOriginHex>& origins) = 0;
    virtual void clear_attack_origin_highlights() = 0;
    // Per-destination predicted facing.  The presenter computes this from the
    // actual reconstructed path (second-to-last hex → final hex direction) and
    // hands the View a lookup table so the per-frame hover code can mirror the
    // 2-hex tail correctly without re-running pathfinding.  Pass an empty
    // vector to clear it.
    struct PredictedFacing { int q = 0; int r = 0; bool facing_left = false; };
    virtual void set_predicted_facings(const std::vector<PredictedFacing>& predictions) = 0;
    virtual void sync_unit_positions() = 0;
    virtual void update_render_data(const std::vector<UnitRenderData>& units) = 0;
    virtual void queue_move_animation(std::uint64_t unit_id,
                                      int from_q,
                                      int from_r,
                                      int to_q,
                                      int to_r,
                                      float duration_seconds) = 0;
    virtual void queue_attack_animation(std::uint64_t attacker_id, float duration_seconds) = 0;
    // Pre-rotates the attacker to face (target_q, target_r) before swinging.
    // Use this overload for attacks and retaliations so the unit visually
    // turns toward its target — including the case where the defender is
    // behind the attacker (e.g. attacker moved past it during a flank).
    virtual void queue_attack_animation_facing(std::uint64_t attacker_id,
                                               int target_q, int target_r) = 0;
    // Flies a projectile sprite (loaded from `projectile_asset`) from the
    // attacker's position to (target_q, target_r) over `duration_seconds`.
    // Empty `projectile_asset` is a no-op shaped like a 0-duration event so
    // visual queues for ranged units without a shipped projectile DEF still
    // resolve cleanly.
    virtual void queue_projectile_animation(std::uint64_t attacker_id,
                                            int target_q, int target_r,
                                            const std::string& projectile_asset,
                                            float duration_seconds) = 0;
    // Plays morale.def's 20-frame aura over the unit's sprite as a one-shot
    // overlay.  Used by the +morale bonus "good morale" trigger.
    virtual void queue_morale_animation(std::uint64_t unit_id) = 0;
    // Plays the TakeDamage (flinch) animation on defender; resolves when the animation finishes.
    virtual void queue_hit_animation(std::uint64_t defender_id) = 0;
    // Waits for the Death animation on a unit that was already committed as a corpse.
    virtual void queue_death_animation(std::uint64_t unit_id) = 0;
    virtual void queue_render_data_commit(const std::vector<UnitRenderData>& units) = 0;
    virtual void clear_visual_events() = 0;
    virtual bool has_pending_visual_events() const = 0;
    // Schedule a callback to fire exactly once, when the visual queue
    // transitions from non-empty to empty.  Used by the presenter to defer
    // the next-turn UI refresh (active-unit highlight, range, attackable
    // tints) until the previous unit's animations have actually finished —
    // otherwise the next unit's overlays appear over the still-animating
    // sprite of the unit that just acted.  Calling again replaces the
    // pending callback; passing `nullptr` cancels.
    virtual void set_idle_callback(std::function<void()> cb) = 0;
    virtual void set_cursor_style(CursorStyle style, int pixel_x, int pixel_y) = 0;
    virtual void show_unit_info_panel(const UnitRenderData& unit_data) = 0;
    virtual void hide_unit_info_panel() = 0;
};