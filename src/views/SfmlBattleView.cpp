#include "SfmlBattleView.hpp"

#include "../models/board.hpp"
#include "../presenters/BattlePresenter.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cctype>
#include <fstream>
#include <iostream>
#include <limits>
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

float cursor_angle_deg(CursorStyle style) {
    switch (style) {
        case CursorStyle::SwordE: return 0.0f;
        case CursorStyle::SwordNE: return -60.0f;
        case CursorStyle::SwordNW: return -120.0f;
        case CursorStyle::SwordW: return 180.0f;
        case CursorStyle::SwordSW: return 120.0f;
        case CursorStyle::SwordSE: return 60.0f;
        case CursorStyle::Default: return 0.0f;
    }
    return 0.0f;
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

    // HUD panel occupies the bottom 20% of the screen.
    hud_background.setPosition({0.0f, battlefield_height});
    hud_background.setSize({screen_width, screen_height - battlefield_height});
    hud_background.setFillColor(sf::Color(36, 36, 42));

    // Action buttons: stacked vertically against the right edge of the HUD.
    // Defend sits ABOVE Wait, both aligned to a 30 px right margin.
    const float btn_w           = 120.0f;
    const float btn_h           = 42.0f;
    const float btn_right_x     = screen_width - btn_w - 30.0f;
    const float btn_defend_y    = battlefield_height + 14.0f;
    const float btn_wait_y      = btn_defend_y + btn_h + 10.0f;

    defend_button.setPosition({btn_right_x, btn_defend_y});
    defend_button.setSize({btn_w, btn_h});
    defend_button.setFillColor(sf::Color(90, 90, 100));

    wait_button.setPosition({btn_right_x, btn_wait_y});
    wait_button.setSize({btn_w, btn_h});
    wait_button.setFillColor(sf::Color(90, 90, 100));

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
    wait_text = std::make_unique<sf::Text>(font);
    defend_text = std::make_unique<sf::Text>(font);
    info_panel_text = std::make_unique<sf::Text>(font);

    hud_text->setCharacterSize(18);
    hud_text->setFillColor(sf::Color::White);
    hud_text->setPosition({350.0f, battlefield_height + 16.0f});

    queue_text->setCharacterSize(16);
    queue_text->setFillColor(sf::Color(230, 230, 230));
    queue_text->setPosition({350.0f, battlefield_height + 44.0f});

    log_text->setCharacterSize(17);
    log_text->setFillColor(sf::Color(220, 220, 220));
    log_text->setPosition({350.0f, battlefield_height + 74.0f});

    defend_text->setCharacterSize(18);
    defend_text->setFillColor(sf::Color::White);
    defend_text->setString("Defend");
    defend_text->setPosition({btn_right_x + 30.0f, btn_defend_y + 9.0f});

    wait_text->setCharacterSize(18);
    wait_text->setFillColor(sf::Color::White);
    wait_text->setString("Wait");
    wait_text->setPosition({btn_right_x + 42.0f, btn_wait_y + 9.0f});

    // Dedicated DEF manager keeps resources alive while sprites reference frame textures.
    def_manager.set_units_root("assets/units");

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

    // Right-click info panel style.
    info_panel_background.setSize({300.0f, 210.0f});
    info_panel_background.setFillColor(sf::Color(20, 20, 24, 240));
    info_panel_background.setOutlineColor(sf::Color(175, 175, 195));
    info_panel_background.setOutlineThickness(2.0f);

    info_panel_text->setCharacterSize(16);
    info_panel_text->setFillColor(sf::Color::White);
}

bool SfmlBattleView::is_open() const {
    return window.isOpen();
}

void SfmlBattleView::on_mouse_hover(int pixel_x, int pixel_y, BattlePresenter& presenter) {
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

            if (wait_button.getGlobalBounds().contains({mx, my})) {
                presenter.on_wait_clicked();
                continue;
            }

            if (defend_button.getGlobalBounds().contains({mx, my})) {
                presenter.on_defend_clicked();
                continue;
            }

            if (is_point_in_battlefield(mx, my)) {
                const auto [q, r] = pixel_to_hex(mx, my);
                presenter.on_hex_clicked(q, r);
            } else {
                presenter.on_right_click_released();
            }
            continue;
        }

        if (const auto* mouseRelease = event->getIf<sf::Event::MouseButtonReleased>()) {
            if (mouseRelease->button == sf::Mouse::Button::Right) {
                presenter.on_right_click_released();
            }
        }
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

    // Recompute hover destination every frame from the actual mouse position so
    // highlights stay correct even when the game state changes without a new
    // MouseMoved event (e.g. after a turn ends while the cursor is stationary).
    update_hover_from_mouse();

    draw_battlefield_background();
    draw_hex_grid();
    draw_units();
    draw_hud();
    draw_turn_queue();
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
        if (!unit.is_corpse) draw_one(unit);
    }
}

