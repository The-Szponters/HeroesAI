#include "SfmlBattleView.hpp"

#include "../models/board.hpp"
#include "../presenters/BattlePresenter.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cctype>
#include <sstream>
#include <unordered_set>

namespace {
constexpr float kPi = 3.14159265358979323846f;

// Convert float cube coordinates to nearest valid axial hex.
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

SfmlBattleView::SfmlBattleView(unsigned int width, unsigned int height, const std::string& title)
    : window(sf::VideoMode({width, height}), title),
      screen_width(static_cast<float>(width)),
      screen_height(static_cast<float>(height)),
      battlefield_height(screen_height * 0.8f),
      hex_radius(28.0f),
      // Origin Y bumped by 1.5 * hex_radius (one full hex row of vertical
      // spacing) so the board sits lower on the battlefield panel and leaves
      // room above it for hero portraits / spell slots later.
      grid_origin(300.0f, 70.0f + 28.0f * 1.5f),
      hud_count(0),
      hud_hp_left(0) {
    window.setFramerateLimit(60);

    // Try project font first, then common Linux system fonts as fallbacks.
    const std::array<const char*, 5> font_candidates = {
        "assets/font.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
        "/usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf",
    };
    bool font_loaded = false;
    for (const char* path : font_candidates) {
        if (font.openFromFile(path)) { font_loaded = true; break; }
    }
    if (!font_loaded) {
        latest_message = "Warning: no font found, HUD text will not render";
    }

    hud_text = std::make_unique<sf::Text>(font);
    queue_text = std::make_unique<sf::Text>(font);
    log_text = std::make_unique<sf::Text>(font);
    info_panel_text = std::make_unique<sf::Text>(font);
    unit_stack_count_text = std::make_unique<sf::Text>(font);

    hud_text->setCharacterSize(18);
    hud_text->setFillColor(sf::Color::White);
    hud_text->setPosition({16.0f, battlefield_height + 8.0f});

    queue_text->setCharacterSize(16);
    queue_text->setFillColor(sf::Color(230, 230, 230));
    queue_text->setPosition({16.0f, battlefield_height + 30.0f});

    log_text->setCharacterSize(17);
    log_text->setFillColor(sf::Color(220, 220, 220));
    log_text->setPosition({16.0f, battlefield_height + 52.0f});

    // Dedicated DEF manager keeps resources alive while sprites reference frame textures.
    // Both unit animations and UI elements (combat cursor, morale aura, action-bar
    // icons, spellbook) share the same DefParser; the manager indexes them under
    // a single normalised filename namespace.
    def_manager.set_search_roots({
        "assets/units",
        "assets/ui",
    });

    // Load a default battle background (grass terrain).
    if (battlefield_texture.loadFromFile("assets/backgrounds/CmBkGrTr.bmp")) {
        const sf::Vector2u ts = battlefield_texture.getSize();
        battlefield_sprite = std::make_unique<sf::Sprite>(battlefield_texture);
        battlefield_sprite->setScale({
            screen_width  / static_cast<float>(ts.x),
            battlefield_height / static_cast<float>(ts.y)
        });
        battlefield_sprite->setPosition({0.0f, 0.0f});
    }

    // Custom combat cursor: hide OS pointer once and forever; the rendered
    // DEF frame replaces it entirely.  Re-enabled only if the user requests
    // CursorStyle::Default (e.g. from outside-window guards).
    window.setMouseCursorVisible(false);
    os_cursor_visible = false;

    info_panel_text->setCharacterSize(15);
    info_panel_text->setFillColor(sf::Color::White);

    unit_stack_count_text->setCharacterSize(8);
    unit_stack_count_text->setFillColor(sf::Color(20, 20, 20));

    unit_stack_team_backer.setFillColor(sf::Color::Transparent);
    unit_stack_team_backer.setOutlineThickness(0.0f);
    unit_stack_hp_back.setFillColor(sf::Color::Black);
    unit_stack_hp_fill.setFillColor(sf::Color::Green);

    load_action_bar_assets();
    load_info_panel_assets();
    load_unit_stack_assets();
}

void SfmlBattleView::load_action_bar_assets() {
    // No background bar — the icons composite directly over the HUD strip.
    // Cluster all five on the right edge with Wait immediately to the left of
    // Defend so the reading order keeps "Wait above Defend" intact.
    constexpr float kIconW = 48.0f;
    constexpr float kIconH = 36.0f;
    constexpr float kPad   = 8.0f;
    const float icon_y     = screen_height - kIconH - 8.0f;

    auto add = [&](ActionKind kind, const std::string& def, float x) {
        action_slots.push_back({
            kind, def, sf::FloatRect({x, icon_y}, {kIconW, kIconH}), 0.0f
        });
    };

    // Right-anchored, walking left so the table reads:
    //   ... Spellbook  Wait  Defend  AutoCombat  Surrender ]edge
    float x = screen_width - kIconW - kPad;
    add(ActionKind::Surrender,  "surrender_icon.def",  x); x -= kIconW + kPad;
    add(ActionKind::AutoCombat, "autocombat_icon.def", x); x -= kIconW + kPad;
    add(ActionKind::Defend,     "defend_icon.def",     x); x -= kIconW + kPad;
    add(ActionKind::Wait,       "wait_icon.def",       x); x -= kIconW + kPad;
    add(ActionKind::Spellbook,  "spellbook.def",       x);
}

void SfmlBattleView::load_info_panel_assets() {
    if (info_panel_texture.loadFromFile("assets/ui/unit_stats.bmp")) {
        info_panel_sprite = std::make_unique<sf::Sprite>(info_panel_texture);
    }
}

void SfmlBattleView::load_unit_stack_assets() {
    if (unit_stack_box_texture.loadFromFile("assets/ui/num_units.bmp")) {
        unit_stack_box_sprite = std::make_unique<sf::Sprite>(unit_stack_box_texture);
    }
}

bool SfmlBattleView::is_open() const {
    return window.isOpen();
}

void SfmlBattleView::on_mouse_hover(int pixel_x, int pixel_y, BattlePresenter& presenter) {
    // Off-battlefield hovers (HUD background, action buttons, turn queue): use
    // the plain HoMM3 pointer (combat_icons frame 6) and don't disturb the
    // presenter's active-unit highlights.  The presenter still owns every
    // battlefield interaction.
    if (!is_point_in_battlefield(static_cast<float>(pixel_x), static_cast<float>(pixel_y))) {
        if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Right)) {
            presenter.on_right_click_released();
        }
        set_cursor_style(CursorStyle::StandardPointer, pixel_x, pixel_y);
        return;
    }

    if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Right)) {
        presenter.on_right_click_pressed(pixel_x, pixel_y);
        set_cursor_style(CursorStyle::StandardPointer, pixel_x, pixel_y);
        return;
    }

    const bool shift_held = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift)
                            || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift);
    presenter.on_mouse_hover(pixel_x, pixel_y, shift_held);
}

void SfmlBattleView::process_events(BattlePresenter& presenter) {
    while (const std::optional<sf::Event> event = window.pollEvent()) {
        if (event->is<sf::Event::Closed>()) {
            window.close();
            continue;
        }

        if (const auto* mouseMove = event->getIf<sf::Event::MouseMoved>()) {
            on_mouse_hover(mouseMove->position.x, mouseMove->position.y, presenter);
            continue;
        }

        if (const auto* mousePress = event->getIf<sf::Event::MouseButtonPressed>()) {
            const float mx = static_cast<float>(mousePress->position.x);
            const float my = static_cast<float>(mousePress->position.y);

            if (mousePress->button == sf::Mouse::Button::Right) {
                presenter.on_right_click_pressed(mousePress->position.x, mousePress->position.y);
                continue;
            }

            if (mousePress->button != sf::Mouse::Button::Left) {
                continue;
            }

            if (route_action_click(mx, my, presenter)) {
                continue;
            }

            if (is_point_in_battlefield(mx, my)) {
                const auto [q, r] = pixel_to_hex(mx, my);
                const bool shift_held = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift)
                                        || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift);
                presenter.on_hex_clicked(q, r, shift_held);
            } else {
                presenter.on_right_click_released();
            }
            continue;
        }

        if (const auto* mouseRelease = event->getIf<sf::Event::MouseButtonReleased>()) {
            if (mouseRelease->button == sf::Mouse::Button::Right) {
                presenter.on_right_click_released();
            }
            continue;
        }

        // The info panel follows the held right mouse button and hides on release.
    }
}

