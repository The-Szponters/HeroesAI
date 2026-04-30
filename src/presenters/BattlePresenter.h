/**
 * @file BattlePresenter.h
 * @brief Mediator between the GameManager model and an IBattleView.
 */
#pragma once

#include "../core/GameManager.h"
#include "../views/IBattleView.h"
#include <optional>
#include <SFML/System/Vector2.hpp>
#include <unordered_map>
#include <unordered_set>

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
    BattlePresenter( core::GameManager& model, views::IBattleView& view );

    void start_battle( );
    void on_hex_clicked( int q, int r, bool shift_held = false );
    void on_mouse_hover( int pixel_x, int pixel_y, bool shift_held );
    void on_right_click_pressed( int pixel_x, int pixel_y );
    void on_right_click_released( );
    void on_defend_clicked( );
    void on_wait_clicked( );

private:
    void refresh_ui_for_active_unit( );
    void push_render_data_to_view( );
    std::vector<views::UnitRenderData> build_render_data_snapshot( ) const;
    std::pair<int, int> pixel_to_hex( float x, float y ) const;
    views::CursorStyle direction_to_cursor( float angle_deg ) const;
    void show_unit_range_preview( const models::Unit& unit );
    static std::uint64_t make_unit_id( const models::Unit* unit );
    static std::optional<views::UnitRenderData>
    find_unit( const std::vector<views::UnitRenderData>& units, std::uint64_t id );
    void queue_move_visual_if_needed( std::uint64_t unit_id,
                                      const std::vector<views::UnitRenderData>& before,
                                      const std::vector<views::UnitRenderData>& after );

    void queue_move_visual_along_path( std::uint64_t unit_id,
                                       const std::vector<std::pair<int, int>>& precomputed_path );

    void finalize_action_visuals( std::uint64_t actor_id, bool had_morale_bonus );
    std::vector<views::IBattleView::AttackOriginHex> build_attack_origins_for_target(
        const models::Unit& attacker,
        const models::Unit& target,
        const std::vector<models::Hex*>& destinations,
        const std::vector<views::IBattleView::PredictedFacing>& predictions ) const;
    static std::vector<views::IBattleView::AttackOriginHex>
    dedupe_attack_origins( const std::vector<views::IBattleView::AttackOriginHex>& origins );
    const std::vector<views::IBattleView::AttackOriginHex>*
    get_cached_attack_origins_for_target( const models::Unit& target ) const;
    const models::Hex*
    resolve_move_head_destination( const models::Unit& unit,
                                   const models::Hex& clicked_or_hovered_hex ) const;
    void highlight_unit_body( const models::Unit& unit, views::HighlightType type ) const;

    models::Hex* find_attack_approach( const models::Unit& attacker,
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
        models::Hex* approach = nullptr;
        views::IBattleView::AttackOriginHex origin{ };
        std::vector<views::IBattleView::AttackOriginHex> all_origins;
        bool directly_adjacent = false;
    };
    PickedApproach pick_attack_approach_for_cursor( const models::Unit& attacker,
                                                    const models::Hex& hovered_hex,
                                                    float pixel_x,
                                                    float pixel_y ) const;

    sf::Vector2f hex_to_pixel( int q, int r ) const;

    core::GameManager& model;
    views::IBattleView& view;
    bool range_preview_active = false;
    bool info_panel_visible = false;

    int last_cursor_px = 0;
    int last_cursor_py = 0;
    std::unordered_map<std::uint64_t, std::vector<views::IBattleView::AttackOriginHex>>
        cached_attack_origins_by_target;

    std::vector<models::Hex*> cached_destinations;
    std::unordered_set<std::int64_t> cached_destinations_set;
    bool is_destination_cached( int q, int r ) const;
};

} // namespace presenters