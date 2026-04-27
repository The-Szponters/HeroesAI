#include "AnimationController.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>

namespace {
// Bottom margin (in canvas pixels) between the canvas's bottom edge and the
// hex centre.  HoMM3 frames already have built-in foot padding inside the
// 450×400 canvas; using a small constant here keeps creature feet exactly on
// the hex centre regardless of how much the upper-body crop changes between
// frames (which is what caused the Pikeman wobble).
constexpr float kCanvasFootPadding = 10.0f;

// Shared RNG for fidget jitter.  Static-local so it survives across calls but
// stays inside the controller TU.
std::mt19937& fidget_rng() {
    static std::mt19937 rng(std::random_device{}());
    return rng;
}
} // namespace

float AnimationController::fps_for_state(AnimState state, float base_fps) {
    switch (state) {
        case AnimState::Stand:      return base_fps * 0.85f;       // ~4.25 fps
        case AnimState::Move:
        case AnimState::TakeDamage:
        case AnimState::Death:
        case AnimState::Attack:     return base_fps * 4.0f;        // 20 fps — Phase 6 spec doubles the previous 2× tempo for snappier combat
        case AnimState::Fidget:     return base_fps * 1.0f;
    }
    return base_fps;
}

void AnimationController::schedule_next_fidget() {
    // Random idle interval between 5 and 10 seconds.
    std::uniform_real_distribution<float> dist(5.0f, 10.0f);
    fidget_cooldown = dist(fidget_rng());
}

AnimationController::AnimationController(std::shared_ptr<DefResource> resource, int initial_group) {
    set_resource(std::move(resource));
    set_animation_group(initial_group);
}

void AnimationController::set_resource(std::shared_ptr<DefResource> new_resource) {
    resource = std::move(new_resource);
    frame_index = 0;
    frame_accumulator = 0.0f;
    finished = false;
    schedule_next_fidget();
    apply_current_frame();
}

void AnimationController::set_animation_group(int new_group_id) {
    group_id = new_group_id;
    anim_state = static_cast<AnimState>(new_group_id);
    frame_index = 0;
    frame_accumulator = 0.0f;
    finished = false;
    loop = true;
    freeze_on_last_frame = true;
    fps = fps_for_state(anim_state, base_fps);
    apply_current_frame();
}

void AnimationController::set_animation_state(AnimState state, bool should_loop, bool should_freeze_on_last_frame) {
    // Bounce-back from Fidget completion is the only transition allowed to
    // skip the cooldown reset; for any explicit external state change we
    // restart the idle timer so a freshly-walking unit doesn't fidget the
    // moment it stops.
    if (state == AnimState::Stand) {
        schedule_next_fidget();
    }

    anim_state = state;
    group_id = static_cast<int>(state);
    loop = should_loop;
    freeze_on_last_frame = should_freeze_on_last_frame;
    frame_index = 0;
    frame_accumulator = 0.0f;
    finished = false;
    fps = fps_for_state(anim_state, base_fps);
    apply_current_frame();
}

void AnimationController::set_hex_center(const sf::Vector2f& center) {
    hex_center = center;
    apply_current_frame();
}

void AnimationController::set_facing_left(bool left) {
    facing_left = left;
    apply_current_frame();
}

void AnimationController::set_scale(float s) {
    scale = s;
    apply_current_frame();
}

void AnimationController::set_opacity(float alpha_0_to_1) {
    opacity = std::clamp(alpha_0_to_1, 0.0f, 1.0f);
    if (sprite) {
        const auto alpha = static_cast<std::uint8_t>(std::round(255.0f * opacity));
        sprite->setColor(sf::Color(255, 255, 255, alpha));
    }
}

void AnimationController::reset_to_first_frame() {
    frame_index = 0;
    frame_accumulator = 0.0f;
    finished = false;
    apply_current_frame();
}

void AnimationController::set_fps(float new_fps) {
    if (new_fps > 0.0f) {
        base_fps = new_fps;
        fps = fps_for_state(anim_state, base_fps);
    }
}

