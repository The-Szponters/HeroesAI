#include "BattlePresenter.hpp"

#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <unordered_set>
#include <vector>

namespace {
constexpr float kHexRadius = 28.0f;
constexpr float kGridOriginX = 300.0f;
// Must stay in lockstep with SfmlBattleView::grid_origin.y so the presenter's
// pixel→hex conversion matches the View's hex→pixel mapping exactly.
constexpr float kGridOriginY = 70.0f + 28.0f * 1.5f;
constexpr float kPi = 3.14159265358979323846f;

std::pair<int, int> cube_round_to_axial(float fq, float fr, float fs) {
    int rq = static_cast<int>(std::round(fq));
    int rr = static_cast<int>(std::round(fr));
    int rs = static_cast<int>(std::round(fs));

    const float dq = std::fabs(static_cast<float>(rq) - fq);
    const float dr = std::fabs(static_cast<float>(rr) - fr);
    const float ds = std::fabs(static_cast<float>(rs) - fs);

    if (dq > dr && dq > ds) {
        rq = -rr - rs;
    } else if (dr > ds) {
        rr = -rq - rs;
    } else {
        rs = -rq - rr;
    }

    (void)rs;
    return {rq, rr};
}
}

BattlePresenter::BattlePresenter(GameManager& model, IBattleView& view)
    : model(model), view(view) {}

void BattlePresenter::start_battle() {
    push_render_data_to_view();   // populate view with initial unit state
    view.sync_unit_positions();   // ensure sprite positions are seeded
    refresh_ui_for_active_unit(); // highlights, HUD, turn-queue
}

