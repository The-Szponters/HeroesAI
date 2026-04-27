#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "DefParser.hpp"

// Owns DefResource instances for the entire battle session.  Shared ownership
// keeps the underlying textures alive for as long as any sprite references them
// — without this, frames go invisible the moment the loader cache evicts them.
//
// The manager scans one or more *search roots* recursively and indexes every
// `.def` it finds by lowercase filename.  Calls to get_or_load() are O(1) on
// repeat hits.  Roots are typically `assets/units/` for creature animations
// and `assets/ui/` for HUD elements (down bar icons, combat cursor, morale,
// spellbook); both are parsed by the same DefParser.
class DefManager {
public:
    DefManager() = default;

    // Replace the current search roots.  The lookup is rebuilt lazily on the
    // next get_or_load() call.
    void set_search_roots(std::vector<std::filesystem::path> roots);

    // Append a search root without dropping previously-registered ones.
    void add_search_root(std::filesystem::path root);

    // Returns nullptr when filename is empty, the file is missing, or parsing
    // fails (errors are swallowed — callers fall back to placeholder sprites).
    [[nodiscard]] std::shared_ptr<DefResource> get_or_load(const std::string& asset_filename);

    void clear_cache();

private:
    static std::string normalize_filename(const std::string& filename);
    void build_lookup();

    std::vector<std::filesystem::path> search_roots;
    bool lookup_built = false;

    std::unordered_map<std::string, std::filesystem::path>          filename_to_path;
    std::unordered_map<std::string, std::shared_ptr<DefResource>>   resource_cache;

    DefParser parser;
};
