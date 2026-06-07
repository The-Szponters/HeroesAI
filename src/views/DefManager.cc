/**
 * @file DefManager.cc
 * @brief Implementation of the DEF resource cache and lookup.
 * @author Dominik Sledziewski
 */
#include <algorithm>
#include <cctype>
#include <exception>

#include "DefManager.h"

namespace views {

void DefManager::setSearchRoots( std::vector<std::filesystem::path> roots ) {
    searchRoots_ = std::move( roots );
    lookupBuilt_ = false;
    filenameToPath_.clear( );
}

void DefManager::addSearchRoot( std::filesystem::path root ) {
    searchRoots_.push_back( std::move( root ) );
    lookupBuilt_ = false;
    filenameToPath_.clear( );
}

std::string DefManager::normalizeFilename( const std::string& filename ) {
    std::string normalized = filename;
    std::transform( normalized.begin( ),
                    normalized.end( ),
                    normalized.begin( ),
                    []( unsigned char c ) { return static_cast<char>( std::tolower( c ) ); } );
    return normalized;
}

void DefManager::buildLookup( ) {
    filenameToPath_.clear( );

    for ( const auto& root : searchRoots_ ) {
        if ( root.empty( ) || ! std::filesystem::exists( root ) ) {
            continue;
        }
        for ( const auto& entry : std::filesystem::recursive_directory_iterator( root ) ) {
            if ( ! entry.is_regular_file( ) ) {
                continue;
}

            const std::string lower_name =
                normalizeFilename( entry.path( ).filename( ).string( ) );

            filenameToPath_.try_emplace( lower_name, entry.path( ) );
        }
    }

    lookupBuilt_ = true;
}

std::shared_ptr<DefResource> DefManager::getOrLoad( const std::string& asset_filename ) {
    if ( asset_filename.empty( ) ) {
        return nullptr;
}

    if ( ! lookupBuilt_ ) {
        buildLookup( );
}

    const std::string key = normalizeFilename( asset_filename );

    if ( const auto cached = resourceCache_.find( key ); cached != resourceCache_.end( ) ) {
        return cached->second;
    }

    const auto path_it = filenameToPath_.find( key );
    if ( path_it == filenameToPath_.end( ) ) {
        return nullptr;
    }

    try {
        auto shared = std::make_shared<DefResource>( parser_.parseFile( path_it->second ) );
        resourceCache_.emplace( key, shared );
        return shared;
    } catch ( const std::exception& ) {
        return nullptr;
    }
}

void DefManager::clearCache( ) {
    resourceCache_.clear( );
}

} // namespace views
