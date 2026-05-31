/**
 * @file PlayerType.h
 * @brief Identifies who controls a battle side.
 * @author Łukasz Szydlik
 */
#pragma once

#include <string>

namespace core {

/**
 * @brief The controller driving a battle side.
 *
 * Human   -- played by the user.
 * Random  -- uniformly random legal action (RandomBotService).
 * Easy    -- fixed-priority heuristic (EasyBotService): cast > ranged >
 *            melee > wait > move > defend.
 */
enum class PlayerType {
    Human,
    Random,
    Easy,
    Minimax
};

inline PlayerType playerTypeFromString( const std::string& value ) {
    if ( value == "random" ) {
        return PlayerType::Random;
    }
    if ( value == "easy" ) {
        return PlayerType::Easy;
    }
    if ( value == "minimax" ) {
        return PlayerType::Minimax;
    }
    return PlayerType::Human;
}

inline const char* playerTypeToString( PlayerType type ) {
    switch ( type ) {
    case PlayerType::Random:
        return "random";
    case PlayerType::Easy:
        return "easy";
    case PlayerType::Minimax:
        return "minimax";
    case PlayerType::Human:
    default:
        return "human";
    }
}

} // namespace core
