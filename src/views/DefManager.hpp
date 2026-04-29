#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "DefParser.hpp"

class DefManager {
public:
    DefManager() = default;

    void set_search_roots(std::vector<std::filesystem::path> roots);

    void add_search_root(std::filesystem::path root);

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
