#include "BattlePresenter.hpp"

#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <unordered_set>
#include <vector>

namespace {
constexpr float kHexRadius = 28.0f;
constexpr float kGridOriginX = 300.0f;

constexpr float kGridOriginY = 70.0f + 28.0f * 1.5f;
constexpr float kPi = 3.14159265358979323846f;

std::int64_t make_hex_key(int q, int r) {
    return (static_cast<std::int64_t>(q) << 32)
           ^ (static_cast<std::uint32_t>(r));
}

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

bool hero_contains_unit(const Hero& hero, const Unit& unit) {
    for (const auto& candidate : hero.get_army().get_units()) {
        if (candidate && candidate.get() == &unit) {
            return true;
        }
    }
    return false;
}

int owner_id_for_unit(const GameManager& model, const Unit& unit) {
    if (hero_contains_unit(model.get_red_hero(), unit)) {
        return 0;
    }
    if (hero_contains_unit(model.get_blue_hero(), unit)) {
        return 1;
    }
    return -1;
}

UnitRenderData make_unit_render_data(const GameManager& model, const Unit& unit, int q, int r, bool is_corpse) {
    UnitRenderData data;
    data.id = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(&unit));
    data.q = q;
    data.r = r;
    data.name = unit.get_name();
    data.asset_filename = unit.get_asset_filename();
    data.description = unit.get_description();
    data.count = unit.get_count();
    data.hp_left = unit.get_health_left();
    data.max_hp_per_unit = unit.get_health();
    data.current_top_unit_hp = unit.get_health_left();
    data.owner_id = owner_id_for_unit(model, unit);
    data.base_attack = unit.get_base_attack();
    data.total_attack = unit.get_attack();
    data.base_defense = unit.get_base_defense();
    data.total_defense = unit.get_defense();
    data.base_speed = unit.get_base_speed();
    data.total_speed = unit.get_speed();
    data.base_damage_min = unit.get_base_damage_min();
    data.total_damage_min = unit.get_damage_min();
    data.base_damage_max = unit.get_base_damage_max();
    data.total_damage_max = unit.get_damage_max();
    data.is_facing_left = unit.is_facing_left();
    data.visual_facing_left = unit.get_visual_facing_left();
    data.is_ranged = unit.is_ranged();
    data.ammo = unit.get_ammo();
    data.max_ammo = unit.get_max_ammo();
    data.is_corpse = is_corpse;
    data.size = unit.get_size();
    data.is_teleporter = unit.is_teleporter_unit();
    data.is_flying = unit.is_flying_unit();
    return data;
}
}

BattlePresenter::BattlePresenter(GameManager& model, IBattleView& view)
    : model(model), view(view) {}

void BattlePresenter::start_battle() {
    push_render_data_to_view();   
    view.sync_unit_positions();   
    refresh_ui_for_active_unit(); 
}