void AnimationController::maybe_trigger_fidget(sf::Time delta_time) {
    // Only consider while the unit is in its looping Stand animation; fidget
    // never preempts a one-shot animation in flight.
    if (anim_state != AnimState::Stand || !resource) return;

    fidget_cooldown -= delta_time.asSeconds();
    if (fidget_cooldown > 0.0f) return;

    // Roll over even when no Group 11 exists, so we don't hammer the check.
    schedule_next_fidget();

    const auto it = resource->groups.find(static_cast<int>(AnimState::Fidget));
    if (it == resource->groups.end() || it->second.empty()) return;

    // One-shot fidget; on completion it returns to Stand via the standard
    // freeze-on-last-frame path + an external transition the View triggers
    // when it observes is_finished() while in Fidget.
    set_animation_state(AnimState::Fidget, /*loop=*/false, /*freeze_on_last_frame=*/true);
}

void AnimationController::update(sf::Time delta_time) {
    if (resource == nullptr) return;

    // Detect Fidget→Stand transition: when a one-shot fidget completes, drop
    // straight back into the looping idle so the unit keeps breathing.
    if (anim_state == AnimState::Fidget && finished) {
        set_animation_state(AnimState::Stand, /*loop=*/true, /*freeze_on_last_frame=*/true);
        return;
    }

    maybe_trigger_fidget(delta_time);

    if (finished) return;

    const auto* group = find_group();
    if (!group || group->empty()) return;

    frame_accumulator += delta_time.asSeconds();
    const float frame_duration = 1.0f / fps;   // recomputed every tick — fps may have changed

    while (frame_accumulator >= frame_duration) {
        frame_accumulator -= frame_duration;

        if (loop) {
            // Use total_frames = group->size() and wrap [0 .. total-1].
            frame_index = (frame_index + 1) % group->size();
            apply_current_frame();
            continue;
        }

        if (frame_index + 1 < group->size()) {
            ++frame_index;
            apply_current_frame();
            continue;
        }

        finished = true;
        if (freeze_on_last_frame) {
            frame_index = group->size() - 1;
            apply_current_frame();
        }
        break;
    }
}

const sf::Sprite* AnimationController::get_sprite() const {
    return sprite.get();
}

bool AnimationController::is_ready() const {
    if (resource == nullptr) return false;
    const auto* group = find_group();
    return group != nullptr && !group->empty();
}

bool AnimationController::is_finished() const {
    return finished;
}

AnimState AnimationController::get_animation_state() const {
    return anim_state;
}

int AnimationController::get_group_id() const {
    return group_id;
}

const std::vector<DefFrame>* AnimationController::find_group() const {
    if (!resource) return nullptr;
    auto it = resource->groups.find(group_id);
    if (it != resource->groups.end() && !it->second.empty()) return &it->second;
    for (int fallback : {1, 0}) {
        auto fb = resource->groups.find(fallback);
        if (fb != resource->groups.end() && !fb->second.empty()) return &fb->second;
    }
    if (!resource->groups.empty()) return &resource->groups.begin()->second;
    return nullptr;
}

void AnimationController::apply_current_frame() {
    if (resource == nullptr) return;

    const auto* group = find_group();
    if (!group || group->empty()) return;

    const DefFrame& frame = (*group)[frame_index % group->size()];
    if (frame.canvas_width == 0 || frame.canvas_height == 0) return;

    if (!sprite) {
        sprite = std::make_unique<sf::Sprite>(frame.texture);
    } else {
        sprite->setTexture(frame.texture, true);
    }

    // ── Canvas-fixed anchor (the Pikeman wobble fix) ────────────────────────
    // Every frame in this DEF shares the same canvas (full_w × full_h) and the
    // resource exposes a per-DEF `feet_y` computed from the Stand group.  By
    // anchoring the sprite at *canvas* (canvas_w/2, feet_y) we guarantee that
    // the creature's feet stay glued to hex_center across every frame — the
    // upper body / weapon may extend or recoil inside the canvas without
    // shifting the unit's apparent position.
    const float canvas_w = static_cast<float>(frame.canvas_width);
    const float canvas_h = static_cast<float>(frame.canvas_height);
    float feet_x = static_cast<float>(resource->feet_x);
    float feet_y = static_cast<float>(resource->feet_y);
    if (feet_x <= 0.0f || feet_x > canvas_w) feet_x = canvas_w * 0.5f;
    if (feet_y <= 0.0f || feet_y > canvas_h) feet_y = canvas_h - kCanvasFootPadding;
    sprite->setOrigin({feet_x, feet_y});
    sprite->setPosition(hex_center);

    const float sx = facing_left ? -scale : scale;
    sprite->setScale({sx, scale});
    const auto alpha = static_cast<std::uint8_t>(std::round(255.0f * opacity));
    sprite->setColor(sf::Color(255, 255, 255, alpha));
}
