/**
 * @file AnimationController.h
 * @brief Per-unit sprite animation state machine.
 */
#pragma once

#include <SFML/Graphics.hpp>
#include <SFML/System.hpp>

#include "DefParser.h"

#include <memory>

namespace views {

/**
 * @brief Possible animation states a unit's sprite can be in.
 */
enum class AnimState : int {
    MOVE = 0,
    STAND = 1,
    TAKE_DAMAGE = 4,
    DEATH = 5,
    FIDGET = 11,
    ATTACK = 12
};

/**
 * @brief Drives a single unit's sprite through animation states.
 *
 * Wraps a parsed DefResource and exposes simple state transitions
 * (Stand, Move, Attack, Death, Fidget...). The presenter / view
 * does not deal with frame indices; it just sets the high-level state.
 */
class AnimationController {
public:
    AnimationController( ) = default;
    AnimationController( std::shared_ptr<DefResource> resource, int initial_group = 1 );

    void setResource( std::shared_ptr<DefResource> resource );
    void setAnimationGroup( int group_id );
    void setAnimationState( AnimState state, bool loop = true, bool freeze_on_last_frame = true );
    void setHexCenter( const sf::Vector2f& center );
    void setFacingLeft( bool facing_left );
    void setScale( float scale );
    void setOpacity( float alpha_0_to_1 );
    void resetToFirstFrame( );
    void setFps( float new_fps );

    void update( sf::Time delta_time );

    [[nodiscard]] const sf::Sprite* getSprite( ) const;
    [[nodiscard]] bool isReady( ) const;
    [[nodiscard]] bool isFinished( ) const;
    [[nodiscard]] AnimState getAnimationState( ) const;
    [[nodiscard]] int getGroupId( ) const;

private:
    void applyCurrentFrame( );
    [[nodiscard]] const std::vector<DefFrame>* findGroup( ) const;
    [[nodiscard]] static float fpsForState( AnimState state, float base_fps );
    void scheduleNextFidget( );
    void maybeTriggerFidget( sf::Time delta_time );

    std::shared_ptr<DefResource> resource_;
    int groupId_ = 1;
    std::size_t frameIndex_ = 0;
    float frameAccumulator_ = 0.0f;

    float baseFps_ = 5.0f;
    float fps_ = 5.0f;
    float scale_ = 1.0f;
    AnimState animState_ = AnimState::STAND;
    bool loop_ = true;
    bool freezeOnLastFrame_ = true;
    bool finished_ = false;

    float fidgetCooldown_ = 0.0f;

    sf::Vector2f hexCenter_ = { 0.0f, 0.0f };
    bool facingLeft_ = false;
    float opacity_ = 1.0f;

    std::unique_ptr<sf::Sprite> sprite_;
};

} // namespace views
