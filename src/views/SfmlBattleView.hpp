#pragma once

#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>

#include <filesystem>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "IBattleView.hpp"
#include "AnimationController.hpp"
#include "DefManager.hpp"

class BattlePresenter;

class SfmlBattleView : public IBattleView {
public:
    SfmlBattleView(unsigned int width, unsigned int height, const std::string& title);

    bool is_open() const;
    void on_mouse_hover(int pixel_x, int pixel_y, BattlePresenter& presenter);
    void process_events(BattlePresenter& presenter);
    void render();

    void clear_all_highlights() override;
    void highlight_hex(int q, int r, HighlightType type) override;
    void update_hud(const std::string& unit_name, int count, int hp_left) override;
    void update_turn_order(const std::vector<TurnQueueSlot>& slots) override;
    void show_message(const std::string& msg) override;
    void set_active_unit_highlight(int q, int r, int size, bool is_facing_left) override;
    void clear_active_unit_highlight() override;
    void set_hover_destination_highlight(int q, int r, bool has_tail, int tail_q, int tail_r) override;
    void clear_hover_destination_highlight() override;
    void set_attack_origin_highlights(const std::vector<AttackOriginHex>& origins) override;
    void clear_attack_origin_highlights() override;
    void set_shift_preview_active(bool active) override;
    void set_predicted_facings(const std::vector<PredictedFacing>& predictions) override;
    void sync_unit_positions() override;
    void update_render_data(const std::vector<UnitRenderData>& units) override;
    void queue_move_animation(std::uint64_t unit_id,
                              int from_q,
                              int from_r,
                              int to_q,
                              int to_r,
                              float duration_seconds) override;
    void queue_attack_animation(std::uint64_t attacker_id, float duration_seconds) override;
    void queue_attack_animation_facing(std::uint64_t attacker_id, int target_q, int target_r) override;
    void queue_projectile_animation(std::uint64_t attacker_id, int target_q, int target_r,
                                    const std::string& projectile_asset,
                                    float duration_seconds) override;
    void queue_morale_animation(std::uint64_t unit_id) override;
    void queue_hit_animation(std::uint64_t defender_id) override;
    void queue_death_animation(std::uint64_t unit_id) override;
    void queue_render_data_commit(const std::vector<UnitRenderData>& units) override;
    void clear_visual_events() override;
    bool has_pending_visual_events() const override;
    void set_idle_callback(std::function<void()> cb) override;
    void set_cursor_style(CursorStyle style, int pixel_x, int pixel_y) override;
    void show_unit_info_panel(const UnitRenderData& unit_data) override;
    void hide_unit_info_panel() override;

private:

    void draw_battlefield_background();
    void draw_hex_grid();
    void draw_units();
    void draw_unit_stack_ui();
    void draw_hud();
    void draw_turn_queue();
    void draw_info_panel();
    void draw_cursor();

    sf::Vector2f hex_to_pixel(int q, int r) const;
    std::pair<int, int> pixel_to_hex(float x, float y) const;
    sf::ConvexShape make_hex_shape(int q, int r) const;

    sf::Vector2f unit_render_center(const UnitRenderData& unit) const;
    sf::Vector2f unit_render_center(const UnitRenderData& unit, int q, int r) const;

    static std::int64_t make_hex_key(int q, int r);
    void update_hover_from_mouse();
    void update_visual_events(sf::Time dt);
    void process_visual_event_start();
    void process_visual_event_finish();
    const UnitRenderData* find_unit_render_data(std::uint64_t id) const;
    void apply_current_render_data_to_controllers(bool reset_standing_anim);
    void refresh_expanded_highlights();
    void handle_corpse_state_transition(const UnitRenderData& unit, AnimationController& controller);

    bool is_point_in_battlefield(float x, float y) const;

    struct MoveVisualEvent {
        enum class Phase {
            Slide,
            TeleportFadeOut,
            TeleportHold,
            TeleportFadeIn,
        } phase = Phase::Slide;

        std::uint64_t unit_id = 0;
        sf::Vector2f from = {0.0f, 0.0f};
        sf::Vector2f to = {0.0f, 0.0f};
        float duration_seconds = 0.5f;
        float elapsed_seconds = 0.0f;
        bool is_teleporter = false;
    };

    struct AttackVisualEvent {
        std::uint64_t attacker_id = 0;

        bool has_target_hex = false;
        int  target_q = 0;
        int  target_r = 0;
        float safety_timeout = 5.0f;   
        float elapsed_seconds = 0.0f;
    };

    struct HitVisualEvent {
        std::uint64_t defender_id = 0;
        float safety_timeout = 5.0f;
        float elapsed_seconds = 0.0f;
    };

