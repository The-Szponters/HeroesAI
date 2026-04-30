/**
 * @file DefManager.h
 * @brief Filesystem cache of parsed DEF animation resources.
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
    DefManager() = default;

    void set_search_roots(std::vector<std::filesystem::path> roots );

    void add_search_root(std::filesystem::path root );

    [[nodiscard]] std::shared_ptr<DefResource> get_or_load(const std::string& asset_filename );

    void clear_cache( );

private:
    static std::string normalize_filename(const std::string& filename );
    void build_lookup( );

    std::vector<std::filesystem::path> search_roots;
    bool lookup_built = false;

    std::unordered_map<std::string, std::filesystem::path>          filename_to_path;
    std::unordered_map<std::string, std::shared_ptr<DefResource>>   resource_cache;

    DefParser parser;
};

}  // namespace views