void SfmlBattleView::render() {
    window.clear(sf::Color(23, 23, 27));

    const sf::Time dt = animation_clock.restart();
    update_visual_events(dt);
    for (auto& [key, controller] : animation_controllers) {
        (void)key;
        controller.update(dt);
    }
    pulse_phase_seconds += dt.asSeconds();
    // Tick down the action-button "pressed" flash timers.
    for (ActionSlot& slot : action_slots) {
        if (slot.pressed_seconds_left > 0.0f) {
            slot.pressed_seconds_left = std::max(0.0f, slot.pressed_seconds_left - dt.asSeconds());
        }
    }

    // Recompute hover destination every frame from the actual mouse position so
    // highlights stay correct even when the game state changes without a new
    // MouseMoved event (e.g. after a turn ends while the cursor is stationary).
    update_hover_from_mouse();

    // Per-frame cursor follow: keep the custom sprite glued to the OS pointer
    // even on frames where no MouseMoved event fires (e.g. during animations).
    // update_hover_from_mouse() also queries the mouse position; we cache the
    // value into `cursor_position` here so neither path makes a second X11
    // server roundtrip per frame.
    cursor_position = window.mapPixelToCoords(sf::Mouse::getPosition(window));

    draw_battlefield_background();
    draw_hex_grid();
    draw_units();
    draw_hud();
    draw_turn_queue();
    draw_unit_stack_ui();
    draw_info_panel();
    draw_cursor();

    window.display();
}

// ── Render helpers ──────────────────────────────────────────────────────────

void SfmlBattleView::draw_battlefield_background() {
    if (battlefield_sprite) {
        window.draw(*battlefield_sprite);
        return;
    }
    sf::RectangleShape battlefield_bg({screen_width, battlefield_height});
    battlefield_bg.setPosition({0.0f, 0.0f});
    battlefield_bg.setFillColor(sf::Color(54, 64, 51));
    window.draw(battlefield_bg);
}

void SfmlBattleView::draw_hex_grid() {
    for (int row = 0; row < Board::HEIGHT; ++row) {
        for (int col = 0; col < Board::WIDTH; ++col) {
            const int q = col - (row - (row & 1)) / 2;
            const int r = row;

            sf::ConvexShape hex = make_hex_shape(q, r);
            hex.setFillColor(sf::Color::Transparent);
            hex.setOutlineColor(sf::Color(200, 200, 220, 70));
            hex.setOutlineThickness(1.0f);

            const auto it = expanded_highlights.find(make_hex_key(q, r));
            if (it != expanded_highlights.end()) {
                switch (it->second) {
                    case HighlightType::ActiveUnit:
                        hex.setOutlineColor(sf::Color(70, 130, 255, 220));
                        hex.setOutlineThickness(2.5f);
                        break;
                    case HighlightType::Walkable:
                        hex.setFillColor(sf::Color(150, 150, 150, 90));
                        break;
                    case HighlightType::Attackable:
                        hex.setFillColor(sf::Color(220, 70, 70, 90));
                        break;
                    case HighlightType::AttackOrigin:
                        hex.setFillColor(sf::Color(95, 95, 95, 150));
                        break;
                    case HighlightType::HoverDestination:
                        // Distinctly darker than Walkable so the player can
                        // see exactly which hex(es) the unit will occupy
                        // *after* moving, on top of the lighter walkable tint.
                        hex.setFillColor(sf::Color(15, 15, 15, 200));
                        break;
                    case HighlightType::None:
                        break;
                }
            }
            window.draw(hex);
        }
    }
}

void SfmlBattleView::draw_units() {
    // Identify the live unit currently holding the turn (if any).  Match by
    // the active-unit highlight's (q,r) — that's what the presenter feeds
    // us; anchoring on a position avoids extending the View interface for a
    // single visual cue.
    std::uint64_t active_unit_id_for_glow = 0;
    if (active_unit_highlight.has_value()) {
        for (const UnitRenderData& u : units_to_draw) {
            if (!u.is_corpse
                && u.q == active_unit_highlight->q
                && u.r == active_unit_highlight->r) {
                active_unit_id_for_glow = u.id;
                break;
            }
        }
    }

    // Phase 6 — golden glow under the active unit.  Renders a slightly-larger
    // yellow-tinted copy of the live sprite behind the unit, alpha pulsing
    // at ~1 Hz between 0.30 and 0.75 so the unit reads as "selected" without
    // visually stealing attention from its own animation.
    auto draw_active_glow = [this](const UnitRenderData& unit) {
        const auto ctrl_it = animation_controllers.find(unit.id);
        if (ctrl_it == animation_controllers.end() || !ctrl_it->second.is_ready()) return;
        const sf::Sprite* base = ctrl_it->second.get_sprite();
        if (base == nullptr) return;

        constexpr float kPulseHz = 1.0f;
        const float t = 0.5f + 0.5f * std::sin(pulse_phase_seconds * 2.0f * kPi * kPulseHz);
        const float alpha_norm = 0.30f + 0.45f * t;
        const auto alpha = static_cast<std::uint8_t>(std::round(255.0f * alpha_norm));

        sf::Sprite glow = *base;                   // copy texture + transform
        glow.setColor(sf::Color(255, 215, 0, alpha));
        const sf::Vector2f s = base->getScale();
        constexpr float kGlowGrow = 1.08f;
        glow.setScale({s.x * kGlowGrow, s.y * kGlowGrow});
        window.draw(glow, sf::BlendAdd);
    };

    auto draw_one = [this](const UnitRenderData& unit) {
        sf::Vector2f center = unit_render_center(unit);
        const auto override_it = visual_position_overrides.find(unit.id);
        if (override_it != visual_position_overrides.end()) {
            center = override_it->second;
        }

        const auto ctrl_it = animation_controllers.find(unit.id);
        if (ctrl_it != animation_controllers.end() && ctrl_it->second.is_ready()) {
            if (const sf::Sprite* sprite = ctrl_it->second.get_sprite()) {
                window.draw(*sprite);
                return;
            }
        }

        // Fallback debug token when DEF art is unavailable.
        const float radius = hex_radius * 0.34f;
        sf::CircleShape body(radius);
        body.setOrigin({radius, radius});
        body.setPosition(center);

        if (unit.is_corpse) {
            body.setFillColor(sf::Color(120, 120, 120, 190));
            body.setOutlineColor(sf::Color(190, 190, 190, 200));
        } else {
            body.setFillColor(sf::Color(240, 210, 90, 230));
            body.setOutlineColor(sf::Color(15, 15, 15, 220));
        }
        body.setOutlineThickness(1.5f);
        window.draw(body);

        sf::ConvexShape facing_marker(3);
        const float dir = unit.is_facing_left ? -1.0f : 1.0f;
        facing_marker.setPoint(0, {center.x + dir * 14.0f, center.y});
        facing_marker.setPoint(1, {center.x + dir *  6.0f, center.y - 6.0f});
        facing_marker.setPoint(2, {center.x + dir *  6.0f, center.y + 6.0f});
        facing_marker.setFillColor(unit.is_corpse ? sf::Color(95, 95, 95) : sf::Color(40, 40, 40));
        window.draw(facing_marker);
    };

    // Corpses first, living units above them.
    for (const UnitRenderData& unit : units_to_draw) {
        if (unit.is_corpse) draw_one(unit);
    }
    for (const UnitRenderData& unit : units_to_draw) {
        if (unit.is_corpse) continue;
        // Glow goes UNDER the sprite — same z-layer, drawn first so the
        // unit's own art composites on top of the additive halo.
        if (unit.id == active_unit_id_for_glow) {
            draw_active_glow(unit);
        }
        draw_one(unit);
    }

    // Active projectile (if a Projectile event sits at the front of the queue).
    if (!visual_events.empty() && visual_events.front().type == VisualEvent::Type::Projectile) {
        const ProjectileVisualEvent& pe = visual_events.front().projectile;
        const float t = std::clamp(pe.elapsed_seconds / pe.duration_seconds, 0.0f, 1.0f);
        const sf::Vector2f pos {
            pe.from.x + (pe.to.x - pe.from.x) * t,
            pe.from.y + (pe.to.y - pe.from.y) * t,
        };

        std::shared_ptr<DefResource> proj = pe.projectile_asset.empty()
            ? nullptr : def_manager.get_or_load(pe.projectile_asset);
        const DefFrame* frame = nullptr;
        if (proj) {
            for (const auto& [gid, frames] : proj->groups) {
                (void)gid;
                for (const DefFrame& f : frames) {
                    if (f.width > 0 && f.height > 0) { frame = &f; break; }
                }
                if (frame) break;
            }
        }

        if (frame) {
            sf::Sprite spr(frame->texture);
            const auto sz = frame->texture.getSize();
            spr.setOrigin({sz.x * 0.5f, sz.y * 0.5f});
            // Rotate the projectile to face its travel direction so arrows
            // visually align with the line of flight.
            const float dx = pe.to.x - pe.from.x;
            const float dy = pe.to.y - pe.from.y;
            const float angle_deg = std::atan2(dy, dx) * (180.0f / kPi);
            spr.setRotation(sf::degrees(angle_deg));
            spr.setPosition(pos);
            window.draw(spr);
        } else {
            // Fallback: tiny circle so the absence of a projectile DEF is
            // never a silent rendering failure during development.
            sf::CircleShape dot(4.0f);
            dot.setOrigin({4.0f, 4.0f});
            dot.setPosition(pos);
            dot.setFillColor(sf::Color(255, 230, 100, 230));
            window.draw(dot);
        }
    }

    // Active morale aura — overlay morale.def's frames just above the unit.
    if (!visual_events.empty() && visual_events.front().type == VisualEvent::Type::Morale) {
        const MoraleVisualEvent& me = visual_events.front().morale;

        std::shared_ptr<DefResource> aura = def_manager.get_or_load("morale.def");
        const UnitRenderData* unit = find_unit_render_data(me.unit_id);
        if (aura && unit != nullptr) {
            const auto group_it = aura->groups.find(0);
            if (group_it != aura->groups.end() && !group_it->second.empty()) {
                const std::vector<DefFrame>& frames = group_it->second;
                const float t = std::clamp(me.elapsed_seconds / me.duration_seconds, 0.0f, 0.999f);
                const std::size_t idx = std::min<std::size_t>(
                    frames.size() - 1,
                    static_cast<std::size_t>(t * static_cast<float>(frames.size())));
                const DefFrame& frame = frames[idx];

                if (frame.width > 0 && frame.height > 0) {
                    sf::Sprite spr(frame.texture);
                    const auto sz = frame.texture.getSize();
                    sf::Vector2f center = unit_render_center(*unit);
                    if (const auto override_it = visual_position_overrides.find(unit->id);
                        override_it != visual_position_overrides.end()) {
                        center = override_it->second;
                    }
                    // Anchor the aura's bottom-centre slightly above the
                    // unit's hex centre so it reads as floating overhead
                    // rather than sitting on the sprite.
                    spr.setOrigin({sz.x * 0.5f, static_cast<float>(sz.y)});
                    spr.setPosition({center.x, center.y - hex_radius * 1.2f});
                    window.draw(spr);
                }
            }
        }
    }

}