    struct DeathVisualEvent {
        std::uint64_t unit_id = 0;
        float safety_timeout = 8.0f;
        float elapsed_seconds = 0.0f;
    };

    struct ProjectileVisualEvent {
        std::uint64_t attacker_id = 0;
        sf::Vector2f from = {0.0f, 0.0f};
        sf::Vector2f to   = {0.0f, 0.0f};
        std::string  projectile_asset;
        float duration_seconds = 0.4f;
        float elapsed_seconds  = 0.0f;
    };

    struct MoraleVisualEvent {
        std::uint64_t unit_id = 0;
        float duration_seconds = 0.85f;
        float elapsed_seconds  = 0.0f;
    };

    struct CommitRenderDataVisualEvent {
        std::vector<UnitRenderData> units;
    };

    struct VisualEvent {
        enum class Type {
            Move,
            Attack,
            Projectile,
            Morale,
            Hit,
            Death,
            CommitRenderData,
        } type = Type::Move;

        bool started = false;
        MoveVisualEvent move;
        AttackVisualEvent attack;
        ProjectileVisualEvent projectile;
        MoraleVisualEvent morale;
        HitVisualEvent hit;
        DeathVisualEvent death;
        CommitRenderDataVisualEvent commit;
    };

    struct ActiveUnitHighlight {
        int q = 0;
        int r = 0;
        int size = 1;
        bool is_facing_left = false;
    };

    struct HoverDestinationHighlight {
        int q = 0;
        int r = 0;
        bool has_tail = false;
        int tail_q = 0;
        int tail_r = 0;
    };

    sf::RenderWindow window;

    float screen_width;
    float screen_height;
    float battlefield_height;
    float hex_radius;
    sf::Vector2f grid_origin;

    std::string hud_unit_name;
    int hud_count;
    int hud_hp_left;
    std::string latest_message;

    std::vector<UnitRenderData> units_to_draw;
    std::vector<UnitRenderData> model_units_latest;
    std::unordered_map<std::uint64_t, sf::Vector2f> visual_position_overrides;
    std::unordered_set<std::uint64_t> corpse_frozen_ids;
    std::unordered_map<std::int64_t, HighlightType> expanded_highlights;
    std::vector<TurnQueueSlot> turn_queue_slots;

    DefManager def_manager;
    std::unordered_map<std::uint64_t, AnimationController> animation_controllers;
    std::unordered_map<std::uint64_t, std::string> controller_asset_files;
    std::deque<VisualEvent> visual_events;
    sf::Clock animation_clock;
    std::function<void()> idle_callback;

    float pulse_phase_seconds = 0.0f;

    sf::Texture battlefield_texture;
    std::unique_ptr<sf::Sprite> battlefield_sprite;

    sf::Texture unit_stack_box_texture;
    std::unique_ptr<sf::Sprite> unit_stack_box_sprite;
    std::unique_ptr<sf::Text> unit_stack_count_text;
    sf::RectangleShape unit_stack_team_backer;
    sf::RectangleShape unit_stack_hp_back;
    sf::RectangleShape unit_stack_hp_fill;

    std::unordered_map<std::int64_t, HighlightType> highlights;
    std::optional<ActiveUnitHighlight> active_unit_highlight;
    std::optional<HoverDestinationHighlight> hover_destination_highlight;
    std::vector<AttackOriginHex> attack_origin_highlights;
    bool shift_preview_active = false;

    std::unordered_map<std::int64_t, bool> predicted_facing_by_hex;

    enum class ActionKind { Spellbook, Wait, Defend, AutoCombat, Surrender };
    struct ActionSlot {
        ActionKind kind = ActionKind::Wait;
        std::string def_filename;
        sf::FloatRect bounds;

        float pressed_seconds_left = 0.0f;
    };
    std::vector<ActionSlot> action_slots;

    sf::Font font;
    std::unique_ptr<sf::Text> hud_text;
    std::unique_ptr<sf::Text> queue_text;
    std::unique_ptr<sf::Text> log_text;

    CursorStyle cursor_style = CursorStyle::Default;
    sf::Vector2f cursor_position = {0.0f, 0.0f};
    bool os_cursor_visible = true;

    bool info_panel_visible = false;
    std::optional<UnitRenderData> info_panel_unit;
    sf::Texture info_panel_texture;
    std::unique_ptr<sf::Sprite>   info_panel_sprite;
    std::unique_ptr<sf::Text>     info_panel_text;

    void load_action_bar_assets();
    void load_info_panel_assets();
    void load_unit_stack_assets();
    bool route_action_click(float x, float y, BattlePresenter& presenter);
    void draw_action_bar();
};