void BattlePresenter::on_hex_clicked(int q, int r) {
    if (view.has_pending_visual_events()) {
        return;
    }

    view.clear_hover_destination_highlight();

    Unit* active_unit = model.get_current_unit();
    if (active_unit == nullptr) return;

    const std::vector<UnitRenderData> before_units = build_render_data_snapshot();

    const int s = -q - r;
    Hex* clicked_hex = nullptr;
    try {
        clicked_hex = &model.get_board().get_hex(q, r, s);
    } catch (const std::out_of_range&) {
        view.show_message("Invalid hex");
        return;
    }

    // Clicked on a unit that isn't the active unit → attempt attack.
    if (clicked_hex->has_unit() && clicked_hex->get_unit().get() != active_unit) {
        Unit* target = clicked_hex->get_unit().get();
        if (target == nullptr) return;

        if (!model.are_enemies(*active_unit, *target)) {
            view.show_message("Cannot attack allied unit");
            return;
        }

        try {
            if (model.can_attack(*active_unit, *clicked_hex)) {
                // Attacker is already adjacent — attack in place.
                Hex& attacker_hex = model.get_board().get_hex(
                    active_unit->get_q(), active_unit->get_r(), active_unit->get_s());
                model.attack(*active_unit, *target, attacker_hex);
                view.show_message("Attack!");
            } else {
                Hex* approach = find_attack_approach(*active_unit, *clicked_hex,
                                                     static_cast<float>(last_cursor_px),
                                                     static_cast<float>(last_cursor_py));
                if (approach != nullptr) {
                    model.attack(*active_unit, *target, *approach);
                    view.show_message("Move + Attack!");
                } else {
                    view.show_message("Cannot reach that enemy");
                    return;
                }
            }
        } catch (const std::exception& e) {
            view.show_message(std::string("Attack failed: ") + e.what());
            return;
        }

        const std::vector<UnitRenderData> after_units = build_render_data_snapshot();
        const std::uint64_t attacker_id = make_unit_id(active_unit);
        const std::uint64_t defender_id = make_unit_id(target);

        // ── Detect what happened during the model's atomic attack() call ──────
        const auto before_att = find_unit(before_units, attacker_id);
        const auto after_att  = find_unit(after_units,  attacker_id);
        const auto before_def = find_unit(before_units, defender_id);
        const auto after_def  = find_unit(after_units,  defender_id);

        // Defender died from the initial attack when their corpse appears in
        // after_units but was alive in before_units.
        const bool defender_died =
            after_def.has_value()  && after_def->is_corpse
            && before_def.has_value() && !before_def->is_corpse;

        // Retaliation occurred when the attacker took damage AND the defender
        // survived the initial strike (dead units cannot counter-attack).
        const bool attacker_took_damage =
            after_att.has_value() && before_att.has_value()
            && (after_att->hp_left < before_att->hp_left
                || (after_att->is_corpse && !before_att->is_corpse));
        const bool retaliation_occurred = attacker_took_damage && !defender_died;

        const bool attacker_died =
            after_att.has_value()  && after_att->is_corpse
            && before_att.has_value() && !before_att->is_corpse;

        // ── Build the sequential visual event chain ───────────────────────────
        view.clear_visual_events();
        view.update_render_data(before_units);   // show pre-attack state
        view.sync_unit_positions();

        // 1. Optional slide-to-adjacent-hex move.
        queue_move_visual_if_needed(attacker_id, before_units, after_units);

        // 2. Attacker swings — pre-rotated to face the defender's hex (#1).
        view.queue_attack_animation_facing(attacker_id, after_def->q, after_def->r);

        // 3. Defender flinches.
        view.queue_hit_animation(defender_id);

        if (defender_died) {
            // 4a. Commit the final state first so the controller switches to the
            //     Death animation, then stall until that animation finishes.
            view.queue_render_data_commit(after_units);
            view.queue_death_animation(defender_id);
        } else if (retaliation_occurred) {
            // 4b. Defender is alive → they retaliate, turning toward the attacker.
            view.queue_attack_animation_facing(defender_id, after_att->q, after_att->r);
            view.queue_hit_animation(attacker_id);

            // Commit final state (attacker's HP now reduced).
            view.queue_render_data_commit(after_units);

            if (attacker_died) {
                // Stall until the attacker's death animation finishes.
                view.queue_death_animation(attacker_id);
            }
        } else {
            // No death, no retaliation — commit straightaway.
            view.queue_render_data_commit(after_units);
        }

        model.next_turn();
        refresh_ui_for_active_unit();
        return;
    }

    // Clicked on an empty hex → attempt move.
    if (model.can_move(*active_unit, *clicked_hex)) {
        try {
            model.move(*active_unit, *clicked_hex);
        } catch (const std::exception& e) {
            view.show_message(std::string("Move failed: ") + e.what());
            return;
        }
        view.show_message("Move executed");

        const std::vector<UnitRenderData> after_units = build_render_data_snapshot();
        const std::uint64_t mover_id = make_unit_id(active_unit);

        view.clear_visual_events();
        view.update_render_data(before_units);
        view.sync_unit_positions();
        queue_move_visual_if_needed(mover_id, before_units, after_units);
        view.queue_render_data_commit(after_units);

        model.next_turn();
        refresh_ui_for_active_unit();
    }
}

