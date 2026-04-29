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
    HoverDestination   
};

enum class CursorStyle {
    Default        = -1,   
    NotAvailable   = 0,    
    NormalMove     = 1,    
    FlyMove        = 2,    
    RangeShoot     = 3,    
    Skip           = 4,    
    QuestionMark   = 5,    
    StandardPointer= 6,    
    SwordNE        = 7,
    SwordE         = 8,
    SwordSE        = 9,
    SwordSW        = 10,
    SwordW         = 11,
    SwordNW        = 12,
    SwordN         = 13,
    SwordS         = 14,
    BrokenArrow    = 15,   
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

class IBattleView {
public:
    virtual ~IBattleView() = default;

    virtual void clear_all_highlights() = 0;
    virtual void highlight_hex(int q, int r, HighlightType type) = 0;
    virtual void update_hud(const std::string& unit_name, int count, int hp_left) = 0;

    struct TurnQueueSlot {
        bool is_divider = false;
        int  round_number = 0;        
        std::string unit_name;        
        bool is_active = false;       
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

    virtual void set_shift_preview_active(bool active) = 0;

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

    virtual void queue_attack_animation_facing(std::uint64_t attacker_id,
                                               int target_q, int target_r) = 0;

    virtual void queue_projectile_animation(std::uint64_t attacker_id,
                                            int target_q, int target_r,
                                            const std::string& projectile_asset,
                                            float duration_seconds) = 0;

    virtual void queue_morale_animation(std::uint64_t unit_id) = 0;

    virtual void queue_hit_animation(std::uint64_t defender_id) = 0;

    virtual void queue_death_animation(std::uint64_t unit_id) = 0;
    virtual void queue_render_data_commit(const std::vector<UnitRenderData>& units) = 0;
    virtual void clear_visual_events() = 0;
    virtual bool has_pending_visual_events() const = 0;

    virtual void set_idle_callback(std::function<void()> cb) = 0;
    virtual void set_cursor_style(CursorStyle style, int pixel_x, int pixel_y) = 0;
    virtual void show_unit_info_panel(const UnitRenderData& unit_data) = 0;
    virtual void hide_unit_info_panel() = 0;
};