void SfmlBattleView::draw_hud() {
    // Subtle full-width strip behind the HUD text + turn queue; no down-bar
    // background under the action icons (the icons render straight on top).
    sf::RectangleShape hud_bg({screen_width, screen_height - battlefield_height});
    hud_bg.setPosition({0.0f, battlefield_height});
    hud_bg.setFillColor(sf::Color(36, 36, 42));
    window.draw(hud_bg);

    hud_text->setString("Unit: " + hud_unit_name +
                        " | Count: "   + std::to_string(hud_count) +
                        " | HP Left: " + std::to_string(hud_hp_left));
    log_text->setString("Log: " + latest_message);

    window.draw(*hud_text);
    window.draw(*log_text);

    draw_action_bar();
}

void SfmlBattleView::draw_action_bar() {
    // No down-bar background — icons composite directly onto the HUD strip.
    // Each *_icon.def packs four poses (0=normal, 1=hover, 2=pressed,
    // 3=disabled); we toggle frame 2 for the brief click-feedback window.
    for (const ActionSlot& slot : action_slots) {
        std::shared_ptr<DefResource> res = def_manager.get_or_load(slot.def_filename);
        if (!res) continue;
        const auto group_it = res->groups.find(0);
        if (group_it == res->groups.end() || group_it->second.empty()) continue;

        const std::vector<DefFrame>& frames = group_it->second;
        const std::size_t want_idx = (slot.pressed_seconds_left > 0.0f && frames.size() > 2)
                                     ? 2u : 0u;
        const DefFrame& frame = frames[want_idx];
        if (frame.width <= 0 || frame.height <= 0) continue;

        sf::Sprite icon(frame.texture);
        icon.setPosition({slot.bounds.position.x, slot.bounds.position.y});
        window.draw(icon);
    }
}

void SfmlBattleView::draw_turn_queue() {
    // ── HoMM3 turn queue with infinite lookahead (Issue #5) ────────────────
    // The presenter has already done all the heavy lifting:
    //   • slot[0] is the current actor (is_active = true).
    //   • Subsequent unit slots are the remaining current-round queue, in
    //     initiative order (waited units tailing per HoMM3 rules).
    //   • A divider slot (is_divider = true) marks "Round N" boundaries.
    //   • Slots after the divider are the *next* round's predicted initiative
    //     order, dead units already filtered out.
    // The View just paints the slots inside a fixed visible capacity.
    constexpr float box_w     = 56.0f;
    constexpr float box_h     = 56.0f;
    constexpr float divider_w = 36.0f;
    constexpr float gap       = 4.0f;
    constexpr float start_x   = 16.0f;
    constexpr float kBarHeight = 44.0f;
    // Sit immediately above the action bar so neither overlaps the other.
    const float queue_y       = screen_height - kBarHeight - box_h - 6.0f;
    const float right_limit   = screen_width - 320.0f;
    constexpr std::size_t kVisibleCapacity = 12;

    float cursor_x = start_x;
    std::size_t painted = 0;

    for (const TurnQueueSlot& slot : turn_queue_slots) {
        if (painted >= kVisibleCapacity) break;
        const float w = slot.is_divider ? divider_w : box_w;
        if (cursor_x + w > right_limit) break;

        if (slot.is_divider) {
            const float divider_h = box_h + 14.0f;
            const float divider_y = queue_y - 7.0f;

            sf::RectangleShape divider({divider_w, divider_h});
            divider.setPosition({cursor_x, divider_y});
            divider.setFillColor(sf::Color(110, 70, 30));
            divider.setOutlineColor(sf::Color(255, 200, 100));
            divider.setOutlineThickness(2.5f);
            window.draw(divider);

            sf::Text round_label(font);
            round_label.setCharacterSize(11);
            round_label.setFillColor(sf::Color(255, 235, 180));
            round_label.setString("Round\n  " + std::to_string(slot.round_number));
            round_label.setPosition({cursor_x + 3.0f, divider_y + 10.0f});
            window.draw(round_label);
        } else {
            sf::RectangleShape box({box_w, box_h});
            box.setPosition({cursor_x, queue_y});
            if (slot.is_active) {
                box.setFillColor(sf::Color(45, 75, 130));
                box.setOutlineColor(sf::Color(255, 215, 0));
                box.setOutlineThickness(3.5f);
            } else {
                box.setFillColor(sf::Color(55, 55, 65));
                box.setOutlineColor(sf::Color(140, 140, 160));
                box.setOutlineThickness(1.0f);
            }
            window.draw(box);

            sf::Text name_text(font);
            name_text.setCharacterSize(12);
            name_text.setFillColor(sf::Color::White);
            std::string short_name = slot.unit_name;
            if (short_name.size() > 8) short_name.resize(8);
            name_text.setString(short_name);
            name_text.setPosition({cursor_x + 4.0f, queue_y + 6.0f});
            window.draw(name_text);

            if (slot.is_active) {
                sf::Text active_marker(font);
                active_marker.setCharacterSize(11);
                active_marker.setFillColor(sf::Color(255, 215, 0));
                active_marker.setString("ACTIVE");
                active_marker.setPosition({cursor_x + 4.0f, queue_y + box_h - 18.0f});
                window.draw(active_marker);
            }
        }

        cursor_x += w + gap;
        ++painted;
    }
}

