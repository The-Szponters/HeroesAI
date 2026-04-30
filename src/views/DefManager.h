/**
 * @file DefManager.h
 * @brief Filesystem cache of parsed DEF animation resources.
 * @author Łukasz Szydlik
 */
#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "DefParser.h"

namespace views {

/**
 * @brief Loads, caches and serves DEF animation resources by name.
 *
 * Walks one or more search roots once, builds a case-insensitive
 * filename-to-path index, and lazily parses requested files into
 * shared DefResource objects so multiple units can share frames.
 */
class DefManager {
public:
    DefManager( ) = default;

    void setSearchRoots( std::vector<std::filesystem::path> roots );

    void addSearchRoot( std::filesystem::path root );

    [[nodiscard]] std::shared_ptr<DefResource> getOrLoad( const std::string& asset_filename );

    void clearCache( );

private:
    static std::string normalizeFilename( const std::string& filename );
    void buildLookup( );

    std::vector<std::filesystem::path> searchRoots_;
    bool lookupBuilt_ = false;

    std::unordered_map<std::string, std::filesystem::path> filenameToPath_;
    std::unordered_map<std::string, std::shared_ptr<DefResource>> resourceCache_;

    DefParser parser_;
};

} // namespace views
