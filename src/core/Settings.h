/**
 * @file Settings.h
 * @brief Application-level configuration loaded from a JSON file.
 * @author Łukasz Szydlik
 */
#pragma once

#include <array>
#include <optional>
#include <string>

#include "../models/UnitId.h"
#include "PlayerType.h"

namespace core {

/**
 * @brief One slot of an army roster.
 *
 * An empty unitId_ means the slot is unused and should be skipped when
 * building the battle roster.
 */
struct ArmySlot {
    std::optional<models::UnitID> unitId_;
    int count_ = 0;
};

constexpr std::size_t K_ARMY_SLOT_COUNT = 7;

using ArmyConfig = std::array<ArmySlot, K_ARMY_SLOT_COUNT>;

/**
 * @brief Application settings parsed from a JSON file.
 *
 * Holds window configuration and the user-editable left/right army
 * rosters. A missing or malformed file falls back to the defaults baked
 * into this struct, which mirror the original hardcoded HoMM3 rosters.
 */
struct Settings {
    unsigned int windowWidth_ = 1280;
    unsigned int windowHeight_ = 960;
    std::string windowTitle_ = "HeroesAI";
    unsigned int framerateLimit_ = 60;

    ArmyConfig leftArmy_;
    ArmyConfig rightArmy_;

    // Who controls each side. Blue = left army (human by default), red =
    // right army (random bot by default), giving an out-of-the-box
    // human-vs-bot match. Set both to a bot type for an automated
    // bot-vs-bot run.
    PlayerType bluePlayer_ = PlayerType::Human;
    PlayerType redPlayer_ = PlayerType::Random;

    // Search depth used by the Minimax player(s), in unit-activation plies.
    int minimaxDepth_ = 4;

    Settings( );

    static Settings loadFromFile( const std::string& filepath );
    void saveToFile( const std::string& filepath ) const;

    // Persists ONLY the army rosters, merging into the existing file so
    // the window / ai sections (and any other keys) are preserved exactly
    // as on disk. Used by the army-setup screen so editing rosters never
    // clobbers hand-edited settings.
    void saveArmiesToFile( const std::string& filepath ) const;
};

} // namespace core