void SfmlBattleView::draw_unit_stack_ui() {
    constexpr float kBoxYOffset = 12.0f;
    constexpr float kBackerPad = 2.0f;
    constexpr float kBarHeight = 3.0f;
    constexpr float kBarGap = 0.0f;

    const sf::Vector2u box_size_u = unit_stack_box_texture.getSize();
    const sf::Vector2f box_size{
        static_cast<float>(box_size_u.x > 0 ? box_size_u.x : 30u),
        static_cast<float>(box_size_u.y > 0 ? box_size_u.y : 11u),
    };

    for (const UnitRenderData& unit : units_to_draw) {
        if (unit.is_corpse || unit.count <= 0) {
            continue;
        }

        sf::Vector2f center = unit_render_center(unit);
        if (const auto override_it = visual_position_overrides.find(unit.id);
            override_it != visual_position_overrides.end()) {
            center = override_it->second;
        }
        if (const auto ctrl_it = animation_controllers.find(unit.id);
            ctrl_it != animation_controllers.end() && ctrl_it->second.is_ready()) {
            if (const sf::Sprite* sprite = ctrl_it->second.get_sprite()) {
                center = sprite->getPosition();
            }
        }

        const sf::Vector2f box_pos {
            std::round(center.x - box_size.x * 0.5f),
            std::round(center.y + kBoxYOffset),
        };
        const sf::Vector2f box_center {
            box_pos.x + box_size.x * 0.5f,
            box_pos.y + box_size.y * 0.5f,
        };

        sf::Color team_color = sf::Color(160, 40, 40, 220);
        if (unit.owner_id == 1) {
            team_color = sf::Color(45, 75, 170, 220);
        } else if (unit.owner_id != 0) {
            team_color = sf::Color(90, 90, 90, 220);
        }

        unit_stack_team_backer.setSize({box_size.x + kBackerPad * 2.0f, box_size.y + kBackerPad * 2.0f});
        unit_stack_team_backer.setPosition({box_pos.x - kBackerPad, box_pos.y - kBackerPad});
        unit_stack_team_backer.setFillColor(team_color);
        unit_stack_team_backer.setOutlineThickness(0.0f);
        window.draw(unit_stack_team_backer);

        const float hp_ratio = (unit.max_hp_per_unit > 0)
            ? std::clamp(static_cast<float>(unit.current_top_unit_hp) / static_cast<float>(unit.max_hp_per_unit), 0.0f, 1.0f)
            : 0.0f;
        sf::Color hp_color = sf::Color::Red;
        if (hp_ratio > 0.5f) {
            hp_color = sf::Color::Green;
        } else if (hp_ratio > 0.2f) {
            hp_color = sf::Color::Yellow;
        }

        unit_stack_hp_back.setSize({box_size.x, kBarHeight});
        unit_stack_hp_back.setPosition({box_pos.x, box_pos.y - kBarHeight - kBarGap});
        window.draw(unit_stack_hp_back);

        unit_stack_hp_fill.setSize({box_size.x * hp_ratio, kBarHeight});
        unit_stack_hp_fill.setPosition({box_pos.x, box_pos.y - kBarHeight - kBarGap});
        unit_stack_hp_fill.setFillColor(hp_color);
        window.draw(unit_stack_hp_fill);

        if (unit_stack_box_sprite) {
            unit_stack_box_sprite->setPosition(box_pos);
            window.draw(*unit_stack_box_sprite);
        } else {
            sf::RectangleShape fallback(box_size);
            fallback.setPosition(box_pos);
            fallback.setFillColor(sf::Color(230, 220, 170));
            fallback.setOutlineColor(sf::Color(90, 70, 30));
            fallback.setOutlineThickness(1.0f);
            window.draw(fallback);
        }

        unit_stack_count_text->setString(std::to_string(unit.count));
        const sf::FloatRect text_bounds = unit_stack_count_text->getLocalBounds();
        unit_stack_count_text->setOrigin({
            std::floor(text_bounds.position.x + text_bounds.size.x * 0.5f),
            std::floor(text_bounds.position.y + text_bounds.size.y * 0.5f),
        });
        unit_stack_count_text->setPosition(box_center);
        window.draw(*unit_stack_count_text);
    }
}

void SfmlBattleView::draw_info_panel() {
    if (!info_panel_visible || !info_panel_unit.has_value()) return;

    const UnitRenderData& u = *info_panel_unit;

    // Anchor the panel to the screen centre so it reads as a modal overlay
    // (the spec calls for centred placement) instead of trailing the unit.
    const sf::Vector2u panel_size = info_panel_sprite
        ? info_panel_texture.getSize()
        : sf::Vector2u{300u, 311u};
    const sf::Vector2f panel_pos {
        (screen_width  - static_cast<float>(panel_size.x)) * 0.5f,
        (screen_height - static_cast<float>(panel_size.y)) * 0.5f,
    };

    if (info_panel_sprite) {
        info_panel_sprite->setPosition(panel_pos);
        window.draw(*info_panel_sprite);
    } else {
        sf::RectangleShape fallback({static_cast<float>(panel_size.x), static_cast<float>(panel_size.y)});
        fallback.setPosition(panel_pos);
        fallback.setFillColor(sf::Color(20, 20, 24, 240));
        fallback.setOutlineColor(sf::Color(175, 175, 195));
        fallback.setOutlineThickness(2.0f);
        window.draw(fallback);
    }

    // Stat block.  The unit_stats.bmp template reserves the upper-left
    // quadrant for a portrait — leave that area blank for now (Phase 5+
    // will paint creature portraits there) and write text into the right
    // column starting just below the title bar.
    constexpr float kPortraitW = 130.0f;
    constexpr float kTextLeftPad  = kPortraitW + 12.0f;
    constexpr float kTextTopPad   = 24.0f;

    std::ostringstream panel;
    panel << u.name << (u.is_corpse ? " [Corpse]" : "") << "\n"
          << "Attack: " << u.total_attack << "\n"
          << "Defense: " << u.total_defense << "\n"
          << "Shoots left: " << (u.is_ranged ? std::to_string(u.ammo) : "") << "\n"
          << "Damage: " << u.total_damage_min << "-" << u.total_damage_max << "\n"
          << "Health: " << u.max_hp_per_unit << "\n"
          << "Health left: " << u.current_top_unit_hp << "\n"
          << "Speed: " << u.total_speed;
    info_panel_text->setString(panel.str());
    info_panel_text->setPosition({panel_pos.x + kTextLeftPad, panel_pos.y + kTextTopPad});
    window.draw(*info_panel_text);
}

void SfmlBattleView::draw_cursor() {
    if (cursor_style == CursorStyle::Default) return;

    // combat_icons.def is loaded lazily on the first call (one-shot — the
    // resource lives in the def_manager cache for the rest of the battle).
    std::shared_ptr<DefResource> res = def_manager.get_or_load("combat_icons.def");
    if (!res) return;

    const auto group_it = res->groups.find(0);
    if (group_it == res->groups.end()) return;
    const std::vector<DefFrame>& frames = group_it->second;

    const int frame_index = static_cast<int>(cursor_style);
    if (frame_index < 0 || frame_index >= static_cast<int>(frames.size())) return;

    const DefFrame& frame = frames[frame_index];
    if (frame.width <= 0 || frame.height <= 0) return;

    // HoMM3 cursor frames are top-left padded inside their canvas: anchoring
    // the sprite at (0,0) and positioning it at the mouse keeps the click
    // hotspot at the icon's tip exactly as the original game does.
    sf::Sprite sprite(frame.texture);
    sprite.setOrigin({0.0f, 0.0f});
    sprite.setPosition(cursor_position);
    window.draw(sprite);
}

void SfmlBattleView::clear_all_highlights() {
    highlights.clear();
    refresh_expanded_highlights();
}

void SfmlBattleView::highlight_hex(int q, int r, HighlightType type) {
    highlights[make_hex_key(q, r)] = type;
    refresh_expanded_highlights();
}

void SfmlBattleView::update_hud(const std::string& unit_name, int count, int hp_left) {
    hud_unit_name = unit_name;
    hud_count = count;
    hud_hp_left = hp_left;
}

