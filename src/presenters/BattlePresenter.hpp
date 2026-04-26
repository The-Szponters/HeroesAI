#pragma once

#include "../core/GameManager.hpp"
#include "../views/IBattleView.hpp"
#include <SFML/System/Vector2.hpp>
#include <optional>

class BattlePresenter {
public:
    BattlePresenter(GameManager& model, IBattleView& view);

    void start_battle();
    void on_hex_clicked(int q, int r);
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

    // Returns the reachable approach hex (adjacent to any target-occupied hex)
    // whose pixel center is closest to (pixel_x, pixel_y), or nullptr if none.
    Hex* find_attack_approach(const Unit& attacker, const Hex& target_hex,
                              float pixel_x, float pixel_y) const;

    // Convert axial hex to screen pixel center.
    sf::Vector2f hex_to_pixel(int q, int r) const;

    GameManager& model;
    IBattleView& view;
    bool range_preview_active = false;
    int last_cursor_px = 0;
    int last_cursor_py = 0;
};