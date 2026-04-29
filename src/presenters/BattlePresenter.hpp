#pragma once

#include "../core/GameManager.hpp"
#include "../views/IBattleView.hpp"
#include <SFML/System/Vector2.hpp>
#include <optional>
#include <unordered_map>
#include <unordered_set>

class BattlePresenter {
public:
    BattlePresenter(GameManager& model, IBattleView& view);

    void start_battle();
    void on_hex_clicked(int q, int r, bool shift_held = false);
    void on_mouse_hover(int pixel_x, int pixel_y, bool shift_held);
    void on_right_click_pressed(int pixel_x, int pixel_y);
    void on_right_click_released();
    void on_defend_clicked();
    void on_wait_clicked();

private:
    void refresh_ui_for_active_unit();
    void push_render_data_to_view();
    std::vector<UnitRenderData> build_render_data_snapshot() const;
    std::pair<int, int> pixel_to_hex(float x, float y) const;
    CursorStyle direction_to_cursor(float angle_deg) const;
    void show_unit_range_preview(const Unit& unit);
    static std::uint64_t make_unit_id(const Unit* unit);
    static std::optional<UnitRenderData> find_unit(const std::vector<UnitRenderData>& units, std::uint64_t id);
    void queue_move_visual_if_needed(std::uint64_t unit_id,
                                     const std::vector<UnitRenderData>& before,
                                     const std::vector<UnitRenderData>& after);

    void queue_move_visual_along_path(std::uint64_t unit_id,
                                      const std::vector<std::pair<int,int>>& precomputed_path);

    void finalize_action_visuals(std::uint64_t actor_id, bool had_morale_bonus);
    std::vector<IBattleView::AttackOriginHex> build_attack_origins_for_target(
        const Unit& attacker,
        const Unit& target,
        const std::vector<Hex*>& destinations,
        const std::vector<IBattleView::PredictedFacing>& predictions) const;
    static std::vector<IBattleView::AttackOriginHex> dedupe_attack_origins(
        const std::vector<IBattleView::AttackOriginHex>& origins);
    const std::vector<IBattleView::AttackOriginHex>* get_cached_attack_origins_for_target(const Unit& target) const;
    const Hex* resolve_move_head_destination(const Unit& unit, const Hex& clicked_or_hovered_hex) const;
    void highlight_unit_body(const Unit& unit, HighlightType type) const;

    Hex* find_attack_approach(const Unit& attacker, const Hex& target_hex,
                              float pixel_x, float pixel_y) const;

    sf::Vector2f hex_to_pixel(int q, int r) const;

    GameManager& model;
    IBattleView& view;
    bool range_preview_active = false;
    bool info_panel_visible = false;   

    int last_cursor_px = 0;
    int last_cursor_py = 0;
    std::unordered_map<std::uint64_t, std::vector<IBattleView::AttackOriginHex>> cached_attack_origins_by_target;

    std::vector<Hex*>          cached_destinations;
    std::unordered_set<std::int64_t> cached_destinations_set;
    bool is_destination_cached(int q, int r) const;
};