void BattlePresenter::on_mouse_hover(int pixel_x, int pixel_y, bool shift_held) {
    last_cursor_px = pixel_x;
    last_cursor_py = pixel_y;

    Unit* active_unit = model.get_current_unit();
    if (active_unit == nullptr) {
        view.clear_active_unit_highlight();
        view.clear_hover_destination_highlight();
        view.set_cursor_style(CursorStyle::Default, pixel_x, pixel_y);
        return;
    }

    const auto [q, r] = pixel_to_hex(static_cast<float>(pixel_x), static_cast<float>(pixel_y));
    const int s = -q - r;

    Hex* hovered_hex = nullptr;
    try {
        hovered_hex = &model.get_board().get_hex(q, r, s);
    } catch (const std::out_of_range&) {
        view.clear_hover_destination_highlight();
        if (range_preview_active) {
            refresh_ui_for_active_unit();
            range_preview_active = false;
        }
        view.set_cursor_style(CursorStyle::Default, pixel_x, pixel_y);
        return;
    }

    if (shift_held && hovered_hex->has_unit()) {
        view.clear_hover_destination_highlight();
        show_unit_range_preview(*hovered_hex->get_unit());
        range_preview_active = true;
        view.set_cursor_style(CursorStyle::Default, pixel_x, pixel_y);
        return;
    }

    if (range_preview_active) {
        refresh_ui_for_active_unit();
        range_preview_active = false;
    }

    if (model.can_move(*active_unit, *hovered_hex)) {
        const sf::Vector2f start_px = hex_to_pixel(active_unit->get_q(), active_unit->get_r());
        const sf::Vector2f hovered_px = hex_to_pixel(hovered_hex->get_q(), hovered_hex->get_r());

        bool future_is_facing_left = active_unit->get_visual_facing_left();
        if (hovered_px.x < start_px.x) {
            future_is_facing_left = true;
        } else if (hovered_px.x > start_px.x) {
            future_is_facing_left = false;
        }

        if (active_unit->get_size() == 2) {
            const int tail_dq = future_is_facing_left ? 1 : -1;
            view.set_hover_destination_highlight(hovered_hex->get_q(),
                                                 hovered_hex->get_r(),
                                                 true,
                                                 hovered_hex->get_q() + tail_dq,
                                                 hovered_hex->get_r());
        } else {
            view.set_hover_destination_highlight(hovered_hex->get_q(), hovered_hex->get_r(), false, 0, 0);
        }
    } else {
        view.clear_hover_destination_highlight();
    }

    const bool is_enemy = hovered_hex->has_unit()
                          && hovered_hex->get_unit().get() != active_unit
                          && model.are_enemies(*active_unit, *hovered_hex->get_unit());

    if (!is_enemy) {
        view.set_cursor_style(CursorStyle::Default, pixel_x, pixel_y);
        return;
    }

    // Determine the approach hex the attacker would come from.
    // For a direct attack (already adjacent) use the attacker's current head hex.
    // For a move+attack use the reachable candidate closest to the cursor.
    const sf::Vector2f fpx{static_cast<float>(pixel_x), static_cast<float>(pixel_y)};
    Hex* approach_hex = nullptr;
    bool directly_adjacent = model.can_attack(*active_unit, *hovered_hex);
    if (!directly_adjacent) {
        approach_hex = find_attack_approach(*active_unit, *hovered_hex, fpx.x, fpx.y);
        if (approach_hex == nullptr) {
            view.set_cursor_style(CursorStyle::Default, pixel_x, pixel_y);
            return;
        }
    }

    // Sword direction: from the target centre toward the attacker position.
    const sf::Vector2f target_center = hex_to_pixel(hovered_hex->get_q(), hovered_hex->get_r());
    sf::Vector2f attacker_center;
    if (directly_adjacent) {
        attacker_center = hex_to_pixel(active_unit->get_q(), active_unit->get_r());
    } else {
        attacker_center = hex_to_pixel(approach_hex->get_q(), approach_hex->get_r());
    }

    const float dx = attacker_center.x - target_center.x;
    const float dy = attacker_center.y - target_center.y;
    const float angle_deg = std::atan2(dy, dx) * (180.0f / kPi);

    view.set_cursor_style(direction_to_cursor(angle_deg), pixel_x, pixel_y);
}