void SfmlBattleView::update_turn_order(const std::vector<TurnQueueSlot>& slots) {
    turn_queue_slots = slots;
}

void SfmlBattleView::show_message(const std::string& msg) {
    latest_message = msg;
}

void SfmlBattleView::set_active_unit_highlight(int q, int r, int size, bool is_facing_left) {
    active_unit_highlight = ActiveUnitHighlight{q, r, size, is_facing_left};
    refresh_expanded_highlights();
}

void SfmlBattleView::clear_active_unit_highlight() {
    active_unit_highlight.reset();
    refresh_expanded_highlights();
}

void SfmlBattleView::set_hover_destination_highlight(int q, int r, bool has_tail, int tail_q, int tail_r) {
    hover_destination_highlight = HoverDestinationHighlight{q, r, has_tail, tail_q, tail_r};
    refresh_expanded_highlights();
}

void SfmlBattleView::clear_hover_destination_highlight() {
    hover_destination_highlight.reset();
    refresh_expanded_highlights();
}

void SfmlBattleView::set_attack_origin_highlights(const std::vector<AttackOriginHex>& origins) {
    attack_origin_highlights = origins;
    refresh_expanded_highlights();
}

void SfmlBattleView::clear_attack_origin_highlights() {
    attack_origin_highlights.clear();
    refresh_expanded_highlights();
}

void SfmlBattleView::set_predicted_facings(const std::vector<PredictedFacing>& predictions) {
    predicted_facing_by_hex.clear();
    predicted_facing_by_hex.reserve(predictions.size());
    for (const PredictedFacing& p : predictions) {
        predicted_facing_by_hex[make_hex_key(p.q, p.r)] = p.facing_left;
    }
}

// Scale picks a creature size relative to the hex grid.  We measure off the
// Stand group specifically (group 1) so dynamic poses like Attack/Move don't
// influence the scale.  Falls back to any non-empty group if Stand is absent.
static float compute_scale(const DefResource& res, int unit_size, float hex_radius,
                           const std::string& asset_filename) {
    auto pick_height = [](const std::vector<DefFrame>& frames) -> float {
        for (const DefFrame& f : frames) {
            if (f.height > 0) return static_cast<float>(f.height);
        }
        return 0.0f;
    };

    float content_h = 0.0f;
    if (auto it = res.groups.find(1); it != res.groups.end()) {
        content_h = pick_height(it->second);
    }
    if (content_h <= 0.0f) {
        for (const auto& [gid, frames] : res.groups) {
            content_h = pick_height(frames);
            if (content_h > 0.0f) break;
        }
    }
    if (content_h <= 0.0f) return 1.0f;
    const float target_h = (unit_size == 2) ? hex_radius * 3.5f : hex_radius * 3.0f;
    float scale = (target_h / content_h) * 0.8f;

    // +25% global size bump for readability — except for Hell Hound (CHHOUN),
    // whose DEF artwork already over-fills its 2-hex footprint.  Filename
    // match is case-insensitive because asset paths can mix cases on disk.
    auto iequals = [](const std::string& a, const char* b) {
        if (a.size() != std::strlen(b)) return false;
        for (std::size_t i = 0; i < a.size(); ++i)
            if (std::tolower(static_cast<unsigned char>(a[i])) !=
                std::tolower(static_cast<unsigned char>(b[i]))) return false;
        return true;
    };
    if (!iequals(asset_filename, "CHHOUN.def")) {
        scale *= 1.25f;
    }
    return scale;
}

void SfmlBattleView::sync_unit_positions() {
    apply_current_render_data_to_controllers(false);
}

void SfmlBattleView::update_render_data(const std::vector<UnitRenderData>& units) {
    model_units_latest = units;
    if (visual_events.empty()) {
        units_to_draw = model_units_latest;
        visual_position_overrides.clear();
        apply_current_render_data_to_controllers(false);
    }
    refresh_expanded_highlights();
}

void SfmlBattleView::queue_move_animation(std::uint64_t unit_id,
                                          int from_q,
                                          int from_r,
                                          int to_q,
                                          int to_r,
                                          float duration_seconds) {
    const UnitRenderData* unit = find_unit_render_data(unit_id);
    if (unit == nullptr) {
        return;
    }

    VisualEvent event;
    event.type = VisualEvent::Type::Move;
    event.move.unit_id = unit_id;
    event.move.from = unit_render_center(*unit, from_q, from_r);
    event.move.to = unit_render_center(*unit, to_q, to_r);
    event.move.duration_seconds = std::max(0.001f, duration_seconds);
    event.move.is_teleporter = unit->is_teleporter;
    visual_events.push_back(event);
}

void SfmlBattleView::queue_attack_animation(std::uint64_t attacker_id, float /*duration_seconds*/) {
    // duration_seconds is ignored; completion is frame-based (is_finished()), with a
    // generous safety timeout to prevent hangs if the DEF lacks an Attack group.
    VisualEvent event;
    event.type = VisualEvent::Type::Attack;
    event.attack.attacker_id = attacker_id;
    visual_events.push_back(event);
}

void SfmlBattleView::queue_attack_animation_facing(std::uint64_t attacker_id, int target_q, int target_r) {
    VisualEvent event;
    event.type = VisualEvent::Type::Attack;
    event.attack.attacker_id   = attacker_id;
    event.attack.has_target_hex = true;
    event.attack.target_q       = target_q;
    event.attack.target_r       = target_r;
    visual_events.push_back(event);
}

void SfmlBattleView::queue_projectile_animation(std::uint64_t attacker_id,
                                                int target_q, int target_r,
                                                const std::string& projectile_asset,
                                                float duration_seconds) {
    VisualEvent event;
    event.type = VisualEvent::Type::Projectile;
    event.projectile.attacker_id     = attacker_id;
    event.projectile.projectile_asset = projectile_asset;
    event.projectile.duration_seconds = std::max(0.05f, duration_seconds);

    // Resolve attacker's current sprite position (post-move overrides included).
    sf::Vector2f from{0.0f, 0.0f};
    if (const auto it = animation_controllers.find(attacker_id); it != animation_controllers.end()) {
        if (const sf::Sprite* s = it->second.get_sprite()) {
            from = s->getPosition();
        }
    }
    if (const UnitRenderData* unit = find_unit_render_data(attacker_id); unit && from == sf::Vector2f{0.0f, 0.0f}) {
        from = unit_render_center(*unit);
    }

    event.projectile.from = from;
    event.projectile.to   = hex_to_pixel(target_q, target_r);
    visual_events.push_back(std::move(event));
}

void SfmlBattleView::queue_morale_animation(std::uint64_t unit_id) {
    VisualEvent event;
    event.type = VisualEvent::Type::Morale;
    event.morale.unit_id = unit_id;
    visual_events.push_back(std::move(event));
}

void SfmlBattleView::queue_hit_animation(std::uint64_t defender_id) {
    VisualEvent event;
    event.type = VisualEvent::Type::Hit;
    event.hit.defender_id = defender_id;
    visual_events.push_back(event);
}

void SfmlBattleView::queue_death_animation(std::uint64_t unit_id) {
    VisualEvent event;
    event.type = VisualEvent::Type::Death;
    event.death.unit_id = unit_id;
    visual_events.push_back(event);
}

void SfmlBattleView::queue_render_data_commit(const std::vector<UnitRenderData>& units) {
    VisualEvent event;
    event.type = VisualEvent::Type::CommitRenderData;
    event.commit.units = units;
    visual_events.push_back(std::move(event));
}

void SfmlBattleView::clear_visual_events() {
    visual_events.clear();
    visual_position_overrides.clear();
}

bool SfmlBattleView::has_pending_visual_events() const {
    return !visual_events.empty();
}

void SfmlBattleView::set_idle_callback(std::function<void()> cb) {
    idle_callback = std::move(cb);
}

bool SfmlBattleView::route_action_click(float x, float y, BattlePresenter& presenter) {
    constexpr float kPressedFlashSeconds = 0.15f;

    for (ActionSlot& slot : action_slots) {
        if (!slot.bounds.contains({x, y})) continue;
        slot.pressed_seconds_left = kPressedFlashSeconds;
        switch (slot.kind) {
            case ActionKind::Wait:       presenter.on_wait_clicked();   break;
            case ActionKind::Defend:     presenter.on_defend_clicked(); break;
            // Spellbook / AutoCombat / Surrender land later in their own
            // phases; for now they're inert hit zones so the player gets
            // hover feedback without surprising side effects.
            case ActionKind::Spellbook:
            case ActionKind::AutoCombat:
            case ActionKind::Surrender:
                show_message("Action not yet implemented");
                break;
        }
        return true;
    }
    return false;
}

