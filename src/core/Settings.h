/**
 * @file Settings.h
 * @brief Application-level configuration loaded from a plain-text file.
 * @author Łukasz Szydlik
 */
#pragma once

#include <string>

namespace core {

/**
 * @brief Application settings parsed from a key=value text file.
 *
 * The file follows a tiny INI-like grammar: one "key = value" per line,
 * '#' or ';' starting a comment, blank lines ignored, no sections.
 * Any missing key falls back to the default value baked into this
 * struct, so the file may be partial or absent entirely.
 */
struct Settings {
    unsigned int windowWidth_ = 1280;
    unsigned int windowHeight_ = 960;
    std::string windowTitle_ = "HeroesAI";
    unsigned int framerateLimit_ = 60;

    static Settings loadFromFile( const std::string& filepath );
};

} // namespace core