void BattlePresenter::on_right_click_pressed(int pixel_x, int pixel_y) {
    const auto [q, r] = pixel_to_hex(static_cast<float>(pixel_x), static_cast<float>(pixel_y));
    const int s = -q - r;

    try {
        const Hex& hex = model.get_board().get_hex(q, r, s);

        if (hex.has_unit()) {
            const std::shared_ptr<Unit> unit = hex.get_unit();
            UnitRenderData data;
            data.id = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(unit.get()));
            data.q = q;
            data.r = r;
            data.name = unit->get_name();
            data.asset_filename = unit->get_asset_filename();
            data.description = unit->get_description();
            data.count = unit->get_count();
            data.hp_left = unit->get_health_left();
            data.base_attack = unit->get_base_attack();
            data.total_attack = unit->get_attack();
            data.base_defense = unit->get_base_defense();
            data.total_defense = unit->get_defense();
            data.base_speed = unit->get_base_speed();
            data.total_speed = unit->get_speed();
            data.base_damage_min = unit->get_base_damage_min();
            data.total_damage_min = unit->get_damage_min();
            data.base_damage_max = unit->get_base_damage_max();
            data.total_damage_max = unit->get_damage_max();
            data.is_facing_left = unit->is_facing_left();
            data.visual_facing_left = unit->get_visual_facing_left();
            data.is_corpse = false;
            data.size = unit->get_size();
            data.is_teleporter = unit->is_teleporter_unit();
            view.show_unit_info_panel(data);
            return;
        }

        const auto& dead_units = hex.get_dead_units();
        for (auto it = dead_units.rbegin(); it != dead_units.rend(); ++it) {
            if (const std::shared_ptr<Unit> dead = it->lock()) {
                UnitRenderData data;
                data.id = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(dead.get()));
                data.q = q;
                data.r = r;
                data.name = dead->get_name();
                data.asset_filename = dead->get_asset_filename();
                data.description = dead->get_description();
                data.count = dead->get_count();
                data.hp_left = dead->get_health_left();
                data.base_attack = dead->get_base_attack();
                data.total_attack = dead->get_attack();
                data.base_defense = dead->get_base_defense();
                data.total_defense = dead->get_defense();
                data.base_speed = dead->get_base_speed();
                data.total_speed = dead->get_speed();
                data.base_damage_min = dead->get_base_damage_min();
                data.total_damage_min = dead->get_damage_min();
                data.base_damage_max = dead->get_base_damage_max();
                data.total_damage_max = dead->get_damage_max();
                data.is_facing_left = dead->is_facing_left();
                data.visual_facing_left = dead->get_visual_facing_left();
                data.is_corpse = true;
                data.size = dead->get_size();
                data.is_teleporter = dead->is_teleporter_unit();
                view.show_unit_info_panel(data);
                return;
            }
        }
    } catch (const std::out_of_range&) {
        // Outside board: hide panel.
    }

    view.hide_unit_info_panel();
}

void BattlePresenter::on_right_click_released() {
    view.hide_unit_info_panel();
}

void BattlePresenter::on_defend_clicked() {
    if (view.has_pending_visual_events()) {
        return;
    }

    Unit* active_unit = model.get_current_unit();
    if (active_unit == nullptr) {
        return;
    }

    model.defend(*active_unit);
    view.clear_hover_destination_highlight();
    view.show_message("Unit defends");
    push_render_data_to_view();
    view.sync_unit_positions();
    model.next_turn();
    refresh_ui_for_active_unit();
}

void BattlePresenter::on_wait_clicked() {
    if (view.has_pending_visual_events()) {
        return;
    }

    Unit* active_unit = model.get_current_unit();
    if (active_unit == nullptr) {
        return;
    }

    model.wait(*active_unit);
    view.clear_hover_destination_highlight();
    view.show_message("Unit waits");
    push_render_data_to_view();
    view.sync_unit_positions();
    model.next_turn();
    refresh_ui_for_active_unit();
}

