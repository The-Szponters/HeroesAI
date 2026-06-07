/**
 * @file ViewportUtils.h
 * @brief Helpers that map a fixed logical render area onto the OS window.
 * @author Dominik Sledziewski
 */
#pragma once

#include <SFML/Graphics.hpp>

namespace views {

/**
 * @brief Logical design resolution shared by every scene.
 *
 * All UI coordinates, hex grid math and asset placement assume this
 * fixed coordinate space. The SceneManager maps it onto the actual
 * window via an sf::View, with letterbox/pillarbox bars whenever the
 * window aspect ratio differs from the logical one.
 */
constexpr float K_LOGICAL_WIDTH = 1280.0f;
constexpr float K_LOGICAL_HEIGHT = 960.0f;

/**
 * @brief Applies a letterboxed sf::View at logical dimensions to a window.
 *
 * Preserves the aspect ratio of the logical render area by computing
 * a viewport that occupies the maximal centered rectangle inside the
 * window whose aspect matches @p logical_w : @p logical_h.
 */
inline void applyLetterboxView( sf::RenderWindow& window,
                                  float logical_w,
                                  float logical_h ) {
    sf::View view( sf::FloatRect( { 0.0f, 0.0f }, { logical_w, logical_h } ) );

    const sf::Vector2u window_size = window.getSize( );
    if ( window_size.x == 0 || window_size.y == 0 || logical_w <= 0.0f || logical_h <= 0.0f ) {
        window.setView( view );
        return;
    }

    const float window_aspect =
        static_cast<float>( window_size.x ) / static_cast<float>( window_size.y );
    const float logical_aspect = logical_w / logical_h;

    if ( window_aspect > logical_aspect ) {
        const float scale = logical_aspect / window_aspect;
        view.setViewport(
            sf::FloatRect( { ( 1.0f - scale ) * 0.5f, 0.0f }, { scale, 1.0f } ) );
    } else {
        const float scale = window_aspect / logical_aspect;
        view.setViewport(
            sf::FloatRect( { 0.0f, ( 1.0f - scale ) * 0.5f }, { 1.0f, scale } ) );
    }

    window.setView( view );
}

} // namespace views
