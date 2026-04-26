#pragma once

#include <string>
#include <vector>
#include <cstdint>

enum class AnimState : int;

enum class HighlightType {
    None,
    ActiveUnit,
    Walkable,
    Attackable,
    HoverDestination   // dark grey/black tint shown on hex(es) the unit will occupy
};

enum class CursorStyle {
    Default,
    SwordE,
    SwordNE,
    SwordNW,
    SwordW,
    SwordSW,
    SwordSE
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
    // Plays the TakeDamage (flinch) animation on defender; resolves when the animation finishes.
    virtual void queue_hit_animation(std::uint64_t defender_id) = 0;
    // Waits for the Death animation on a unit that was already committed as a corpse.
    virtual void queue_death_animation(std::uint64_t unit_id) = 0;
    virtual void queue_render_data_commit(const std::vector<UnitRenderData>& units) = 0;
    virtual void clear_visual_events() = 0;
    virtual bool has_pending_visual_events() const = 0;
    virtual void set_cursor_style(CursorStyle style, int pixel_x, int pixel_y) = 0;
    virtual void show_unit_info_panel(const UnitRenderData& unit_data) = 0;
    virtual void hide_unit_info_panel() = 0;
};