void BattlePresenter::refresh_ui_for_active_unit() {
    view.clear_all_highlights();
    view.clear_hover_destination_highlight();

    Unit* active_unit = model.get_current_unit();
    if (active_unit == nullptr) {
        view.clear_active_unit_highlight();
        view.show_message("Battle Over");
        return;
    }

    view.update_hud(active_unit->get_name(), active_unit->get_count(), active_unit->get_health_left());

    // ── Build the lookahead turn queue (Issue #5) ──────────────────────────
    // 1. Drain the current round (already initiative-ordered, dead-filtered).
    // 2. If we still have headroom, append a "Round N+1" divider followed by
    //    the *next* round's predicted initiative order, also dead-filtered.
    // The View clamps to its own visible capacity, but we cap here too so we
    // never serialise more than two full rounds of state.
    constexpr std::size_t kLookaheadCapacity = 12;
    std::vector<IBattleView::TurnQueueSlot> slots;
    slots.reserve(kLookaheadCapacity);

    bool first = true;
    for (Unit* unit : model.get_unit_queue_in_round()) {
        if (slots.size() >= kLookaheadCapacity) break;
        if (unit == nullptr || unit->get_count() <= 0) continue;
        IBattleView::TurnQueueSlot slot;
        slot.unit_name = unit->get_name();
        slot.is_active = first;   // slot[0] is the actor
        slots.push_back(std::move(slot));
        first = false;
    }

    if (slots.size() < kLookaheadCapacity) {
        IBattleView::TurnQueueSlot divider;
        divider.is_divider  = true;
        divider.round_number = model.get_round_number() + 1;
        slots.push_back(std::move(divider));

        for (Unit* unit : model.peek_next_round_order()) {
            if (slots.size() >= kLookaheadCapacity) break;
            if (unit == nullptr || unit->get_count() <= 0) continue;
            IBattleView::TurnQueueSlot slot;
            slot.unit_name = unit->get_name();
            slots.push_back(std::move(slot));
        }
    }
    view.update_turn_order(slots);
    view.set_active_unit_highlight(active_unit->get_q(),
                                   active_unit->get_r(),
                                   active_unit->get_size(),
                                   active_unit->is_facing_left());

    // For 2-hex units highlight both the head and the tail hex.
    const int active_tail_dq = active_unit->is_facing_left() ? 1 : -1;
    view.highlight_hex(active_unit->get_q(), active_unit->get_r(), HighlightType::ActiveUnit);
    if (active_unit->get_size() == 2) {
        view.highlight_hex(active_unit->get_q() + active_tail_dq,
                           active_unit->get_r(), HighlightType::ActiveUnit);
    }

    // Walkable destinations — for 2-hex units also highlight the tail at each destination.
    const std::vector<Hex*> destinations = model.get_available_destinations(*active_unit);
    for (Hex* hex : destinations) {
        if (hex != nullptr) {
            view.highlight_hex(hex->get_q(), hex->get_r(), HighlightType::Walkable);
            if (active_unit->get_size() == 2) {
                view.highlight_hex(hex->get_q() + active_tail_dq,
                                   hex->get_r(), HighlightType::Walkable);
            }
        }
    }

    // Build the per-destination predicted-facing map.  For each reachable hex
    // we ask the model for the actual path; the second-to-last → last hex
    // direction tells us how the unit will be oriented when it arrives, which
    // is exactly what the View needs to predict the 2-hex tail position.
    std::vector<IBattleView::PredictedFacing> predictions;
    predictions.reserve(destinations.size());
    for (Hex* dest : destinations) {
        if (dest == nullptr) continue;
        const std::vector<const Hex*> path = model.find_path(*active_unit, *dest);
        bool facing_left = active_unit->is_facing_left();
        if (path.size() >= 2) {
            const Hex* penult = path[path.size() - 2];
            const sf::Vector2f penult_px = hex_to_pixel(penult->get_q(), penult->get_r());
            const sf::Vector2f final_px  = hex_to_pixel(dest->get_q(), dest->get_r());
            constexpr float kFlipDeadZone = 1.0f;
            if      (final_px.x < penult_px.x - kFlipDeadZone) facing_left = true;
            else if (final_px.x > penult_px.x + kFlipDeadZone) facing_left = false;
        }
        predictions.push_back({dest->get_q(), dest->get_r(), facing_left});
    }
    view.set_predicted_facings(predictions);

    // Adjacent attackable enemies (no movement needed).
    const std::vector<std::pair<Unit*, Hex*>> attacks = model.get_available_attacks(*active_unit);
    for (const auto& [target, hex] : attacks) {
        if (target != nullptr && hex != nullptr) {
            view.highlight_hex(hex->get_q(), hex->get_r(), HighlightType::Attackable);
        }
    }

    // Non-adjacent enemies reachable via move+attack: for every reachable
    // destination, check its six neighbours for occupying units.
    static constexpr int kDq[] = {1, 1, 0, -1, -1, 0};
    static constexpr int kDr[] = {0, -1, -1, 0, 1, 1};
    for (const Hex* dest : destinations) {
        if (dest == nullptr) continue;
        for (int i = 0; i < 6; ++i) {
            const int nq = dest->get_q() + kDq[i];
            const int nr = dest->get_r() + kDr[i];
            try {
                const Hex& nhex = model.get_board().get_hex(nq, nr, -nq - nr);
                if (!nhex.has_unit()) continue;
                if (nhex.get_q() == active_unit->get_q()
                    && nhex.get_r() == active_unit->get_r()) continue;
                if (!model.are_enemies(*active_unit, *nhex.get_unit())) continue;
                view.highlight_hex(nhex.get_q(), nhex.get_r(), HighlightType::Attackable);
            } catch (const std::out_of_range&) {}
        }
    }
}