void SfmlBattleView::set_cursor_style(CursorStyle style, int pixel_x, int pixel_y) {
    cursor_style = style;
    cursor_position = {static_cast<float>(pixel_x), static_cast<float>(pixel_y)};

    // Only push the OS visibility flag on transitions: X11's XDefineCursor is
    // a synchronous server roundtrip, and calling it on every MouseMoved
    // event was the cause of the in-game input lag.
    const bool want_visible = (style == CursorStyle::Default);
    if (want_visible != os_cursor_visible) {
        window.setMouseCursorVisible(want_visible);
        os_cursor_visible = want_visible;
    }
}

void SfmlBattleView::show_unit_info_panel(const UnitRenderData& unit_data) {
    info_panel_visible = true;
    info_panel_unit = unit_data;
}

void SfmlBattleView::hide_unit_info_panel() {
    info_panel_visible = false;
    info_panel_unit.reset();
}

sf::Vector2f SfmlBattleView::unit_render_center(const UnitRenderData& unit) const {
    return unit_render_center(unit, unit.q, unit.r);
}

sf::Vector2f SfmlBattleView::unit_render_center(const UnitRenderData& unit, int q, int r) const {
    const sf::Vector2f head = hex_to_pixel(q, r);
    if (unit.size != 2) return head;

    // 2-hex units occupy a tail hex extending opposite the facing direction:
    //   facing left  → tail at (q+1, r)
    //   facing right → tail at (q-1, r)
    const int tail_dq = unit.is_facing_left ? 1 : -1;
    const sf::Vector2f tail = hex_to_pixel(q + tail_dq, r);
    return {(head.x + tail.x) * 0.5f, (head.y + tail.y) * 0.5f};
}

const UnitRenderData* SfmlBattleView::find_unit_render_data(std::uint64_t id) const {
    for (const UnitRenderData& unit : units_to_draw) {
        if (unit.id == id) return &unit;
    }
    for (const UnitRenderData& unit : model_units_latest) {
        if (unit.id == id) return &unit;
    }
    return nullptr;
}

void SfmlBattleView::handle_corpse_state_transition(const UnitRenderData& unit,
                                                    AnimationController& controller) {
    if (!unit.is_corpse) {
        corpse_frozen_ids.erase(unit.id);
        return;
    }

    if (corpse_frozen_ids.count(unit.id) == 0) {
        controller.set_animation_state(AnimState::Death, false, true);
        corpse_frozen_ids.insert(unit.id);
    }
}

void SfmlBattleView::apply_current_render_data_to_controllers(bool reset_standing_anim) {
    std::unordered_set<std::uint64_t> present_ids;

    for (const UnitRenderData& unit : units_to_draw) {
        present_ids.insert(unit.id);
        if (unit.asset_filename.empty()) {
            continue;
        }

        std::shared_ptr<DefResource> resource = def_manager.get_or_load(unit.asset_filename);
        if (!resource) {
            show_message("DEF not found or failed to parse: " + unit.asset_filename);
            continue;
        }

        sf::Vector2f center = unit_render_center(unit);
        if (const auto it = visual_position_overrides.find(unit.id); it != visual_position_overrides.end()) {
            center = it->second;
        }
        const float scale = compute_scale(*resource, unit.size, hex_radius, unit.asset_filename);

        auto ctrl_it = animation_controllers.find(unit.id);
        if (ctrl_it == animation_controllers.end()) {
            AnimationController controller(resource, static_cast<int>(AnimState::Stand));
            controller.set_hex_center(center);
            controller.set_facing_left(unit.visual_facing_left);
            controller.set_scale(scale);
            if (unit.is_corpse) {
                controller.set_animation_state(AnimState::Death, false, true);
                corpse_frozen_ids.insert(unit.id);
            } else {
                controller.set_animation_state(AnimState::Stand, true, true);
            }
            animation_controllers.emplace(unit.id, std::move(controller));
            controller_asset_files[unit.id] = unit.asset_filename;
            continue;
        }

        AnimationController& controller = ctrl_it->second;
        const auto asset_it = controller_asset_files.find(unit.id);
        if (asset_it == controller_asset_files.end() || asset_it->second != unit.asset_filename) {
            controller.set_resource(resource);
            controller_asset_files[unit.id] = unit.asset_filename;
        }
        controller.set_hex_center(center);
        controller.set_facing_left(unit.visual_facing_left);
        controller.set_scale(scale);

        if (unit.is_corpse) {
            handle_corpse_state_transition(unit, controller);
        } else if (reset_standing_anim || controller.get_animation_state() == AnimState::Death) {
            controller.set_animation_state(AnimState::Stand, true, true);
        }
    }

    for (auto it = animation_controllers.begin(); it != animation_controllers.end();) {
        if (present_ids.count(it->first) == 0) {
            corpse_frozen_ids.erase(it->first);
            visual_position_overrides.erase(it->first);
            controller_asset_files.erase(it->first);
            it = animation_controllers.erase(it);
        } else {
            ++it;
        }
    }
}

void SfmlBattleView::refresh_expanded_highlights() {
    // Layered build: each later layer wins on key collisions, so the priority
    // is base < 2-hex tail mirroring < hover preview < active unit.  The
    // active unit highlight thus *cannot* be overwritten by hover or walkable
    // tints — addressing the "tail randomly drops" symptom in Issue #4.
    expanded_highlights = highlights;

    // 1. Mirror highlights onto the tail hex of every 2-hex unit on the board
    //    so attackable / walkable tints cover both halves of the footprint.
    for (const UnitRenderData& unit : units_to_draw) {
        if (unit.size != 2) continue;
        const std::int64_t head_key = make_hex_key(unit.q, unit.r);
        const auto hit = highlights.find(head_key);
        if (hit == highlights.end()) continue;
        const int tail_dq = unit.is_facing_left ? 1 : -1;
        expanded_highlights[make_hex_key(unit.q + tail_dq, unit.r)] = hit->second;
    }

    // 2. Hover destination overlay (head + predicted tail) — uses the dark
    // 2a. Attack-origin overlays (all valid post-attack positions).
    for (const AttackOriginHex& origin : attack_origin_highlights) {
        expanded_highlights[make_hex_key(origin.q, origin.r)] = HighlightType::AttackOrigin;
        if (origin.has_tail) {
            expanded_highlights[make_hex_key(origin.tail_q, origin.tail_r)] = HighlightType::AttackOrigin;
        }
    }

    // 2b. Hover destination overlay (head + predicted tail) — uses the dark
    //    HoverDestination tint so it stands out from the lighter Walkable
    //    base layer that already covers every reachable hex.
    if (hover_destination_highlight.has_value()) {
        const HoverDestinationHighlight& hover = *hover_destination_highlight;
        expanded_highlights[make_hex_key(hover.q, hover.r)] = HighlightType::HoverDestination;
        if (hover.has_tail) {
            expanded_highlights[make_hex_key(hover.tail_q, hover.tail_r)] = HighlightType::HoverDestination;
        }
    }

    // 3. Active unit ALWAYS wins — head AND tail rendered every frame, fully
    //    decoupled from hover state.  This is the "always reliably highlighted
    //    every frame" requirement from the spec.
    if (active_unit_highlight.has_value()) {
        const ActiveUnitHighlight& active = *active_unit_highlight;
        expanded_highlights[make_hex_key(active.q, active.r)] = HighlightType::ActiveUnit;
        if (active.size == 2) {
            const int tail_dq = active.is_facing_left ? 1 : -1;
            expanded_highlights[make_hex_key(active.q + tail_dq, active.r)] = HighlightType::ActiveUnit;
        }
    }
}

