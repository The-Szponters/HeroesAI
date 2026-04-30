/**
 * @file AnimationController.cc
 * @brief Implementation of the unit-sprite animation state machine.
 * @author Dominik Śledziewski
 */
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>

#include "AnimationController.h"

namespace views {

namespace {

constexpr float K_CANVAS_FOOT_PADDING = 10.0f;

std::mt19937& fidgetRng( ) {
    static std::mt19937 rng( std::random_device{ }( ) );
    return rng;
}
} // namespace

float AnimationController::fpsForState( AnimState state, float base_fps ) {
    switch ( state ) {
    case AnimState::STAND:
        return base_fps * 0.85f;
    case AnimState::MOVE:
    case AnimState::TAKE_DAMAGE:
    case AnimState::DEATH:
    case AnimState::ATTACK:
        return base_fps * 2.0f;
    case AnimState::FIDGET:
        return base_fps * 1.0f;
    }
    return base_fps;
}

void AnimationController::scheduleNextFidget( ) {
    std::uniform_real_distribution<float> dist( 5.0f, 10.0f );
    fidgetCooldown_ = dist( fidgetRng( ) );
}

AnimationController::AnimationController( std::shared_ptr<DefResource> resource,
                                          int initial_group ) {
    setResource( std::move( resource ) );
    setAnimationGroup( initial_group );
}

void AnimationController::setResource( std::shared_ptr<DefResource> new_resource ) {
    resource_ = std::move( new_resource );
    frameIndex_ = 0;
    frameAccumulator_ = 0.0f;
    finished_ = false;
    scheduleNextFidget( );
    applyCurrentFrame( );
}

void AnimationController::setAnimationGroup( int new_group_id ) {
    groupId_ = new_group_id;
    animState_ = static_cast<AnimState>( new_group_id );
    frameIndex_ = 0;
    frameAccumulator_ = 0.0f;
    finished_ = false;
    loop_ = true;
    freezeOnLastFrame_ = true;
    fps_ = fpsForState( animState_, baseFps_ );
    applyCurrentFrame( );
}

void AnimationController::setAnimationState( AnimState state,
                                               bool should_loop,
                                               bool should_freeze_on_last_frame ) {
    if ( state == AnimState::STAND ) {
        scheduleNextFidget( );
    }

    animState_ = state;
    groupId_ = static_cast<int>( state );
    loop_ = should_loop;
    freezeOnLastFrame_ = should_freeze_on_last_frame;
    frameIndex_ = 0;
    frameAccumulator_ = 0.0f;
    finished_ = false;
    fps_ = fpsForState( animState_, baseFps_ );
    applyCurrentFrame( );
}

void AnimationController::setHexCenter( const sf::Vector2f& center ) {
    hexCenter_ = center;
    applyCurrentFrame( );
}

void AnimationController::setFacingLeft( bool left ) {
    facingLeft_ = left;
    applyCurrentFrame( );
}

void AnimationController::setScale( float s ) {
    scale_ = s;
    applyCurrentFrame( );
}

void AnimationController::setOpacity( float alpha_0_to_1 ) {
    opacity_ = std::clamp( alpha_0_to_1, 0.0f, 1.0f );
    if ( sprite_ ) {
        const auto alpha = static_cast<std::uint8_t>( std::round( 255.0f * opacity_ ) );
        sprite_->setColor( sf::Color( 255, 255, 255, alpha ) );
    }
}

void AnimationController::resetToFirstFrame( ) {
    frameIndex_ = 0;
    frameAccumulator_ = 0.0f;
    finished_ = false;
    applyCurrentFrame( );
}

void AnimationController::setFps( float new_fps ) {
    if ( new_fps > 0.0f ) {
        baseFps_ = new_fps;
        fps_ = fpsForState( animState_, baseFps_ );
    }
}

void AnimationController::maybeTriggerFidget( sf::Time delta_time ) {
    if ( animState_ != AnimState::STAND || ! resource_ ) {
        return;
}

    fidgetCooldown_ -= delta_time.asSeconds( );
    if ( fidgetCooldown_ > 0.0f ) {
        return;
}

    scheduleNextFidget( );

    const auto it = resource_->groups_.find( static_cast<int>( AnimState::FIDGET ) );
    if ( it == resource_->groups_.end( ) || it->second.empty( ) ) {
        return;
}

    setAnimationState( AnimState::FIDGET, false, true );
}

void AnimationController::update( sf::Time delta_time ) {
    if ( resource_ == nullptr ) {
        return;
}

    if ( animState_ == AnimState::FIDGET && finished_ ) {
        setAnimationState( AnimState::STAND, true, true );
        return;
    }

    maybeTriggerFidget( delta_time );

    if ( finished_ ) {
        return;
}

    const auto* group = findGroup( );
    if ( ! group || group->empty( ) ) {
        return;
}

    frameAccumulator_ += delta_time.asSeconds( );
    const float frame_duration = 1.0f / fps_;

    while ( frameAccumulator_ >= frame_duration ) {
        frameAccumulator_ -= frame_duration;

        if ( loop_ ) {
            frameIndex_ = ( frameIndex_ + 1 ) % group->size( );
            applyCurrentFrame( );
            continue;
        }

        if ( frameIndex_ + 1 < group->size( ) ) {
            ++frameIndex_;
            applyCurrentFrame( );
            continue;
        }

        finished_ = true;
        if ( freezeOnLastFrame_ ) {
            frameIndex_ = group->size( ) - 1;
            applyCurrentFrame( );
        }
        break;
    }
}

const sf::Sprite* AnimationController::getSprite( ) const {
    return sprite_.get( );
}

bool AnimationController::isReady( ) const {
    if ( resource_ == nullptr ) {
        return false;
}
    const auto* group = findGroup( );
    return group != nullptr && ! group->empty( );
}

bool AnimationController::isFinished( ) const {
    return finished_;
}

AnimState AnimationController::getAnimationState( ) const {
    return animState_;
}

int AnimationController::getGroupId( ) const {
    return groupId_;
}

const std::vector<DefFrame>* AnimationController::findGroup( ) const {
    if ( ! resource_ ) {
        return nullptr;
}
    auto it = resource_->groups_.find( groupId_ );
    if ( it != resource_->groups_.end( ) && ! it->second.empty( ) ) {
        return &it->second;
}
    for ( int fallback : { 1, 0 } ) {
        auto fb = resource_->groups_.find( fallback );
        if ( fb != resource_->groups_.end( ) && ! fb->second.empty( ) ) {
            return &fb->second;
}
    }
    if ( ! resource_->groups_.empty( ) ) {
        return &resource_->groups_.begin( )->second;
}
    return nullptr;
}

void AnimationController::applyCurrentFrame( ) {
    if ( resource_ == nullptr ) {
        return;
}

    const auto* group = findGroup( );
    if ( ! group || group->empty( ) ) {
        return;
}

    const DefFrame& frame = ( *group )[frameIndex_ % group->size( )];
    if ( frame.canvasWidth_ == 0 || frame.canvasHeight_ == 0 ) {
        return;
}

    if ( ! sprite_ ) {
        sprite_ = std::make_unique<sf::Sprite>( frame.texture_ );
    } else {
        sprite_->setTexture( frame.texture_, true );
    }

    const float canvas_w = static_cast<float>( frame.canvasWidth_ );
    const float canvas_h = static_cast<float>( frame.canvasHeight_ );
    float feet_x = static_cast<float>( resource_->feetX_ );
    float feet_y = static_cast<float>( resource_->feetY_ );
    if ( feet_x <= 0.0f || feet_x > canvas_w ) {
        feet_x = canvas_w * 0.5f;
}
    if ( feet_y <= 0.0f || feet_y > canvas_h ) {
        feet_y = canvas_h - K_CANVAS_FOOT_PADDING;
}
    sprite_->setOrigin( { feet_x, feet_y } );
    sprite_->setPosition( hexCenter_ );

    const float sx = facingLeft_ ? -scale_ : scale_;
    sprite_->setScale( { sx, scale_ } );
    const auto alpha = static_cast<std::uint8_t>( std::round( 255.0f * opacity_ ) );
    sprite_->setColor( sf::Color( 255, 255, 255, alpha ) );
}

} // namespace views
