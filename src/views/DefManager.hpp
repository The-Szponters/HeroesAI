#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>

#include "DefParser.hpp"

// DefManager owns loaded DefResource instances for the full battle/session.
// Shared ownership prevents texture lifetime issues that make sprites invisible.
class DefManager {
public:
    DefManager() = default;

    void set_units_root(std::filesystem::path units_root);

    // Returns nullptr when filename is empty or no file/parse result exists.
    [[nodiscard]] std::shared_ptr<DefResource> get_or_load(const std::string& asset_filename);

    void clear_cache();

private:
    static std::string normalize_filename(const std::string& filename);
    void build_lookup();

    std::filesystem::path units_root_path;
    bool lookup_built = false;

    std::unordered_map<std::string, std::filesystem::path> filename_to_path;
    std::unordered_map<std::string, std::shared_ptr<DefResource>> resource_cache;

    DefParser parser;
};