void SfmlBattleView::update_hover_from_mouse() {
    // Presenter drives hover while previewing attack origins; keep its chosen
    // destination marker stable even when the cursor is stationary.
    if (!attack_origin_highlights.empty()) {
        return;
    }

    // During animations input is blocked; hide any stale hover highlight.
    if (has_pending_visual_events() || !active_unit_highlight.has_value()) {
        if (hover_destination_highlight.has_value()) {
            hover_destination_highlight.reset();
            refresh_expanded_highlights();
        }
        return;
    }

    // Reuse the per-frame mouse position cached by render() — querying the OS
    // mouse is an X11 roundtrip and we already paid for one this frame.
    const auto [hover_q, hover_r] = pixel_to_hex(cursor_position.x, cursor_position.y);
    const std::int64_t hover_key  = make_hex_key(hover_q, hover_r);

    // Hex must be walkable.  We deliberately ignore highlight types other than
    // Walkable here so attack-range tinting can't fool the destination preview.
    const auto hit = highlights.find(hover_key);
    if (hit == highlights.end() || hit->second != HighlightType::Walkable) {
        if (hover_destination_highlight.has_value()) {
            hover_destination_highlight.reset();
            refresh_expanded_highlights();
        }
        return;
    }

    // ── Predicted final facing (Issue #2/#4) ───────────────────────────────
    // Preferred: presenter-published `predicted_facing_by_hex`, derived from
    // the actual reconstructed path's last segment (second-to-last → last
    // hex).  Fallback: simple destination-vs-current-X comparison.
    bool future_facing_left = active_unit_highlight->is_facing_left;
    if (const auto pf = predicted_facing_by_hex.find(hover_key);
        pf != predicted_facing_by_hex.end()) {
        future_facing_left = pf->second;
    } else {
        const sf::Vector2f active_px = hex_to_pixel(active_unit_highlight->q, active_unit_highlight->r);
        const sf::Vector2f dest_px   = hex_to_pixel(hover_q, hover_r);
        if      (dest_px.x < active_px.x - 1.0f) future_facing_left = true;
        else if (dest_px.x > active_px.x + 1.0f) future_facing_left = false;
    }

    HoverDestinationHighlight new_hover;
    new_hover.q = hover_q;
    new_hover.r = hover_r;
    if (active_unit_highlight->size == 2) {
        new_hover.has_tail = true;
        new_hover.tail_q   = hover_q + (future_facing_left ? 1 : -1);
        new_hover.tail_r   = hover_r;
    }

    if (hover_destination_highlight.has_value()
        && hover_destination_highlight->q        == new_hover.q
        && hover_destination_highlight->r        == new_hover.r
        && hover_destination_highlight->has_tail == new_hover.has_tail
        && hover_destination_highlight->tail_q   == new_hover.tail_q
        && hover_destination_highlight->tail_r   == new_hover.tail_r) {
        return;
    }

    hover_destination_highlight = new_hover;
    refresh_expanded_highlights();
}

void SfmlBattleView::update_visual_events(sf::Time dt) {
    if (visual_events.empty()) {
        return;
    }

    VisualEvent& event = visual_events.front();
    if (!event.started) {
        process_visual_event_start();
        event.started = true;
    }

    bool finished = false;
    switch (event.type) {
        // ── Move ──────────────────────────────────────────────────────────────
        case VisualEvent::Type::Move: {
            event.move.elapsed_seconds += dt.asSeconds();
            if (!event.move.is_teleporter) {
                const float t = std::clamp(event.move.elapsed_seconds / event.move.duration_seconds, 0.0f, 1.0f);
                const sf::Vector2f pos = {
                    event.move.from.x + (event.move.to.x - event.move.from.x) * t,
                    event.move.from.y + (event.move.to.y - event.move.from.y) * t,
                };
                visual_position_overrides[event.move.unit_id] = pos;

                if (auto it = animation_controllers.find(event.move.unit_id); it != animation_controllers.end()) {
                    it->second.set_hex_center(pos);
                }

                finished = t >= 1.0f;
                break;
            }

            const float fade_out_seconds = std::max(0.08f, event.move.duration_seconds * 0.25f);
            const float hold_seconds     = std::max(0.05f, event.move.duration_seconds * 0.15f);
            const float fade_in_seconds  = std::max(0.08f, event.move.duration_seconds * 0.25f);

            auto ctrl_it = animation_controllers.find(event.move.unit_id);
            AnimationController* ctrl = (ctrl_it != animation_controllers.end()) ? &ctrl_it->second : nullptr;

            switch (event.move.phase) {
                case MoveVisualEvent::Phase::TeleportFadeOut: {
                    const float t = std::clamp(event.move.elapsed_seconds / fade_out_seconds, 0.0f, 1.0f);
                    if (ctrl) { ctrl->set_opacity(1.0f - t); ctrl->set_hex_center(event.move.from); }
                    visual_position_overrides[event.move.unit_id] = event.move.from;
                    if (t >= 1.0f) {
                        event.move.phase = MoveVisualEvent::Phase::TeleportHold;
                        event.move.elapsed_seconds = 0.0f;
                    }
                    break;
                }
                case MoveVisualEvent::Phase::TeleportHold: {
                    if (ctrl) ctrl->set_opacity(0.0f);
                    if (event.move.elapsed_seconds >= hold_seconds) {
                        event.move.phase = MoveVisualEvent::Phase::TeleportFadeIn;
                        event.move.elapsed_seconds = 0.0f;
                        visual_position_overrides[event.move.unit_id] = event.move.to;
                        if (ctrl) ctrl->set_hex_center(event.move.to);
                    }
                    break;
                }
                case MoveVisualEvent::Phase::TeleportFadeIn: {
                    const float t = std::clamp(event.move.elapsed_seconds / fade_in_seconds, 0.0f, 1.0f);
                    if (ctrl) { ctrl->set_opacity(t); ctrl->set_hex_center(event.move.to); }
                    visual_position_overrides[event.move.unit_id] = event.move.to;
                    finished = t >= 1.0f;
                    break;
                }
                case MoveVisualEvent::Phase::Slide:
                    break;
            }
            break;
        }

        // ── Attack ────────────────────────────────────────────────────────────
        // Completion is purely frame-based: wait until AnimationController
        // reports the one-shot Attack animation has played its last frame.
        // The safety_timeout only guards against missing/empty DEF groups.
        case VisualEvent::Type::Attack: {
            event.attack.elapsed_seconds += dt.asSeconds();
            const auto it = animation_controllers.find(event.attack.attacker_id);
            if (it != animation_controllers.end()) {
                finished = it->second.is_finished()
                           || event.attack.elapsed_seconds >= event.attack.safety_timeout;
            } else {
                finished = true;
            }
            break;
        }

        // ── Projectile ────────────────────────────────────────────────────────
        // Resolves on elapsed time only; the actual sprite drawing happens in
        // draw_units() so the projectile composites correctly above corpses
        // and below the HUD.
        case VisualEvent::Type::Projectile: {
            event.projectile.elapsed_seconds += dt.asSeconds();
            finished = event.projectile.elapsed_seconds >= event.projectile.duration_seconds;
            break;
        }

        // ── Morale aura ───────────────────────────────────────────────────────
        case VisualEvent::Type::Morale: {
            event.morale.elapsed_seconds += dt.asSeconds();
            finished = event.morale.elapsed_seconds >= event.morale.duration_seconds;
            break;
        }

        // ── Hit (TakeDamage / flinch) ─────────────────────────────────────────
        case VisualEvent::Type::Hit: {
            event.hit.elapsed_seconds += dt.asSeconds();
            const auto it = animation_controllers.find(event.hit.defender_id);
            if (it != animation_controllers.end()) {
                finished = it->second.is_finished()
                           || event.hit.elapsed_seconds >= event.hit.safety_timeout;
            } else {
                finished = true;
            }
            break;
        }

        // ── Death ─────────────────────────────────────────────────────────────
        // The Death animation is started by the preceding CommitRenderData event
        // (via apply_current_render_data_to_controllers).  This event just stalls
        // until the animation controller reports it has played the last frame.
        case VisualEvent::Type::Death: {
            event.death.elapsed_seconds += dt.asSeconds();
            const auto it = animation_controllers.find(event.death.unit_id);
            if (it != animation_controllers.end()) {
                finished = it->second.is_finished()
                           || event.death.elapsed_seconds >= event.death.safety_timeout;
            } else {
                finished = true;
            }
            break;
        }

        // ── CommitRenderData ──────────────────────────────────────────────────
        case VisualEvent::Type::CommitRenderData: {
            finished = true;
            break;
        }
    }

    if (finished) {
        process_visual_event_finish();
        visual_events.pop_front();

        // Resolve any zero-duration commit events immediately so that the state
        // they encode (e.g. a unit turning into a corpse) is applied in the same
        // frame and subsequent Death events can start checking is_finished() at
        // the very next tick.
        while (!visual_events.empty() && visual_events.front().type == VisualEvent::Type::CommitRenderData) {
            process_visual_event_start();
            process_visual_event_finish();
            visual_events.pop_front();
        }

        // Queue just drained → fire the one-shot idle callback (if any).
        // We move it out before invoking so the callback can install a new
        // one without losing it to the local clear.
        if (visual_events.empty() && idle_callback) {
            std::function<void()> cb = std::move(idle_callback);
            idle_callback = nullptr;
            cb();
        }
    }
}

