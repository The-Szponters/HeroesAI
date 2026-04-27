#include "DefManager.hpp"

#include <algorithm>
#include <cctype>
#include <exception>

void DefManager::set_search_roots(std::vector<std::filesystem::path> roots) {
    search_roots = std::move(roots);
    lookup_built = false;
    filename_to_path.clear();
}

void DefManager::add_search_root(std::filesystem::path root) {
    search_roots.push_back(std::move(root));
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

    for (const auto& root : search_roots) {
        if (root.empty() || !std::filesystem::exists(root)) {
            continue;
        }
        for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
            if (!entry.is_regular_file()) continue;

            const std::string lower_name = normalize_filename(entry.path().filename().string());
            // First match wins — earlier roots take precedence over later ones.
            filename_to_path.try_emplace(lower_name, entry.path());
        }
    }

    lookup_built = true;
}

std::shared_ptr<DefResource> DefManager::get_or_load(const std::string& asset_filename) {
    if (asset_filename.empty()) return nullptr;

    if (!lookup_built) build_lookup();

    const std::string key = normalize_filename(asset_filename);

    if (const auto cached = resource_cache.find(key); cached != resource_cache.end()) {
        return cached->second;
    }

    const auto path_it = filename_to_path.find(key);
    if (path_it == filename_to_path.end()) {
        return nullptr;
    }

    try {
        auto shared = std::make_shared<DefResource>(parser.parse_file(path_it->second));
        resource_cache.emplace(key, shared);
        return shared;
    } catch (const std::exception&) {
        return nullptr;
    }
}

void DefManager::clear_cache() {
    resource_cache.clear();
}
