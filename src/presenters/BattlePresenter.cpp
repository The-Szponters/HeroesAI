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
// Must stay in lockstep with SfmlBattleView::grid_origin.y so the presenter's
// pixel→hex conversion matches the View's hex→pixel mapping exactly.
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
    return data;
}
}

BattlePresenter::BattlePresenter(GameManager& model, IBattleView& view)
    : model(model), view(view) {}

void BattlePresenter::start_battle() {
    push_render_data_to_view();   // populate view with initial unit state
    view.sync_unit_positions();   // ensure sprite positions are seeded
    refresh_ui_for_active_unit(); // highlights, HUD, turn-queue
}

void BattlePresenter::on_hex_clicked(int q, int r, bool /*shift_held*/) {
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

    // Clicked on a unit that isn't the active unit → attempt attack.
    if (clicked_hex->has_unit() && clicked_hex->get_unit().get() != active_unit) {
        Unit* target = clicked_hex->get_unit().get();
        if (target == nullptr) return;

        if (!model.are_enemies(*active_unit, *target)) {
            view.show_message("Cannot attack allied unit");
            return;
        }

        // Ranged path (HoMM3 shooter, ammo > 0, not blocked, not adjacent):
        // resolve in place, no movement, no retaliation.  We commit this
        // BEFORE the can_attack check below because shooting reaches farther
        // than melee adjacency.
        const bool will_shoot = model.will_shoot(*active_unit, *target);
        const bool had_morale = model.active_unit_has_morale_bonus();

        try {
            if (will_shoot) {
                Hex& attacker_hex = model.get_board().get_hex(
                    active_unit->get_q(), active_unit->get_r(), active_unit->get_s());
                model.attack(*active_unit, *target, attacker_hex);
                view.show_message("Shoot!");
            } else if (model.can_attack(*active_unit, *clicked_hex)) {
                // Attacker is already adjacent — attack in place.
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

        // Ranged: attacker swings in place, projectile flies, defender flinches,
        // then commit.  No movement, no retaliation.
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
        finalize_action_visuals(attacker_id, had_morale);
        return;
    }

    // Clicked on an empty hex → attempt move.
    const Hex* move_head_hex = resolve_move_head_destination(*active_unit, *clicked_hex);
    if (move_head_hex != nullptr) {
        const bool had_morale = model.active_unit_has_morale_bonus();
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
        queue_move_visual_if_needed(mover_id, before_units, after_units);
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
        show_unit_range_preview(*hovered_hex->get_unit());
        range_preview_active = true;
        view.set_cursor_style(CursorStyle::QuestionMark, pixel_x, pixel_y);
        return;
    }

    if (range_preview_active) {
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
        // Empty hex (or allied unit): NormalMove if we can stand there,
        // FlyMove if the active unit is a teleporter/flier, otherwise
        // NotAvailable to clearly mark unreachable terrain.
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

    // ── Ranged: intercept the clear-shot path before any approach math ────
    // A ranged unit with ammo, not blocked and not already adjacent fires in
    // place — no movement, no destination highlight needed.  Distance > 10
    // hexes triggers the BrokenArrow cursor to telegraph the 50% range
    // penalty, matching HoMM3.
    if (active_unit->is_ranged() && active_unit->get_ammo() > 0) {
        Unit* hovered_unit = hovered_hex->get_unit().get();
        if (hovered_unit != nullptr && model.will_shoot(*active_unit, *hovered_unit)) {
            view.clear_attack_origin_highlights();
            view.clear_hover_destination_highlight();
            const int dist = ActionManager::hex_distance(*active_unit, *hovered_unit);
            view.set_cursor_style(
                dist > 10 ? CursorStyle::BrokenArrow : CursorStyle::RangeShoot,
                pixel_x, pixel_y);
            return;
        }
        // Otherwise the ranged unit will be forced into melee (blocked or
        // already adjacent) — the BrokenArrow cursor is set after the
        // approach-resolution block below so we still render the same red
        // attack-origin highlight a melee attacker would get.
    }

    // Determine the approach hex the attacker would come from.
    // For a direct attack (already adjacent) use the attacker's current head hex.
    // For a move+attack use the reachable candidate closest to the cursor.
    const sf::Vector2f fpx{static_cast<float>(pixel_x), static_cast<float>(pixel_y)};
    Hex* approach_hex = nullptr;
    bool directly_adjacent = model.can_attack(*active_unit, *hovered_hex);
    std::vector<IBattleView::AttackOriginHex> attack_origins;
    if (directly_adjacent) {
        try {
            approach_hex = &model.get_board().get_hex(active_unit->get_q(), active_unit->get_r(), active_unit->get_s());
        } catch (const std::out_of_range&) {}
        IBattleView::AttackOriginHex origin;
        origin.q = active_unit->get_q();
        origin.r = active_unit->get_r();
        if (active_unit->get_size() == 2) {
            const int tail_dq = active_unit->is_facing_left() ? 1 : -1;
            origin.has_tail = true;
            origin.tail_q = origin.q + tail_dq;
            origin.tail_r = origin.r;
        }
        attack_origins.push_back(origin);
    } else if (const auto* cached = get_cached_attack_origins_for_target(*hovered_hex->get_unit()); cached) {
        attack_origins = *cached;
    }

    if (!directly_adjacent) {
        if (attack_origins.empty()) {
            approach_hex = find_attack_approach(*active_unit, *hovered_hex, fpx.x, fpx.y);
        } else {
            const IBattleView::AttackOriginHex* best = nullptr;
            float best_d2 = std::numeric_limits<float>::max();
            for (const IBattleView::AttackOriginHex& origin : attack_origins) {
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
                    approach_hex = &model.get_board().get_hex(best->q, best->r, -best->q - best->r);
                } catch (const std::out_of_range&) {}
            }
        }

        if (approach_hex == nullptr) {
            view.clear_attack_origin_highlights();
            view.set_cursor_style(CursorStyle::NotAvailable, pixel_x, pixel_y);
            return;
        }
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
    }

    // ── Sword direction ───────────────────────────────────────────────────
    // Pick the (attacker body hex, target body hex) pair that is actually
    // adjacent — that is the swing.  Using the closest pixel-distance pair
    // (the previous heuristic) could choose body cells two hexes apart,
    // producing an off-grid angle that didn't map cleanly to any hex
    // direction, which is why some 2-hex attacks displayed a wrong sword.
    bool future_facing_left = active_unit->get_visual_facing_left();
    int attack_head_q = active_unit->get_q();
    int attack_head_r = active_unit->get_r();
    if (!directly_adjacent) {
        attack_head_q = approach_hex->get_q();
        attack_head_r = approach_hex->get_r();
        if (attack_head_q < active_unit->get_q())      future_facing_left = true;
        else if (attack_head_q > active_unit->get_q()) future_facing_left = false;
    }

    std::vector<std::pair<int,int>> attacker_body{{attack_head_q, attack_head_r}};
    if (active_unit->get_size() == 2) {
        const int tail_dq = future_facing_left ? 1 : -1;
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
    for (const auto& [aq, ar] : attacker_body) {
        for (const auto& [tq, tr] : target_body) {
            if (are_adj_hex(aq, ar, tq, tr)) {
                attacker_strike = hex_to_pixel(aq, ar);
                target_strike   = hex_to_pixel(tq, tr);
                strike_resolved = true;
                break;
            }
        }
        if (strike_resolved) break;
    }

    // Vector points ATTACKER → TARGET, i.e. the direction the blade travels.
    // The HoMM3 cursor frames are oriented so the tip lies on the side facing
    // the target — anchoring the cursor sprite's top-left at the mouse means
    // the tip lands on the target with the handle trailing back toward the
    // attacker.  Inverting from the previous (target → attacker) convention
    // is the 180° flip the player saw on screen.
    const float dx = target_strike.x - attacker_strike.x;
    const float dy = target_strike.y - attacker_strike.y;
    const float angle_deg = std::atan2(dy, dx) * (180.0f / kPi);

    // Ranged unit forced into melee (blocked or adjacent → can't shoot) shows
    // the broken-arrow cursor so the player knows their swing will deal half
    // damage.  Pure melee attackers fall through to the directional sword.
    const CursorStyle attack_cursor = active_unit->is_ranged()
        ? CursorStyle::BrokenArrow
        : direction_to_cursor(angle_deg);
    view.set_cursor_style(attack_cursor, pixel_x, pixel_y);
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
        // Outside board: hide panel.
    }

    view.hide_unit_info_panel();
    info_panel_visible = false;
}

void BattlePresenter::on_right_click_released() {
    view.hide_unit_info_panel();
    info_panel_visible = false;
}

void BattlePresenter::on_defend_clicked() {
    if (view.has_pending_visual_events()) {
        return;
    }

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
    if (view.has_pending_visual_events()) {
        return;
    }

    Unit* active_unit = model.get_current_unit();
    if (active_unit == nullptr) {
        return;
    }

    // Wait forfeits any pending morale bonus (handled inside model.wait()),
    // so no aura plays — `false` skips the morale tail.
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

    // The morale aura plays at the END of an action chain (queued in
    // finalize_action_visuals), not at turn start — the player wanted to
    // see the bonus animation on the unit's *post-action* position.

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

    // Build the per-destination predicted-facing map.  For each reachable hex
    // we ask the model for the actual path; the second-to-last → last hex
    // direction tells us how the unit will be oriented when it arrives, which
    // is exactly what the View needs to predict the 2-hex tail position.
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

    // Walkable destinations — for 2-hex units highlight the destination tail
    // derived from the same predicted-facing map used by hover preview.
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

        // Also highlight the mirrored tail-side cell so both body-hex entry
        // points remain visibly available for UX (left/right approach parity).
        view.highlight_hex(hex->get_q() - predicted_tail_dq, hex->get_r(), HighlightType::Walkable);
    }

    view.set_predicted_facings(predictions);

    // Cache all valid post-attack standing positions per target once per turn
    // so hover only does cheap nearest-origin selection.
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

    // Adjacent attackable enemies (no movement needed).
    const std::vector<std::pair<Unit*, Hex*>> attacks = model.get_available_attacks(*active_unit);
    for (const auto& [target, hex] : attacks) {
        if (target != nullptr && hex != nullptr) {
            highlight_unit_body(*target, HighlightType::Attackable);
        }
    }

    // Ranged: every enemy on the board is shootable (subject to ammo + not
    // being blocked by an adjacent enemy).  peek_next_round_order returns all
    // currently-alive units already deduplicated, so 2-hex enemies aren't
    // visited twice and units that have already acted this round are still
    // included.
    for (Unit* candidate : model.peek_next_round_order()) {
        if (candidate == nullptr || candidate == active_unit) continue;
        if (candidate->get_count() <= 0) continue;
        if (!model.are_enemies(*active_unit, *candidate)) continue;
        if (!model.will_shoot(*active_unit, *candidate)) continue;
        highlight_unit_body(*candidate, HighlightType::Attackable);
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
                highlight_unit_body(*nhex.get_unit(), HighlightType::Attackable);
            } catch (const std::out_of_range&) {}
        }
    }
}