void SfmlBattleView::draw_hud() {
    window.draw(hud_background);
    window.draw(defend_button);
    window.draw(wait_button);
    window.draw(*defend_text);
    window.draw(*wait_text);

    hud_text->setString("Unit: " + hud_unit_name +
                        " | Count: "   + std::to_string(hud_count) +
                        " | HP Left: " + std::to_string(hud_hp_left));
    log_text->setString("Log: " + latest_message);

    window.draw(*hud_text);
    window.draw(*log_text);
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
    const float queue_y       = screen_height - box_h - 12.0f;
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

void SfmlBattleView::draw_info_panel() {
    if (!info_panel_visible || !info_panel_unit.has_value()) return;

    const UnitRenderData& u = *info_panel_unit;
    const sf::Vector2f center = hex_to_pixel(u.q, u.r);
    sf::Vector2f panel_pos = {center.x + 20.0f, center.y - 90.0f};

    // Keep the panel inside the window.
    panel_pos.x = std::clamp(panel_pos.x, 10.0f,
                             screen_width  - info_panel_background.getSize().x - 10.0f);
    panel_pos.y = std::clamp(panel_pos.y, 10.0f,
                             screen_height - info_panel_background.getSize().y - 10.0f);

    info_panel_background.setPosition(panel_pos);

    std::ostringstream panel;
    panel << u.name << (u.is_corpse ? " [Corpse]" : "") << "\n"
          << "Count: "      << u.count            << "\n"
          << "HP Left: "    << u.hp_left          << "\n"
          << "Attack: "     << u.base_attack      << " (" << u.total_attack      << ")\n"
          << "Defense: "    << u.base_defense     << " (" << u.total_defense     << ")\n"
          << "Speed: "      << u.base_speed       << " (" << u.total_speed       << ")\n"
          << "Damage Min: " << u.base_damage_min  << " (" << u.total_damage_min  << ")\n"
          << "Damage Max: " << u.base_damage_max  << " (" << u.total_damage_max  << ")";
    if (!u.description.empty()) {
        panel << "\n\n" << u.description;
    }

    info_panel_text->setString(panel.str());
    info_panel_text->setPosition({panel_pos.x + 12.0f, panel_pos.y + 10.0f});

    window.draw(info_panel_background);
    window.draw(*info_panel_text);
}

void SfmlBattleView::draw_cursor() {
    if (cursor_style == CursorStyle::Default) return;

    const float angle = cursor_angle_deg(cursor_style);

    sf::RectangleShape blade({30.0f, 4.0f});
    blade.setOrigin({6.0f, 2.0f});
    blade.setPosition(cursor_position);
    blade.setRotation(sf::degrees(angle));
    blade.setFillColor(sf::Color(220, 220, 230));

    sf::RectangleShape handle({8.0f, 6.0f});
    handle.setOrigin({4.0f, 3.0f});
    handle.setPosition(cursor_position);
    handle.setRotation(sf::degrees(angle));
    handle.setFillColor(sf::Color(110, 80, 40));

    window.draw(blade);
    window.draw(handle);
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

void SfmlBattleView::set_cursor_style(CursorStyle style, int pixel_x, int pixel_y) {
    cursor_style = style;
    cursor_position = {static_cast<float>(pixel_x), static_cast<float>(pixel_y)};

    // Hide OS cursor only while rendering custom sword cursor.
    window.setMouseCursorVisible(style == CursorStyle::Default);
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
    // During animations input is blocked; hide any stale hover highlight.
    if (has_pending_visual_events() || !active_unit_highlight.has_value()) {
        if (hover_destination_highlight.has_value()) {
            hover_destination_highlight.reset();
            refresh_expanded_highlights();
        }
        return;
    }

    // Use mapPixelToCoords so the result is correct regardless of any view
    // transform that SFML applies to the window's coordinate system.
    const sf::Vector2i mouse_px   = sf::Mouse::getPosition(window);
    const sf::Vector2f mouse_world = window.mapPixelToCoords(mouse_px);
    const auto [hover_q, hover_r] = pixel_to_hex(mouse_world.x, mouse_world.y);
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

void SfmlBattleView::process_visual_event_update() {
    // Kept for future extension; update logic currently lives in update_visual_events.
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

void SfmlBattleView::debug_render_sanity_check() {
    // ── 1. Hardcoded file load ────────────────────────────────────────────────
    const std::filesystem::path def_path =
        "/home/dominik/Documents/zpr/assets/units/castle/CPKMAN.def";

    DefParser::debug_parse_file(def_path.string());
    std::cout << "[DBG] Absolute path being opened: " << def_path << "\n";

    {
        std::ifstream probe(def_path, std::ios::binary);
        std::cout << "[DBG] std::ifstream::is_open() = " << std::boolalpha << probe.is_open() << "\n";
        if (!probe.is_open()) {
            std::cout << "[DBG] FATAL: cannot open file — check CWD and asset path.\n";
            std::cout << "[DBG] CWD = " << std::filesystem::current_path() << "\n";
            return;
        }
    }

    std::shared_ptr<DefResource> resource;
    try {
        DefParser parser;
        resource = std::make_shared<DefResource>(parser.parse_file(def_path));
    } catch (const std::exception& e) {
        std::cout << "[DBG] DefParser::parse_file() threw: " << e.what() << "\n";
        return;
    }

    std::cout << "[DBG] Parsed OK — canvas " << resource->canvas_width
              << " x " << resource->canvas_height
              << ", groups: " << resource->groups.size() << "\n";

    for (const auto& [gid, frames] : resource->groups) {
        std::cout << "[DBG]   group " << gid << ": " << frames.size() << " frame(s)";
        if (!frames.empty())
            std::cout << "  [0] " << frames[0].width << "x" << frames[0].height
                      << "  off=(" << frames[0].offset_x << "," << frames[0].offset_y << ")";
        std::cout << "\n";
    }

    // Find the first group that has at least one frame with non-zero dimensions.
    const DefFrame* first_frame = nullptr;
    int group_used = -1;
    for (auto& [gid, frames] : resource->groups) {
        for (const DefFrame& f : frames) {
            if (f.width > 0 && f.height > 0) {
                first_frame = &f;
                group_used = gid;
                break;
            }
        }
        if (first_frame) break;
    }

    if (!first_frame) {
        std::cout << "[DBG] FATAL: no frame with valid dimensions found!\n";
        return;
    }

    std::cout << "[DBG] Using group=" << group_used
              << "  frame size=" << first_frame->width << "x" << first_frame->height << "\n";

    // ── 5. Texture transparency check (read back pixels via copyToImage) ──────
    {
        const sf::Image img = first_frame->texture.copyToImage();
        const unsigned int img_w = img.getSize().x;
        const unsigned int img_h = img.getSize().y;
        std::cout << "[DBG] copyToImage() size: " << img_w << "x" << img_h << "\n";

        bool all_transparent = true;
        const unsigned int pixels_to_check = std::min(100u, img_w * img_h);
        for (unsigned int i = 0; i < pixels_to_check; ++i) {
            if (img.getPixel({i % img_w, i / img_w}).a != 0) {
                all_transparent = false;
                break;
            }
        }
        if (all_transparent) {
            std::cout << "[DBG] WARNING: ENTIRE TEXTURE IS TRANSPARENT! "
                         "Parser may be producing wrong pixel data.\n";
        } else {
            std::cout << "[DBG] Texture has non-transparent pixels — data looks OK.\n";
        }

        // Print the RGBA of the first non-trivial pixel for a quick sanity number.
        for (unsigned int i = 0; i < img_w * img_h; ++i) {
            const sf::Color px = img.getPixel({i % img_w, i / img_w});
            if (px.a > 0) {
                std::cout << "[DBG] First opaque pixel [" << (i % img_w) << ","
                          << (i / img_w) << "] = rgba("
                          << static_cast<int>(px.r) << ","
                          << static_cast<int>(px.g) << ","
                          << static_cast<int>(px.b) << ","
                          << static_cast<int>(px.a) << ")\n";
                break;
            }
        }
    }

    // ── 2. "Ugly Debug" Magenta box (proves the render loop works) ───────────
    sf::RectangleShape debug_box({100.0f, 100.0f});
    debug_box.setFillColor(sf::Color::Magenta);
    debug_box.setPosition({400.0f, 300.0f});

    // ── 3. Sprite at the same position, no HoMM3 offsets applied ─────────────
    sf::Sprite debug_sprite(first_frame->texture);
    debug_sprite.setOrigin({0.0f, 0.0f});
    debug_sprite.setPosition({400.0f, 300.0f});

    // ── 4. Blocking debug render loop ────────────────────────────────────────
    std::cout << "[DBG] Entering debug render loop.\n"
              << "[DBG]   • Magenta box at (400,300) — always visible if SFML works.\n"
              << "[DBG]   • DEF sprite drawn on top — visible if parser+texture work.\n"
              << "[DBG]   • Press Escape to exit debug view and continue normally.\n"
              << "[DBG]   • Close window to terminate.\n";

    while (window.isOpen()) {
        while (const std::optional<sf::Event> ev = window.pollEvent()) {
            if (ev->is<sf::Event::Closed>()) {
                window.close();
                return;
            }
            if (const auto* kp = ev->getIf<sf::Event::KeyPressed>()) {
                if (kp->code == sf::Keyboard::Key::Escape) {
                    std::cout << "[DBG] Escape pressed — exiting debug view.\n";
                    return; // Window stays open; game continues normally.
                }
            }
        }

        window.clear(sf::Color(50, 50, 50));
        window.draw(debug_box);    // Magenta — always visible.
        window.draw(debug_sprite); // DEF frame — visible iff parser+texture OK.
        window.display();
    }
}