void BattlePresenter::show_unit_range_preview(const Unit& unit) {
    view.clear_all_highlights();
    view.highlight_hex(unit.get_q(), unit.get_r(), HighlightType::ActiveUnit);

    const std::vector<Hex*> destinations = model.get_available_destinations(unit);
    for (Hex* hex : destinations) {
        if (hex != nullptr) {
            view.highlight_hex(hex->get_q(), hex->get_r(), HighlightType::Walkable);
        }
    }

    const std::vector<std::pair<Unit*, Hex*>> attacks = model.get_available_attacks(unit);
    for (const auto& [target, hex] : attacks) {
        if (target != nullptr && hex != nullptr) {
            view.highlight_hex(hex->get_q(), hex->get_r(), HighlightType::Attackable);
        }
    }
}

void BattlePresenter::push_render_data_to_view() {
    view.update_render_data(build_render_data_snapshot());
}

std::vector<UnitRenderData> BattlePresenter::build_render_data_snapshot() const {
    std::vector<UnitRenderData> units;
    const auto& grid = model.get_board().get_grid();

    for (const Hex& hex : grid) {
        if (hex.has_unit()) {
            auto unit = hex.get_unit();
            // Only emit each unit once (2-hex units occupy two hexes; skip duplicates).
            if (hex.get_q() != unit->get_q() || hex.get_r() != unit->get_r()) continue;
            units.push_back(UnitRenderData{
                .id = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(unit.get())),
                .q = hex.get_q(),
                .r = hex.get_r(),
                .name = unit->get_name(),
                .asset_filename = unit->get_asset_filename(),
                .description = unit->get_description(),
                .count = unit->get_count(),
                .hp_left = unit->get_health_left(),
                .base_attack = unit->get_base_attack(),
                .total_attack = unit->get_attack(),
                .base_defense = unit->get_base_defense(),
                .total_defense = unit->get_defense(),
                .base_speed = unit->get_base_speed(),
                .total_speed = unit->get_speed(),
                .base_damage_min = unit->get_base_damage_min(),
                .total_damage_min = unit->get_damage_min(),
                .base_damage_max = unit->get_base_damage_max(),
                .total_damage_max = unit->get_damage_max(),
                .is_facing_left = unit->is_facing_left(),
                .visual_facing_left = unit->get_visual_facing_left(),
                .is_corpse = false,
                .size = unit->get_size(),
                .is_teleporter = unit->is_teleporter_unit()
            });
        }

        for (const std::weak_ptr<Unit>& dead_weak : hex.get_dead_units()) {
            if (const std::shared_ptr<Unit> dead = dead_weak.lock()) {
                units.push_back(UnitRenderData{
                    .id = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(dead.get())),
                    .q = hex.get_q(),
                    .r = hex.get_r(),
                    .name = dead->get_name(),
                    .asset_filename = dead->get_asset_filename(),
                    .description = dead->get_description(),
                    .count = dead->get_count(),
                    .hp_left = dead->get_health_left(),
                    .base_attack = dead->get_base_attack(),
                    .total_attack = dead->get_attack(),
                    .base_defense = dead->get_base_defense(),
                    .total_defense = dead->get_defense(),
                    .base_speed = dead->get_base_speed(),
                    .total_speed = dead->get_speed(),
                    .base_damage_min = dead->get_base_damage_min(),
                    .total_damage_min = dead->get_damage_min(),
                    .base_damage_max = dead->get_base_damage_max(),
                    .total_damage_max = dead->get_damage_max(),
                    .is_facing_left = dead->is_facing_left(),
                    .visual_facing_left = dead->get_visual_facing_left(),
                    .is_corpse = true,
                    .size = dead->get_size(),
                    .is_teleporter = dead->is_teleporter_unit()
                });
            }
        }
    }

    return units;
}

