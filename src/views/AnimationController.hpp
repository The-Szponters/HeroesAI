#pragma once

#include <SFML/Graphics.hpp>
#include <SFML/System.hpp>

#include "DefParser.hpp"

#include <memory>

enum class AnimState : int {
    Move       = 0,
    Stand      = 1,
    TakeDamage = 4,   
    Death      = 5,
    Fidget     = 11,  
    Attack     = 12,
};

class AnimationController {
public:
    AnimationController() = default;
    AnimationController(std::shared_ptr<DefResource> resource, int initial_group = 1);

    void set_resource(std::shared_ptr<DefResource> resource);
    void set_animation_group(int group_id);
    void set_animation_state(AnimState state, bool loop = true, bool freeze_on_last_frame = true);
    void set_hex_center(const sf::Vector2f& center);
    void set_facing_left(bool facing_left);
    void set_scale(float scale);
    void set_opacity(float alpha_0_to_1);
    void reset_to_first_frame();
    void set_fps(float new_fps);

    void update(sf::Time delta_time);

    [[nodiscard]] const sf::Sprite* get_sprite() const;
    [[nodiscard]] bool is_ready() const;
    [[nodiscard]] bool is_finished() const;
    [[nodiscard]] AnimState get_animation_state() const;
    [[nodiscard]] int get_group_id() const;

private:
    void apply_current_frame();
    [[nodiscard]] const std::vector<DefFrame>* find_group() const;
    [[nodiscard]] static float fps_for_state(AnimState state, float base_fps);
    void schedule_next_fidget();
    void maybe_trigger_fidget(sf::Time delta_time);

    std::shared_ptr<DefResource> resource;
    int group_id = 1;
    std::size_t frame_index = 0;
    float frame_accumulator = 0.0f;

    float base_fps = 5.0f;
    float fps = 5.0f;
    float scale = 1.0f;
    AnimState anim_state = AnimState::Stand;
    bool loop = true;
    bool freeze_on_last_frame = true;
    bool finished = false;

    float fidget_cooldown = 0.0f;

    sf::Vector2f hex_center = {0.0f, 0.0f};
    bool facing_left = false;
    float opacity = 1.0f;

    std::unique_ptr<sf::Sprite> sprite;
};
