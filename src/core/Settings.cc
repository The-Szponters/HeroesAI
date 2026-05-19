/**
 * @file Settings.cc
 * @brief Implementation of the plain-text settings loader.
 * @author Łukasz Szydlik
 */
#include "Settings.h"

#include <fstream>
#include <stdexcept>
#include <string>

namespace core {

namespace {

std::string trim( std::string_view sv ) {
    const auto first = sv.find_first_not_of( " \t\r\n" );
    if ( first == std::string_view::npos ) {
        return std::string( );
    }
    const auto last = sv.find_last_not_of( " \t\r\n" );
    return std::string( sv.substr( first, last - first + 1 ) );
}

bool parseUInt( const std::string& value, unsigned int& out ) {
    try {
        unsigned long parsed = std::stoul( value );
        out = static_cast<unsigned int>( parsed );
        return true;
    } catch ( const std::exception& ) {
        return false;
    }
}

} // namespace

Settings Settings::loadFromFile( const std::string& filepath ) {
    Settings settings;
    std::ifstream file( filepath );
    if ( ! file.is_open( ) ) {
        return settings;
    }

    std::string line;
    while ( std::getline( file, line ) ) {
        const std::string trimmed = trim( line );
        if ( trimmed.empty( ) || trimmed[0] == '#' || trimmed[0] == ';' ) {
            continue;
        }

        const auto eq_pos = trimmed.find( '=' );
        if ( eq_pos == std::string::npos ) {
            continue;
        }

        const std::string key = trim( std::string_view( trimmed ).substr( 0, eq_pos ) );
        const std::string value = trim( std::string_view( trimmed ).substr( eq_pos + 1 ) );

        if ( key == "window_width" ) {
            parseUInt( value, settings.windowWidth_ );
        } else if ( key == "window_height" ) {
            parseUInt( value, settings.windowHeight_ );
        } else if ( key == "window_title" ) {
            settings.windowTitle_ = value;
        } else if ( key == "framerate_limit" ) {
            parseUInt( value, settings.framerateLimit_ );
        }
    }

    return settings;
}

} // namespace core
