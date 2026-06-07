/**
 * @file BattleLayout.h
 * @brief Single source of truth for battle hex-grid layout constants.
 * @author Dominik Sledziewski
 */
#pragma once

namespace views {

/**
 * @brief Hex outer radius in logical pixels.
 *
 * Drives both the rendered hex geometry in SfmlBattleView and the
 * pixel<->hex math in BattlePresenter; the two must agree, so they
 * share these constants instead of redeclaring them locally.
 */
constexpr float K_HEX_RADIUS = 36.0f;

/**
 * @brief Logical x of the (q=0, r=0) hex center.
 */
constexpr float K_GRID_ORIGIN_X = 200.0f;

/**
 * @brief Logical y of the (q=0, r=0) hex center.
 *
 * Includes one row of vertical hex offset (1.5 * radius) below a
 * fixed top margin so the first row clears the HUD area.
 */
constexpr float K_GRID_ORIGIN_Y = 130.0f + K_HEX_RADIUS * 1.5f;

} // namespace views