std::uint64_t BattlePresenter::make_unit_id(const Unit* unit) {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(unit));
}

std::optional<UnitRenderData> BattlePresenter::find_unit(const std::vector<UnitRenderData>& units,
                                                         std::uint64_t id) {
    for (const UnitRenderData& unit : units) {
        if (unit.id == id) {
            return unit;
        }
    }
    return std::nullopt;
}

void BattlePresenter::queue_move_visual_if_needed(std::uint64_t unit_id,
                                                  const std::vector<UnitRenderData>& before,
                                                  const std::vector<UnitRenderData>& after) {
    const std::optional<UnitRenderData> before_unit = find_unit(before, unit_id);
    const std::optional<UnitRenderData> after_unit  = find_unit(after, unit_id);
    if (!before_unit.has_value() || !after_unit.has_value()) return;
    if (before_unit->q == after_unit->q && before_unit->r == after_unit->r) return;

    // Resolve the actual unit pointer so we can ask the model for the
    // reconstructed path it took.  Without the path we'd be slid in a straight
    // line from src→dst, which mis-orients the sprite on C-shaped routes
    // (Issue #1, walking).
    Unit* unit_ptr = nullptr;
    for (Unit* u : model.get_unit_queue_in_round()) {
        if (u != nullptr && make_unit_id(u) == unit_id) { unit_ptr = u; break; }
    }

    auto queue_single_segment = [&](int from_q, int from_r, int to_q, int to_r) {
        const int dq = to_q - from_q;
        const int dr = to_r - from_r;
        const int ds = -dq - dr;
        const int hex_distance = std::max({std::abs(dq), std::abs(dr), std::abs(ds)});
        const float duration_seconds = 0.18f * static_cast<float>(std::max(1, hex_distance));
        view.queue_move_animation(unit_id, from_q, from_r, to_q, to_r, duration_seconds);
    };

    if (unit_ptr != nullptr) {
        try {
            const int s = -after_unit->q - after_unit->r;
            const Hex& dest_hex = model.get_board().get_hex(after_unit->q, after_unit->r, s);
            const std::vector<const Hex*> chain = model.find_path(*unit_ptr, dest_hex);
            if (chain.size() >= 2) {
                for (std::size_t i = 1; i < chain.size(); ++i) {
                    queue_single_segment(chain[i-1]->get_q(), chain[i-1]->get_r(),
                                         chain[i]->get_q(),   chain[i]->get_r());
                }
                return;
            }
        } catch (const std::out_of_range&) {}
    }

    // Fallback: straight-line slide.  The View's MoveEvent::start handles the
    // single-segment facing so this is still correct, just less granular.
    queue_single_segment(before_unit->q, before_unit->r, after_unit->q, after_unit->r);
}