void BattlePresenter::on_hex_clicked(int q, int r, bool ) {
    if (view.has_pending_visual_events()) {
        return;
    }

    view.clear_hover_destination_highlight();
    view.clear_attack_origin_highlights();

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

    if (clicked_hex->has_unit() && clicked_hex->get_unit().get() != active_unit) {
        Unit* target = clicked_hex->get_unit().get();
        if (target == nullptr) return;

        if (!model.are_enemies(*active_unit, *target)) {
            view.show_message("Cannot attack allied unit");
            return;
        }

        const bool will_shoot = model.will_shoot(*active_unit, *target);
        const bool had_morale = model.active_unit_has_morale_bonus();

        std::vector<std::pair<int,int>> intended_path_for_attack;

        try {
            if (will_shoot) {
                Hex& attacker_hex = model.get_board().get_hex(
                    active_unit->get_q(), active_unit->get_r(), active_unit->get_s());
                model.attack(*active_unit, *target, attacker_hex);
                view.show_message("Shoot!");
            } else if (model.can_attack(*active_unit, *clicked_hex)) {

                Hex& attacker_hex = model.get_board().get_hex(
                    active_unit->get_q(), active_unit->get_r(), active_unit->get_s());
                model.attack(*active_unit, *target, attacker_hex);
                view.show_message("Attack!");
            } else {
                Hex* approach = nullptr;
                if (const auto* cached = get_cached_attack_origins_for_target(*target); cached && !cached->empty()) {
                    const sf::Vector2f fpx{static_cast<float>(last_cursor_px), static_cast<float>(last_cursor_py)};
                    const IBattleView::AttackOriginHex* best = nullptr;
                    float best_d2 = std::numeric_limits<float>::max();
                    for (const IBattleView::AttackOriginHex& origin : *cached) {
                        const sf::Vector2f cp = hex_to_pixel(origin.q, origin.r);
                        const float dx = cp.x - fpx.x;
                        const float dy = cp.y - fpx.y;
                        const float d2 = dx * dx + dy * dy;
                        if (d2 < best_d2) {
                            best_d2 = d2;
                            best = &origin;
                        }
                    }
                    if (best != nullptr) {
                        try {
                            approach = &model.get_board().get_hex(best->q, best->r, -best->q - best->r);
                        } catch (const std::out_of_range&) {}
                    }
                }
                if (approach == nullptr) {
                    approach = find_attack_approach(*active_unit, *clicked_hex,
                                                    static_cast<float>(last_cursor_px),
                                                    static_cast<float>(last_cursor_py));
                }
                if (approach != nullptr) {

                    try {
                        const std::vector<const Hex*> chain =
                            model.find_path(*active_unit, *approach);
                        intended_path_for_attack.reserve(chain.size());
                        for (const Hex* h : chain) {
                            if (h != nullptr) intended_path_for_attack.emplace_back(h->get_q(), h->get_r());
                        }
                    } catch (const std::exception&) {}
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

        const auto before_att = find_unit(before_units, attacker_id);
        const auto after_att  = find_unit(after_units,  attacker_id);
        const auto before_def = find_unit(before_units, defender_id);
        const auto after_def  = find_unit(after_units,  defender_id);

        const bool defender_died =
            after_def.has_value()  && after_def->is_corpse
            && before_def.has_value() && !before_def->is_corpse;

        const bool attacker_took_damage =
            after_att.has_value() && before_att.has_value()
            && (after_att->hp_left < before_att->hp_left
                || (after_att->is_corpse && !before_att->is_corpse));
        const bool retaliation_occurred = attacker_took_damage && !defender_died;

        const bool attacker_died =
            after_att.has_value()  && after_att->is_corpse
            && before_att.has_value() && !before_att->is_corpse;

        view.clear_visual_events();
        view.update_render_data(before_units);   
        view.sync_unit_positions();

        if (will_shoot) {
            view.queue_attack_animation_facing(attacker_id, after_def->q, after_def->r);
            view.queue_projectile_animation(attacker_id, after_def->q, after_def->r,
                                            active_unit->get_projectile_asset(), 0.4f);
            view.queue_hit_animation(defender_id);

            if (defender_died) {
                view.queue_render_data_commit(after_units);
                view.queue_death_animation(defender_id);
            } else {
                view.queue_render_data_commit(after_units);
            }
            model.next_turn();
            finalize_action_visuals(attacker_id, had_morale);
            return;
        }

        if (intended_path_for_attack.size() >= 2) {
            queue_move_visual_along_path(attacker_id, intended_path_for_attack);
        } else {
            queue_move_visual_if_needed(attacker_id, before_units, after_units);
        }

        view.queue_attack_animation_facing(attacker_id, after_def->q, after_def->r);

        view.queue_hit_animation(defender_id);

        if (defender_died) {

            view.queue_render_data_commit(after_units);
            view.queue_death_animation(defender_id);
        } else if (retaliation_occurred) {

            view.queue_attack_animation_facing(defender_id, after_att->q, after_att->r);
            view.queue_hit_animation(attacker_id);

            view.queue_render_data_commit(after_units);

            if (attacker_died) {

                view.queue_death_animation(attacker_id);
            }
        } else {

            view.queue_render_data_commit(after_units);
        }

        model.next_turn();
        finalize_action_visuals(attacker_id, had_morale);
        return;
    }

    const Hex* move_head_hex = resolve_move_head_destination(*active_unit, *clicked_hex);
    if (move_head_hex != nullptr) {
        const bool had_morale = model.active_unit_has_morale_bonus();

        std::vector<std::pair<int,int>> intended_path;
        try {
            const std::vector<const Hex*> chain = model.find_path(*active_unit, *move_head_hex);
            intended_path.reserve(chain.size());
            for (const Hex* h : chain) {
                if (h != nullptr) intended_path.emplace_back(h->get_q(), h->get_r());
            }
        } catch (const std::exception&) {}

        try {
            Hex& move_head = model.get_board().get_hex(
                move_head_hex->get_q(), move_head_hex->get_r(), -move_head_hex->get_q() - move_head_hex->get_r());
            model.move(*active_unit, move_head);
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
        if (intended_path.size() >= 2) {
            queue_move_visual_along_path(mover_id, intended_path);
        } else {
            queue_move_visual_if_needed(mover_id, before_units, after_units);
        }
        view.queue_render_data_commit(after_units);

        model.next_turn();
        finalize_action_visuals(mover_id, had_morale);
    }
}

void BattlePresenter::on_mouse_hover(int pixel_x, int pixel_y, bool shift_held) {
    last_cursor_px = pixel_x;
    last_cursor_py = pixel_y;

    Unit* active_unit = model.get_current_unit();
    if (active_unit == nullptr) {
        view.clear_active_unit_highlight();
        view.clear_hover_destination_highlight();
        view.clear_attack_origin_highlights();
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
        view.clear_attack_origin_highlights();
        if (range_preview_active) {
            refresh_ui_for_active_unit();
            range_preview_active = false;
        }
        view.set_cursor_style(CursorStyle::Default, pixel_x, pixel_y);
        return;
    }

    if (shift_held && hovered_hex->has_unit()) {
        view.clear_hover_destination_highlight();
        view.clear_attack_origin_highlights();
        view.set_shift_preview_active(true);
        show_unit_range_preview(*hovered_hex->get_unit());
        range_preview_active = true;
        view.set_cursor_style(CursorStyle::QuestionMark, pixel_x, pixel_y);
        return;
    }

    if (range_preview_active) {
        view.set_shift_preview_active(false);
        refresh_ui_for_active_unit();
        range_preview_active = false;
    }

    if (const Hex* move_head_hex = resolve_move_head_destination(*active_unit, *hovered_hex);
        move_head_hex != nullptr) {
        const sf::Vector2f start_px = hex_to_pixel(active_unit->get_q(), active_unit->get_r());
        const sf::Vector2f hovered_px = hex_to_pixel(move_head_hex->get_q(), move_head_hex->get_r());

        bool future_is_facing_left = active_unit->get_visual_facing_left();
        if (hovered_px.x < start_px.x) {
            future_is_facing_left = true;
        } else if (hovered_px.x > start_px.x) {
            future_is_facing_left = false;
        }

        if (active_unit->get_size() == 2) {
            const int tail_dq = future_is_facing_left ? 1 : -1;
            view.set_hover_destination_highlight(move_head_hex->get_q(),
                                                 move_head_hex->get_r(),
                                                 true,
                                                 move_head_hex->get_q() + tail_dq,
                                                 move_head_hex->get_r());
        } else {
            view.set_hover_destination_highlight(move_head_hex->get_q(), move_head_hex->get_r(), false, 0, 0);
        }
    } else {
        view.clear_hover_destination_highlight();
    }

    const bool is_enemy = hovered_hex->has_unit()
                          && hovered_hex->get_unit().get() != active_unit
                          && model.are_enemies(*active_unit, *hovered_hex->get_unit());

    if (!is_enemy) {
        view.clear_attack_origin_highlights();

        const bool reachable = (resolve_move_head_destination(*active_unit, *hovered_hex) != nullptr);
        CursorStyle empty_style = CursorStyle::NotAvailable;
        if (reachable) {
            empty_style = active_unit->is_teleporter_unit()
                          ? CursorStyle::FlyMove
                          : CursorStyle::NormalMove;
        }
        view.set_cursor_style(empty_style, pixel_x, pixel_y);
        return;
    }

    if (active_unit->is_ranged() && active_unit->get_ammo() > 0) {
        Unit* hovered_unit = hovered_hex->get_unit().get();
        if (hovered_unit != nullptr && model.will_shoot(*active_unit, *hovered_unit)) {
            const int dist = ActionManager::hex_distance(*active_unit, *hovered_unit);
            view.set_cursor_style(
                dist > 10 ? CursorStyle::BrokenArrow : CursorStyle::RangeShoot,
                pixel_x, pixel_y);

                    for (const Hex* hex : cached_destinations) {
                        if (hex == nullptr) continue;
                        view.highlight_hex(hex->get_q(), hex->get_r(), HighlightType::Walkable);
                        if (active_unit->get_size() != 2) continue;
                        bool facing_left = active_unit->get_visual_facing_left();
                        const int predicted_tail_dq = facing_left ? 1 : -1;
                        view.highlight_hex(hex->get_q() + predicted_tail_dq, hex->get_r(), HighlightType::Walkable);
                        view.highlight_hex(hex->get_q() - predicted_tail_dq, hex->get_r(), HighlightType::Walkable);
                    }
            return;
        }

    }

    const sf::Vector2f fpx{static_cast<float>(pixel_x), static_cast<float>(pixel_y)};
    Hex* approach_hex = nullptr;
    const bool directly_adjacent = model.can_attack(*active_unit, *hovered_hex);
    std::vector<IBattleView::AttackOriginHex> attack_origins;

    if (const auto* cached = get_cached_attack_origins_for_target(*hovered_hex->get_unit()); cached) {
        attack_origins = *cached;
    }

    if (directly_adjacent) {
        IBattleView::AttackOriginHex self_origin;
        self_origin.q = active_unit->get_q();
        self_origin.r = active_unit->get_r();
        if (active_unit->get_size() == 2) {
            const int tail_dq = active_unit->is_facing_left() ? 1 : -1;
            self_origin.has_tail = true;
            self_origin.tail_q = self_origin.q + tail_dq;
            self_origin.tail_r = self_origin.r;
        }
        const bool present = std::any_of(attack_origins.begin(), attack_origins.end(),
            [&](const auto& o) { return o.q == self_origin.q && o.r == self_origin.r; });
        if (!present) attack_origins.push_back(self_origin);
    }

    if (!attack_origins.empty()) {

        sf::Vector2f defender_center = hex_to_pixel(hovered_hex->get_q(), hovered_hex->get_r());
        if (const auto& tu = hovered_hex->get_unit(); tu && tu->get_size() == 2) {
            const int tdq = tu->is_facing_left() ? 1 : -1;
            const sf::Vector2f tail = hex_to_pixel(tu->get_q() + tdq, tu->get_r());
            defender_center = {(defender_center.x + tail.x) * 0.5f,
                               (defender_center.y + tail.y) * 0.5f};
        }

        const float mouse_angle = std::atan2(fpx.y - defender_center.y,
                                             fpx.x - defender_center.x);

        const IBattleView::AttackOriginHex* best = nullptr;
        float best_diff = std::numeric_limits<float>::max();
        for (const IBattleView::AttackOriginHex& origin : attack_origins) {
            sf::Vector2f op = hex_to_pixel(origin.q, origin.r);
            if (origin.has_tail) {
                const sf::Vector2f tp = hex_to_pixel(origin.tail_q, origin.tail_r);
                const float dh = (op.x - defender_center.x) * (op.x - defender_center.x)
                               + (op.y - defender_center.y) * (op.y - defender_center.y);
                const float dt = (tp.x - defender_center.x) * (tp.x - defender_center.x)
                               + (tp.y - defender_center.y) * (tp.y - defender_center.y);
                if (dt < dh) op = tp;
            }
            const float origin_angle = std::atan2(op.y - defender_center.y,
                                                  op.x - defender_center.x);
            float diff = std::fabs(mouse_angle - origin_angle);
            if (diff > kPi) diff = 2.0f * kPi - diff;
            if (diff < best_diff) { best_diff = diff; best = &origin; }
        }
        if (best != nullptr) {
            try {
                approach_hex = &model.get_board().get_hex(best->q, best->r, -best->q - best->r);
            } catch (const std::out_of_range&) {}
        }
    } else if (!directly_adjacent) {

        approach_hex = find_attack_approach(*active_unit, *hovered_hex, fpx.x, fpx.y);
    }

    if (approach_hex == nullptr) {
        view.clear_attack_origin_highlights();
        view.set_cursor_style(CursorStyle::NotAvailable, pixel_x, pixel_y);
        return;
    }

    if (!attack_origins.empty()) {
        view.set_attack_origin_highlights(attack_origins);
    } else {
        view.clear_attack_origin_highlights();
    }

    if (approach_hex != nullptr) {
        bool has_tail = false;
        int tail_q = 0;
        int tail_r = 0;
        if (active_unit->get_size() == 2) {
            bool facing_left = active_unit->get_visual_facing_left();
            if (approach_hex->get_q() < active_unit->get_q()) facing_left = true;
            else if (approach_hex->get_q() > active_unit->get_q()) facing_left = false;
            const int tail_dq = facing_left ? 1 : -1;
            has_tail = true;
            tail_q = approach_hex->get_q() + tail_dq;
            tail_r = approach_hex->get_r();
        }
        view.set_hover_destination_highlight(approach_hex->get_q(), approach_hex->get_r(), has_tail, tail_q, tail_r);

        for (const Hex* hex : cached_destinations) {
            if (hex == nullptr) continue;
            view.highlight_hex(hex->get_q(), hex->get_r(), HighlightType::Walkable);
            if (active_unit->get_size() != 2) continue;
            bool facing_left = active_unit->get_visual_facing_left();
            const int predicted_tail_dq = facing_left ? 1 : -1;
            view.highlight_hex(hex->get_q() + predicted_tail_dq, hex->get_r(), HighlightType::Walkable);
            view.highlight_hex(hex->get_q() - predicted_tail_dq, hex->get_r(), HighlightType::Walkable);
        }
    }

    int attack_head_q = active_unit->get_q();
    int attack_head_r = active_unit->get_r();
    if (!directly_adjacent) {
        attack_head_q = approach_hex->get_q();
        attack_head_r = approach_hex->get_r();
    }

    std::vector<std::pair<int,int>> attacker_body{{attack_head_q, attack_head_r}};
    if (active_unit->get_size() == 2) {
        const int tail_dq = active_unit->is_facing_left() ? 1 : -1;
        attacker_body.emplace_back(attack_head_q + tail_dq, attack_head_r);
    }

    std::vector<std::pair<int,int>> target_body{{hovered_hex->get_q(), hovered_hex->get_r()}};
    if (const auto& tu = hovered_hex->get_unit(); tu && tu->get_size() == 2) {
        const int tdq = tu->is_facing_left() ? 1 : -1;
        target_body.emplace_back(tu->get_q() + tdq, tu->get_r());
    }

    auto are_adj_hex = [](int aq, int ar, int bq, int br) {
        const int as = -aq - ar;
        const int bs = -bq - br;
        return std::max({std::abs(aq - bq), std::abs(ar - br), std::abs(as - bs)}) == 1;
    };

    sf::Vector2f attacker_strike = hex_to_pixel(attack_head_q, attack_head_r);
    sf::Vector2f target_strike   = hex_to_pixel(hovered_hex->get_q(), hovered_hex->get_r());
    bool strike_resolved = false;
    float best_target_d2 = std::numeric_limits<float>::max();
    for (const auto& [aq, ar] : attacker_body) {
        for (const auto& [tq, tr] : target_body) {
            if (!are_adj_hex(aq, ar, tq, tr)) continue;
            const sf::Vector2f tp = hex_to_pixel(tq, tr);
            const float d2 = (tp.x - fpx.x) * (tp.x - fpx.x)
                           + (tp.y - fpx.y) * (tp.y - fpx.y);
            if (d2 < best_target_d2) {
                best_target_d2  = d2;
                attacker_strike = hex_to_pixel(aq, ar);
                target_strike   = tp;
                strike_resolved = true;
            }
        }
    }
    (void)strike_resolved;

    const float dx = attacker_strike.x - target_strike.x;
    const float dy = attacker_strike.y - target_strike.y;
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
            UnitRenderData data = make_unit_render_data(model, *unit, q, r, false);
            view.show_unit_info_panel(data);
            info_panel_visible = true;
            return;
        }

        const auto& dead_units = hex.get_dead_units();
        for (auto it = dead_units.rbegin(); it != dead_units.rend(); ++it) {
            if (const std::shared_ptr<Unit> dead = it->lock()) {
                UnitRenderData data = make_unit_render_data(model, *dead, q, r, true);
                view.show_unit_info_panel(data);
                info_panel_visible = true;
                return;
            }
        }
    } catch (const std::out_of_range&) {

    }

    view.hide_unit_info_panel();
    info_panel_visible = false;
}

void BattlePresenter::on_right_click_released() {
    view.hide_unit_info_panel();
    info_panel_visible = false;
}

void BattlePresenter::on_defend_clicked() {

    view.clear_visual_events();
    view.set_idle_callback(nullptr);

    Unit* active_unit = model.get_current_unit();
    if (active_unit == nullptr) {
        return;
    }

    const bool had_morale = model.active_unit_has_morale_bonus();
    const std::uint64_t actor_id = make_unit_id(active_unit);
    model.defend(*active_unit);
    view.clear_hover_destination_highlight();
    view.show_message("Unit defends");
    push_render_data_to_view();
    view.sync_unit_positions();
    model.next_turn();
    finalize_action_visuals(actor_id, had_morale);
}

void BattlePresenter::on_wait_clicked() {

    view.clear_visual_events();
    view.set_idle_callback(nullptr);

    Unit* active_unit = model.get_current_unit();
    if (active_unit == nullptr) {
        return;
    }

    const std::uint64_t actor_id = make_unit_id(active_unit);
    model.wait(*active_unit);
    view.clear_hover_destination_highlight();
    view.show_message("Unit waits");
    push_render_data_to_view();
    view.sync_unit_positions();
    model.next_turn();
    finalize_action_visuals(actor_id, false);
}

void BattlePresenter::refresh_ui_for_active_unit() {
    view.clear_all_highlights();
    view.clear_hover_destination_highlight();
    view.clear_attack_origin_highlights();
    cached_attack_origins_by_target.clear();

    Unit* active_unit = model.get_current_unit();
    if (active_unit == nullptr) {
        view.clear_active_unit_highlight();
        view.show_message("Battle Over");
        return;
    }

    view.update_hud(active_unit->get_name(), active_unit->get_count(), active_unit->get_health_left());

    constexpr std::size_t kLookaheadCapacity = 12;
    std::vector<IBattleView::TurnQueueSlot> slots;
    slots.reserve(kLookaheadCapacity);

    bool first = true;
    for (Unit* unit : model.get_unit_queue_in_round()) {
        if (slots.size() >= kLookaheadCapacity) break;
        if (unit == nullptr || unit->get_count() <= 0) continue;
        IBattleView::TurnQueueSlot slot;
        slot.unit_name = unit->get_name();
        slot.is_active = first;   
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

    const int active_tail_dq = active_unit->is_facing_left() ? 1 : -1;
    view.highlight_hex(active_unit->get_q(), active_unit->get_r(), HighlightType::ActiveUnit);
    if (active_unit->get_size() == 2) {
        view.highlight_hex(active_unit->get_q() + active_tail_dq,
                           active_unit->get_r(), HighlightType::ActiveUnit);
    }

    cached_destinations = model.get_available_destinations(*active_unit);
    cached_destinations_set.clear();
    cached_destinations_set.reserve(cached_destinations.size() * 2);
    for (const Hex* h : cached_destinations) {
        if (h != nullptr) cached_destinations_set.insert(make_hex_key(h->get_q(), h->get_r()));
    }
    const std::vector<Hex*>& destinations = cached_destinations;
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

    for (Hex* hex : destinations) {
        if (hex == nullptr) continue;
        view.highlight_hex(hex->get_q(), hex->get_r(), HighlightType::Walkable);
        if (active_unit->get_size() != 2) continue;

        bool facing_left = active_unit->is_facing_left();
        for (const IBattleView::PredictedFacing& p : predictions) {
            if (p.q == hex->get_q() && p.r == hex->get_r()) {
                facing_left = p.facing_left;
                break;
            }
        }
        const int predicted_tail_dq = facing_left ? 1 : -1;
        view.highlight_hex(hex->get_q() + predicted_tail_dq, hex->get_r(), HighlightType::Walkable);

        view.highlight_hex(hex->get_q() - predicted_tail_dq, hex->get_r(), HighlightType::Walkable);
    }

    view.set_predicted_facings(predictions);

    const auto& grid = model.get_board().get_grid();
    for (const Hex& hex : grid) {
        if (!hex.has_unit()) continue;
        const std::shared_ptr<Unit> maybe_target = hex.get_unit();
        if (!maybe_target) continue;
        Unit* target = maybe_target.get();
        if (target == active_unit) continue;
        if (hex.get_q() != target->get_q() || hex.get_r() != target->get_r()) continue;
        if (!model.are_enemies(*active_unit, *target)) continue;

        const std::vector<IBattleView::AttackOriginHex> origins =
            build_attack_origins_for_target(*active_unit, *target, destinations, predictions);
        if (!origins.empty()) {
            cached_attack_origins_by_target[make_unit_id(target)] = origins;
        }
    }

    const std::vector<std::pair<Unit*, Hex*>> attacks = model.get_available_attacks(*active_unit);
    for (const auto& [target, hex] : attacks) {
        if (target != nullptr && hex != nullptr) {
            highlight_unit_body(*target, HighlightType::Attackable);
        }
    }

    for (Unit* candidate : model.peek_next_round_order()) {
        if (candidate == nullptr || candidate == active_unit) continue;
        if (candidate->get_count() <= 0) continue;
        if (!model.are_enemies(*active_unit, *candidate)) continue;
        if (!model.will_shoot(*active_unit, *candidate)) continue;
        highlight_unit_body(*candidate, HighlightType::Attackable);
    }

    static constexpr int kDq[] = {1, 1, 0, -1, -1, 0};
    static constexpr int kDr[] = {0, -1, -1, 0, 1, 1};
    const int active_tail_dq_for_attacks = active_unit->is_facing_left() ? 1 : -1;
    for (const Hex* dest : destinations) {
        if (dest == nullptr) continue;
        std::vector<std::pair<int,int>> origins{{dest->get_q(), dest->get_r()}};
        if (active_unit->get_size() == 2) {
            origins.emplace_back(dest->get_q() + active_tail_dq_for_attacks, dest->get_r());
        }
        for (const auto& [oq, orr] : origins) {
            for (int i = 0; i < 6; ++i) {
                const int nq = oq + kDq[i];
                const int nr = orr + kDr[i];
                try {
                    const Hex& nhex = model.get_board().get_hex(nq, nr, -nq - nr);
                    if (!nhex.has_unit()) continue;
                    if (nhex.get_unit().get() == active_unit) continue;
                    if (!model.are_enemies(*active_unit, *nhex.get_unit())) continue;
                    highlight_unit_body(*nhex.get_unit(), HighlightType::Attackable);
                } catch (const std::out_of_range&) {}
            }
        }
    }
}

void BattlePresenter::show_unit_range_preview(const Unit& unit) {

    view.clear_all_highlights();
    view.clear_hover_destination_highlight();
    view.clear_attack_origin_highlights();
    highlight_unit_body(unit, HighlightType::ActiveUnit);

    auto are_adjacent = [](int aq, int ar, int bq, int br) {
        const int as = -aq - ar;
        const int bs = -bq - br;
        return std::max({std::abs(aq - bq), std::abs(ar - br), std::abs(as - bs)}) == 1;
    };

    const std::vector<Hex*> destinations = model.get_available_destinations(unit);

    const int unit_tail_dq = unit.is_facing_left() ? 1 : -1;
    for (Hex* dest : destinations) {
        if (dest == nullptr) continue;
        view.highlight_hex(dest->get_q(), dest->get_r(), HighlightType::Walkable);
        if (unit.get_size() == 2) {
            view.highlight_hex(dest->get_q() + unit_tail_dq, dest->get_r(), HighlightType::Walkable);
        }
    }

    for (Unit* candidate : model.peek_next_round_order()) {
        if (candidate == nullptr || candidate == &unit) continue;
        if (candidate->get_count() <= 0) continue;
        if (!model.are_enemies(unit, *candidate)) continue;

        bool can_hit = false;

        for (const auto& [target, hex] : model.get_available_attacks(unit)) {
            if (target == candidate) { can_hit = true; break; }
        }

        if (!can_hit && model.will_shoot(unit, *candidate)) can_hit = true;

        if (!can_hit) {
            std::vector<std::pair<int,int>> body;
            body.emplace_back(candidate->get_q(), candidate->get_r());
            if (candidate->get_size() == 2) {
                const int tail_dq = candidate->is_facing_left() ? 1 : -1;
                body.emplace_back(candidate->get_q() + tail_dq, candidate->get_r());
            }
            const int attacker_tail_dq = unit.is_facing_left() ? 1 : -1;
            for (Hex* dest : destinations) {
                if (dest == nullptr) continue;
                std::vector<std::pair<int,int>> attacker_body{
                    {dest->get_q(), dest->get_r()}};
                if (unit.get_size() == 2) {
                    attacker_body.emplace_back(dest->get_q() + attacker_tail_dq, dest->get_r());
                }
                for (const auto& [aq, ar] : attacker_body) {
                    for (const auto& [bq, br] : body) {
                        if (are_adjacent(aq, ar, bq, br)) {
                            can_hit = true; break;
                        }
                    }
                    if (can_hit) break;
                }
                if (can_hit) break;
            }
        }

        if (can_hit) highlight_unit_body(*candidate, HighlightType::Attackable);
    }
}

void BattlePresenter::push_render_data_to_view() {
    view.update_render_data(build_render_data_snapshot());
}

std::vector<IBattleView::AttackOriginHex> BattlePresenter::build_attack_origins_for_target(
    const Unit& attacker,
    const Unit& target,
    const std::vector<Hex*>& destinations,
    const std::vector<IBattleView::PredictedFacing>& ) const {
    auto are_adjacent = [](int aq, int ar, int bq, int br) {
        const int as = -aq - ar;
        const int bs = -bq - br;
        const int d = std::max({std::abs(aq - bq), std::abs(ar - br), std::abs(as - bs)});
        return d == 1;
    };

    std::vector<std::pair<int, int>> target_body;
    target_body.emplace_back(target.get_q(), target.get_r());
    if (target.get_size() == 2) {
        const int tail_dq = target.is_facing_left() ? 1 : -1;
        target_body.emplace_back(target.get_q() + tail_dq, target.get_r());
    }

    const int attacker_tail_dq = attacker.is_facing_left() ? 1 : -1;

    std::vector<IBattleView::AttackOriginHex> out;
    out.reserve(destinations.size());
    for (Hex* dest : destinations) {
        if (dest == nullptr) continue;

        std::vector<std::pair<int, int>> attacker_body;
        attacker_body.emplace_back(dest->get_q(), dest->get_r());

        IBattleView::AttackOriginHex origin;
        origin.q = dest->get_q();
        origin.r = dest->get_r();
        if (attacker.get_size() == 2) {
            attacker_body.emplace_back(dest->get_q() + attacker_tail_dq, dest->get_r());
            origin.has_tail = true;
            origin.tail_q = dest->get_q() + attacker_tail_dq;
            origin.tail_r = dest->get_r();
        }

        bool can_strike = false;
        for (const auto& [aq, ar] : attacker_body) {
            for (const auto& [tq, tr] : target_body) {
                if (are_adjacent(aq, ar, tq, tr)) {
                    can_strike = true;
                    break;
                }
            }
            if (can_strike) break;
        }

        if (can_strike) {
            out.push_back(origin);
        }
    }
    return dedupe_attack_origins(out);
}

std::vector<IBattleView::AttackOriginHex> BattlePresenter::dedupe_attack_origins(
    const std::vector<IBattleView::AttackOriginHex>& origins) {
    std::vector<IBattleView::AttackOriginHex> out;
    out.reserve(origins.size());
    std::unordered_set<std::int64_t> seen_heads;
    for (const IBattleView::AttackOriginHex& o : origins) {
        const std::int64_t key = make_hex_key(o.q, o.r);
        if (!seen_heads.insert(key).second) continue;
        out.push_back(o);
    }
    return out;
}

const std::vector<IBattleView::AttackOriginHex>* BattlePresenter::get_cached_attack_origins_for_target(
    const Unit& target) const {
    const auto it = cached_attack_origins_by_target.find(make_unit_id(&target));
    if (it == cached_attack_origins_by_target.end()) {
        return nullptr;
    }
    return &it->second;
}

bool BattlePresenter::is_destination_cached(int q, int r) const {
    return cached_destinations_set.find(make_hex_key(q, r)) != cached_destinations_set.end();
}

const Hex* BattlePresenter::resolve_move_head_destination(const Unit& unit, const Hex& clicked_or_hovered_hex) const {

    if (is_destination_cached(clicked_or_hovered_hex.get_q(), clicked_or_hovered_hex.get_r())) {
        return &clicked_or_hovered_hex;
    }
    if (unit.get_size() != 2) {
        return nullptr;
    }

    for (const int tail_dq : {-1, 1}) {
        const int head_q = clicked_or_hovered_hex.get_q() - tail_dq;
        const int head_r = clicked_or_hovered_hex.get_r();
        const int head_s = -head_q - head_r;
        if (!is_destination_cached(head_q, head_r)) continue;
        try {
            return &model.get_board().get_hex(head_q, head_r, head_s);
        } catch (const std::out_of_range&) {}
    }
    return nullptr;
}

void BattlePresenter::highlight_unit_body(const Unit& unit, HighlightType type) const {
    view.highlight_hex(unit.get_q(), unit.get_r(), type);
    if (unit.get_size() == 2) {
        const int tail_dq = unit.is_facing_left() ? 1 : -1;
        view.highlight_hex(unit.get_q() + tail_dq, unit.get_r(), type);
    }
}

std::vector<UnitRenderData> BattlePresenter::build_render_data_snapshot() const {
    std::vector<UnitRenderData> units;
    const auto& grid = model.get_board().get_grid();
    std::unordered_set<std::uint64_t> emitted_corpse_ids;

    for (const Hex& hex : grid) {
        if (hex.has_unit()) {
            auto unit = hex.get_unit();

            if (hex.get_q() != unit->get_q() || hex.get_r() != unit->get_r()) continue;
            units.push_back(make_unit_render_data(model, *unit, hex.get_q(), hex.get_r(), false));
        }

        for (const std::weak_ptr<Unit>& dead_weak : hex.get_dead_units()) {
            if (const std::shared_ptr<Unit> dead = dead_weak.lock()) {
                const std::uint64_t dead_id =
                    static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(dead.get()));

                if (!emitted_corpse_ids.insert(dead_id).second) {
                    continue;
                }
                units.push_back(make_unit_render_data(model, *dead, dead->get_q(), dead->get_r(), true));
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

void BattlePresenter::finalize_action_visuals(std::uint64_t actor_id, bool had_morale_bonus) {

    view.clear_all_highlights();
    view.clear_active_unit_highlight();
    view.clear_hover_destination_highlight();
    view.clear_attack_origin_highlights();
    view.set_predicted_facings({});
    cached_attack_origins_by_target.clear();
    cached_destinations.clear();
    cached_destinations_set.clear();

    if (had_morale_bonus) {
        view.queue_morale_animation(actor_id);
        view.show_message("Good morale! Bonus action.");
    }

    view.set_idle_callback([this]{ refresh_ui_for_active_unit(); });

    if (!view.has_pending_visual_events()) {
        view.set_idle_callback(nullptr);
        refresh_ui_for_active_unit();
    }
}

void BattlePresenter::queue_move_visual_along_path(std::uint64_t unit_id,
                                                   const std::vector<std::pair<int,int>>& path) {
    if (path.size() < 2) return;

    constexpr float kSecondsPerHex = 0.08f;

    for (Unit* u : model.get_unit_queue_in_round()) {
        if (u == nullptr || make_unit_id(u) != unit_id) continue;
        if (!u->ignores_path_blockers()) break;
        const auto& [from_q, from_r] = path.front();
        const auto& [to_q,   to_r]   = path.back();
        const int dq = to_q - from_q;
        const int dr = to_r - from_r;
        const int ds = -dq - dr;
        const int hex_dist = std::max({std::abs(dq), std::abs(dr), std::abs(ds)});
        const float duration_seconds = kSecondsPerHex * static_cast<float>(std::max(1, hex_dist));
        view.queue_move_animation(unit_id, from_q, from_r, to_q, to_r, duration_seconds);
        return;
    }

    for (std::size_t i = 1; i < path.size(); ++i) {
        const auto& [from_q, from_r] = path[i - 1];
        const auto& [to_q,   to_r]   = path[i];
        const int dq = to_q - from_q;
        const int dr = to_r - from_r;
        const int ds = -dq - dr;
        const int hex_dist = std::max({std::abs(dq), std::abs(dr), std::abs(ds)});
        const float duration_seconds = kSecondsPerHex * static_cast<float>(std::max(1, hex_dist));
        view.queue_move_animation(unit_id, from_q, from_r, to_q, to_r, duration_seconds);
    }
}

void BattlePresenter::queue_move_visual_if_needed(std::uint64_t unit_id,
                                                  const std::vector<UnitRenderData>& before,
                                                  const std::vector<UnitRenderData>& after) {
    const std::optional<UnitRenderData> before_unit = find_unit(before, unit_id);
    const std::optional<UnitRenderData> after_unit  = find_unit(after, unit_id);
    if (!before_unit.has_value() || !after_unit.has_value()) return;
    if (before_unit->q == after_unit->q && before_unit->r == after_unit->r) return;

    Unit* unit_ptr = nullptr;
    for (Unit* u : model.get_unit_queue_in_round()) {
        if (u != nullptr && make_unit_id(u) == unit_id) { unit_ptr = u; break; }
    }

    auto queue_single_segment = [&](int from_q, int from_r, int to_q, int to_r) {
        const int dq = to_q - from_q;
        const int dr = to_r - from_r;
        const int ds = -dq - dr;
        const int hex_distance = std::max({std::abs(dq), std::abs(dr), std::abs(ds)});

        constexpr float kSecondsPerHex = 0.08f;
        const float duration_seconds = kSecondsPerHex * static_cast<float>(std::max(1, hex_distance));
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

    const std::vector<Hex*>& reachable = cached_destinations.empty()
        ? (const_cast<BattlePresenter*>(this)->cached_destinations =
              model.get_available_destinations(attacker))
        : cached_destinations;
    if (reachable.empty()) return nullptr;

    auto are_adjacent = [](int aq, int ar, int bq, int br) {
        const int as = -aq - ar;
        const int bs = -bq - br;
        const int d = std::max({std::abs(aq - bq), std::abs(ar - br), std::abs(as - bs)});
        return d == 1;
    };

    std::vector<std::pair<int,int>> body_hexes;
    body_hexes.emplace_back(target_hex.get_q(), target_hex.get_r());
    if (target_hex.has_unit()) {
        const Unit* t = target_hex.get_unit().get();
        if (t && t->get_size() == 2) {
            const int tdq = t->is_facing_left() ? 1 : -1;
            body_hexes.emplace_back(t->get_q() + tdq, t->get_r());
        }
    }

    const int attacker_tail_dq = attacker.is_facing_left() ? 1 : -1;

    std::vector<Hex*> candidates;
    candidates.reserve(reachable.size());
    for (Hex* candidate : reachable) {
        if (candidate == nullptr) continue;

        std::vector<std::pair<int,int>> attacker_body;
        attacker_body.emplace_back(candidate->get_q(), candidate->get_r());
        if (attacker.get_size() == 2) {
            attacker_body.emplace_back(candidate->get_q() + attacker_tail_dq, candidate->get_r());
        }

        bool can_strike = false;
        for (const auto& [aq, ar] : attacker_body) {
            for (const auto& [tq, tr] : body_hexes) {
                if (are_adjacent(aq, ar, tq, tr)) {
                    can_strike = true;
                    break;
                }
            }
            if (can_strike) break;
        }
        if (can_strike) candidates.push_back(candidate);
    }

    if (candidates.empty()) return nullptr;

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

    float a = angle_deg;
    if (a < 0.0f) a += 360.0f;

    if (a < 30.0f || a >= 330.0f) return CursorStyle::SwordE;
    if (a < 90.0f)                return CursorStyle::SwordSE;
    if (a < 150.0f)               return CursorStyle::SwordSW;
    if (a < 210.0f)               return CursorStyle::SwordW;
    if (a < 270.0f)               return CursorStyle::SwordNW;
    return CursorStyle::SwordNE;
}