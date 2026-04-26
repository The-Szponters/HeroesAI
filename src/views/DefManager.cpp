#include "DefManager.hpp"

#include <algorithm>
#include <cctype>
#include <exception>

void DefManager::set_units_root(std::filesystem::path units_root) {
    units_root_path = std::move(units_root);
    lookup_built = false;
    filename_to_path.clear();
}

std::string DefManager::normalize_filename(const std::string& filename) {
    std::string normalized = filename;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return normalized;
}

void DefManager::build_lookup() {
    filename_to_path.clear();

    if (units_root_path.empty() || !std::filesystem::exists(units_root_path)) {
        lookup_built = true;
        return;
    }

    for (const auto& entry : std::filesystem::recursive_directory_iterator(units_root_path)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        const std::string lower_name = normalize_filename(entry.path().filename().string());
        filename_to_path[lower_name] = entry.path();
    }

    lookup_built = true;
}

std::shared_ptr<DefResource> DefManager::get_or_load(const std::string& asset_filename) {
    if (asset_filename.empty()) {
        return nullptr;
    }

    if (!lookup_built) {
        build_lookup();
    }

    const std::string key = normalize_filename(asset_filename);

    const auto cached_it = resource_cache.find(key);
    if (cached_it != resource_cache.end()) {
        return cached_it->second;
    }

    const auto path_it = filename_to_path.find(key);
    if (path_it == filename_to_path.end()) {
        return nullptr;
    }

    try {
        DefResource resource = parser.parse_file(path_it->second);
        auto shared = std::make_shared<DefResource>(std::move(resource));
        resource_cache.emplace(key, shared);
        return shared;
    } catch (const std::exception&) {
        return nullptr;
    }
}

void DefManager::clear_cache() {
    resource_cache.clear();
}