void SfmlBattleView::process_visual_event_start() {
    if (visual_events.empty()) return;

    VisualEvent& event = visual_events.front();
    switch (event.type) {
        case VisualEvent::Type::Move: {
            event.move.elapsed_seconds = 0.0f;
            if (auto it = animation_controllers.find(event.move.unit_id); it != animation_controllers.end()) {
                if (event.move.is_teleporter) {
                    event.move.phase = MoveVisualEvent::Phase::TeleportFadeOut;
                    it->second.set_animation_state(AnimState::Stand, true, true);
                    it->second.set_opacity(1.0f);
                } else {
                    event.move.phase = MoveVisualEvent::Phase::Slide;
                    it->second.set_animation_state(AnimState::Move, true, true);

                    // ── Per-segment facing (Issue #1, walking) ────────────
                    // For multi-hop paths the presenter queues one MoveEvent
                    // per hop; here we flip the sprite for THIS segment based
                    // on the segment's own (from→to), so a C-shaped path
                    // mirrors the unit correctly at every turn.
                    constexpr float kFlipDeadZone = 1.0f;
                    if (event.move.to.x < event.move.from.x - kFlipDeadZone) {
                        it->second.set_facing_left(true);
                    } else if (event.move.to.x > event.move.from.x + kFlipDeadZone) {
                        it->second.set_facing_left(false);
                    }
                }
            }
            visual_position_overrides[event.move.unit_id] = event.move.from;
            break;
        }
        case VisualEvent::Type::Attack: {
            if (auto it = animation_controllers.find(event.attack.attacker_id); it != animation_controllers.end()) {
                // ── Pre-attack facing (Issue #1, attacking/retaliating) ──
                // If a target hex was supplied, rotate the attacker toward it
                // *before* starting the swing, comparing pixel X of attacker
                // vs target hex.  Same comparison used for retaliation, so
                // the defender turns toward the attacker even if the attacker
                // is now standing on the defender's previous side.
                if (event.attack.has_target_hex) {
                    const sf::Vector2f tgt_px = hex_to_pixel(event.attack.target_q, event.attack.target_r);
                    const sf::Vector2f own_px = it->second.get_sprite()
                        ? it->second.get_sprite()->getPosition()
                        : tgt_px;
                    constexpr float kFlipDeadZone = 1.0f;
                    if (tgt_px.x < own_px.x - kFlipDeadZone)      it->second.set_facing_left(true);
                    else if (tgt_px.x > own_px.x + kFlipDeadZone) it->second.set_facing_left(false);
                }
                it->second.set_animation_state(AnimState::Attack, false, true);
            }
            break;
        }
        case VisualEvent::Type::Projectile: {
            // Snapshot the live attacker position when the event actually
            // starts (the queue may have been built before the attacker's
            // sprite moved into its final pose).
            if (auto it = animation_controllers.find(event.projectile.attacker_id); it != animation_controllers.end()) {
                if (const sf::Sprite* s = it->second.get_sprite()) {
                    event.projectile.from = s->getPosition();
                }
            }
            break;
        }
        case VisualEvent::Type::Morale: {
            // No state to set up — duration is fixed, frame index is derived
            // from elapsed in draw_units().
            break;
        }
        case VisualEvent::Type::Hit: {
            if (auto it = animation_controllers.find(event.hit.defender_id); it != animation_controllers.end()) {
                it->second.set_animation_state(AnimState::TakeDamage, false, true);
            }
            break;
        }
        case VisualEvent::Type::Death: {
            // The Death animation was already started by the CommitRenderData event that
            // preceded this one (handle_corpse_state_transition).  Nothing to do here.
            break;
        }
        case VisualEvent::Type::CommitRenderData: {
            // No-op: commit happens in finish to keep ordering deterministic.
            break;
        }
    }
}

void SfmlBattleView::process_visual_event_finish() {
    if (visual_events.empty()) return;

    VisualEvent& event = visual_events.front();
    switch (event.type) {
        case VisualEvent::Type::Move: {
            visual_position_overrides[event.move.unit_id] = event.move.to;
            if (auto it = animation_controllers.find(event.move.unit_id); it != animation_controllers.end()) {
                it->second.set_opacity(1.0f);
                it->second.set_hex_center(event.move.to);
                it->second.set_animation_state(AnimState::Stand, true, true);
            }
            break;
        }
        case VisualEvent::Type::Attack: {
            // One-shot animation done — smoothly return attacker to idle.
            if (auto it = animation_controllers.find(event.attack.attacker_id); it != animation_controllers.end()) {
                it->second.set_animation_state(AnimState::Stand, true, true);
            }
            break;
        }
        case VisualEvent::Type::Projectile: {
            // No state to clean up — sprite stops being drawn once popped.
            break;
        }
        case VisualEvent::Type::Morale: {
            // No-op; the aura sprite is drawn directly from event state.
            break;
        }
        case VisualEvent::Type::Hit: {
            // Flinch done — return unit to idle (unless it will become a corpse
            // on the next CommitRenderData, which will override this).
            if (auto it = animation_controllers.find(event.hit.defender_id); it != animation_controllers.end()) {
                it->second.set_animation_state(AnimState::Stand, true, true);
            }
            break;
        }
        case VisualEvent::Type::Death: {
            // Nothing to do: the controller is already frozen on the last death
            // frame (freeze_on_last_frame=true set by handle_corpse_state_transition).
            break;
        }
        case VisualEvent::Type::CommitRenderData: {
            units_to_draw = event.commit.units;
            model_units_latest = event.commit.units;
            visual_position_overrides.clear();
            apply_current_render_data_to_controllers(false);
            refresh_expanded_highlights();
            break;
        }
    }
}

sf::Vector2f SfmlBattleView::hex_to_pixel(int q, int r) const {
    // Pointy-top axial -> pixel conversion.
    const float x = hex_radius * (std::sqrt(3.0f) * (static_cast<float>(q) + static_cast<float>(r) * 0.5f));
    const float y = hex_radius * (1.5f * static_cast<float>(r));
    return {grid_origin.x + x, grid_origin.y + y};
}

std::pair<int, int> SfmlBattleView::pixel_to_hex(float x, float y) const {
    // Pixel -> axial for pointy-top layout.
    const float px = x - grid_origin.x;
    const float py = y - grid_origin.y;

    const float fq = (std::sqrt(3.0f) / 3.0f * px - 1.0f / 3.0f * py) / hex_radius;
    const float fr = (2.0f / 3.0f * py) / hex_radius;
    const float fs = -fq - fr;

    return cube_round_to_axial(fq, fr, fs);
}

sf::ConvexShape SfmlBattleView::make_hex_shape(int q, int r) const {
    sf::ConvexShape shape(6);

    const sf::Vector2f center = hex_to_pixel(q, r);
    for (int i = 0; i < 6; ++i) {
        const float angle = (60.0f * static_cast<float>(i) - 30.0f) * (kPi / 180.0f);
        const float vx = center.x + hex_radius * std::cos(angle);
        const float vy = center.y + hex_radius * std::sin(angle);
        shape.setPoint(i, {vx, vy});
    }

    return shape;
}

std::int64_t SfmlBattleView::make_hex_key(int q, int r) {
    return (static_cast<std::int64_t>(q) << 32) ^ (static_cast<std::int64_t>(r) & 0xffffffffLL);
}

bool SfmlBattleView::is_point_in_battlefield(float x, float y) const {
    return x >= 0.0f && x <= screen_width && y >= 0.0f && y < battlefield_height;
}