void BattlePresenter::show_unit_range_preview(const Unit& unit) {
    // Shift+hover preview (Epic 4): wipe ALL existing tints — including the
    // dark hover-destination square — and paint ONLY enemies the hovered
    // unit can attack right now (melee adjacency, ranged shoot, or
    // move-and-attack), all in red.
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

    for (Unit* candidate : model.peek_next_round_order()) {
        if (candidate == nullptr || candidate == &unit) continue;
        if (candidate->get_count() <= 0) continue;
        if (!model.are_enemies(unit, *candidate)) continue;

        bool can_hit = false;

        // 1. Already adjacent (melee).
        for (const auto& [target, hex] : model.get_available_attacks(unit)) {
            if (target == candidate) { can_hit = true; break; }
        }

        // 2. Within ranged shot.
        if (!can_hit && model.will_shoot(unit, *candidate)) can_hit = true;

        // 3. Move-then-attack: any reachable destination adjacent to candidate.
        if (!can_hit) {
            std::vector<std::pair<int,int>> body;
            body.emplace_back(candidate->get_q(), candidate->get_r());
            if (candidate->get_size() == 2) {
                const int tail_dq = candidate->is_facing_left() ? 1 : -1;
                body.emplace_back(candidate->get_q() + tail_dq, candidate->get_r());
            }
            for (Hex* dest : destinations) {
                if (dest == nullptr) continue;
                for (const auto& [bq, br] : body) {
                    if (are_adjacent(dest->get_q(), dest->get_r(), bq, br)) {
                        can_hit = true; break;
                    }
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
    const std::vector<IBattleView::PredictedFacing>& /*predictions*/) const {
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

    // Tail position is governed by the unit's *logical* facing, which is
    // fixed at initial placement (Unit::set_position guarantees
    // logical_facing_left never changes).  Using the visual / movement-
    // predicted facing for the tail produced the wrong attacker body and
    // dropped valid approach hexes for some directions — fixed here.
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
    // Hover hot path: O(1) cache lookup instead of a BFS per call (the BFS
    // was the dominant per-MouseMoved cost for high-speed units).
    if (is_destination_cached(clicked_or_hovered_hex.get_q(), clicked_or_hovered_hex.get_r())) {
        return &clicked_or_hovered_hex;
    }
    if (unit.get_size() != 2) {
        return nullptr;
    }

    // Support clicking/hovering either potential tail-side mapping and
    // resolve back to whichever head destination is actually reachable.
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
            // Only emit each unit once (2-hex units occupy two hexes; skip duplicates).
            if (hex.get_q() != unit->get_q() || hex.get_r() != unit->get_r()) continue;
            units.push_back(make_unit_render_data(model, *unit, hex.get_q(), hex.get_r(), false));
        }

        for (const std::weak_ptr<Unit>& dead_weak : hex.get_dead_units()) {
            if (const std::shared_ptr<Unit> dead = dead_weak.lock()) {
                const std::uint64_t dead_id =
                    static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(dead.get()));
                // 2-hex corpses are registered in two hex dead-lists; draw once.
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
    // Hide every transient highlight so the just-acted unit's sprite plays
    // its animation against a clean board — no stale move-range overlays,
    // no next-turn active-unit ring, no hover destination dots.
    view.clear_all_highlights();
    view.clear_active_unit_highlight();
    view.clear_hover_destination_highlight();
    view.clear_attack_origin_highlights();
    view.set_predicted_facings({});
    cached_attack_origins_by_target.clear();
    cached_destinations.clear();
    cached_destinations_set.clear();

    // Morale aura plays at the very end of the chain, after the unit has
    // actually arrived / struck — i.e. on top of its post-action position
    // — and only when the action consumed the +morale bonus.
    if (had_morale_bonus) {
        view.queue_morale_animation(actor_id);
        view.show_message("Good morale! Bonus action.");
    }

    // Defer the next-unit UI refresh until the queue empties so the new
    // active unit's highlights don't appear while the previous unit is
    // still mid-animation.
    view.set_idle_callback([this]{ refresh_ui_for_active_unit(); });

    // Instantly advance the UI when there is no animation queue left to wait
    // for (the common wait/defend path, or any no-op visual chain).
    if (!view.has_pending_visual_events()) {
        view.set_idle_callback(nullptr);
        refresh_ui_for_active_unit();
    }
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
        const float duration_seconds = 0.08f * static_cast<float>(std::max(1, hex_distance));
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
    // Reuse the per-turn destination cache so MouseMoved-driven approach
    // queries don't re-run a BFS each event.  Cache is only stale when no
    // refresh has happened yet (very first hover after start_battle); in
    // that case fall back to a fresh BFS so tests stay deterministic.
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

    // Tail offset is fixed by the unit's *logical* facing (it never changes
    // after initial placement), so the attacker body at every reachable
    // destination is { head, head + logical_tail_dq }.
    const int attacker_tail_dq = attacker.is_facing_left() ? 1 : -1;

    // Keep only reachable destinations from which the attacker can actually
    // strike the defender body. This supports 2-hex attackers attacking via
    // tail adjacency, not only head adjacency.
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