std::pair<int, int> BattlePresenter::pixel_to_hex(float x, float y) const {
    const float px = x - kGridOriginX;
    const float py = y - kGridOriginY;

    const float fq = (std::sqrt(3.0f) / 3.0f * px - 1.0f / 3.0f * py) / kHexRadius;
    const float fr = (2.0f / 3.0f * py) / kHexRadius;
    const float fs = -fq - fr;

    return cube_round_to_axial(fq, fr, fs);
}

sf::Vector2f BattlePresenter::hex_to_pixel(int q, int r) const {
    const float px = kGridOriginX + kHexRadius * (std::sqrt(3.0f) * (static_cast<float>(q) + static_cast<float>(r) * 0.5f));
    const float py = kGridOriginY + kHexRadius * (1.5f * static_cast<float>(r));
    return {px, py};
}

Hex* BattlePresenter::find_attack_approach(const Unit& attacker, const Hex& target_hex,
                                            float pixel_x, float pixel_y) const {
    static constexpr int kDq[] = {1, 1, 0, -1, -1, 0};
    static constexpr int kDr[] = {0, -1, -1, 0, 1, 1};

    const std::vector<Hex*> reachable = model.get_available_destinations(attacker);
    const std::unordered_set<Hex*> reachable_set(reachable.begin(), reachable.end());

    // All hexes occupied by the target body (head + tail for 2-hex units).
    std::vector<std::pair<int,int>> body_hexes;
    body_hexes.emplace_back(target_hex.get_q(), target_hex.get_r());
    if (target_hex.has_unit()) {
        const Unit* t = target_hex.get_unit().get();
        if (t && t->get_size() == 2) {
            const int tdq = t->is_facing_left() ? 1 : -1;
            body_hexes.emplace_back(t->get_q() + tdq, t->get_r());
        }
    }

    // Collect empty reachable hexes adjacent to any body hex (no duplicates).
    std::vector<Hex*> candidates;
    std::unordered_set<Hex*> seen;
    for (const auto& [bq, br] : body_hexes) {
        for (int i = 0; i < 6; ++i) {
            const int aq = bq + kDq[i];
            const int ar = br + kDr[i];
            try {
                Hex& adj = model.get_board().get_hex(aq, ar, -aq - ar);
                if (adj.has_unit()) continue;         // body hexes also excluded here
                if (!reachable_set.count(&adj)) continue;
                if (seen.insert(&adj).second) candidates.push_back(&adj);
            } catch (const std::out_of_range&) {}
        }
    }

    if (candidates.empty()) return nullptr;

    // Return the candidate whose pixel centre is closest to the cursor.
    Hex* best = nullptr;
    float best_d2 = std::numeric_limits<float>::max();
    for (Hex* c : candidates) {
        const sf::Vector2f cp = hex_to_pixel(c->get_q(), c->get_r());
        const float dx = cp.x - pixel_x;
        const float dy = cp.y - pixel_y;
        if (const float d2 = dx*dx + dy*dy; d2 < best_d2) { best_d2 = d2; best = c; }
    }
    return best;
}

CursorStyle BattlePresenter::direction_to_cursor(float angle_deg) const {
    // Normalize atan2 result from [-180, 180] to [0, 360).
    float a = angle_deg;
    if (a < 0.0f) a += 360.0f;

    // Divide the circle into 6 sectors of 60° aligned to pointy-top hex directions.
    // Screen y-axis points downward, so South is positive-y (atan2 returns positive values there).
    //   E  : [330, 360) ∪ [0,  30)
    //   SE : [30,  90)
    //   SW : [90,  150)
    //   W  : [150, 210)
    //   NW : [210, 270)
    //   NE : [270, 330)
    if (a < 30.0f || a >= 330.0f) return CursorStyle::SwordE;
    if (a < 90.0f)                return CursorStyle::SwordSE;
    if (a < 150.0f)               return CursorStyle::SwordSW;
    if (a < 210.0f)               return CursorStyle::SwordW;
    if (a < 270.0f)               return CursorStyle::SwordNW;
    return CursorStyle::SwordNE;
}