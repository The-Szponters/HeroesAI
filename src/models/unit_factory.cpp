#include "unit_factory.hpp"
#include <fstream>
#include <stdexcept>
std::unordered_map<UnitID, nlohmann::json> UnitFactory::unit_data;

std::string UnitFactory::unit_id_to_string(UnitID id) {
    static const char* unit_names[] = {
        "Pikeman", "Halberdier", "Archer", "Marksman", "Griffin", "RoyalGriffin", "Swordsman", "Crusader", "Monk", "Zealot", "Cavalier", "Champion", "Angel", "Archangel",
        "Imp", "Familiar", "Gog", "Magog", "HellHound", "Cerberus", "Demon", "HornedDemon", "PitFiend", "PitLord", "Efreet", "EfreetSultan", "Devil", "ArchDevil",
        "Skeleton", "SkeletonWarrior", "WalkingDead", "Zombie", "Wight", "Wraith", "Vampire", "VampireLord", "Lich", "PowerLich", "BlackKnight", "DreadKnight", "BoneDragon", "GhostDragon"
    };
    int index = static_cast<int>(id);
    if (index >= 0 && index < 42) {
        return unit_names[index];
    }
    throw std::invalid_argument("Unknown UnitID");
}

void UnitFactory::init(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("UnitFactory: could not open " + filepath);
    }
    
    nlohmann::json j;
    file >> j;
    
    unit_data.clear();
    for (int i = 0; i < 42; ++i) {
        UnitID id = static_cast<UnitID>(i);
        std::string key = unit_id_to_string(id);
        if (j.contains(key)) {
            unit_data[id] = j[key];
        }
    }
}

std::shared_ptr<Unit> UnitFactory::create_unit(UnitID id, int count) {
    auto it = unit_data.find(id);
    if (it == unit_data.end()) {
        throw std::runtime_error("UnitFactory: JSON data not loaded or unit not found: " + unit_id_to_string(id));
    }
    
    const auto& data = it->second;
    std::string name = unit_id_to_string(id);
    
    try {
        int tier = data.at("tier").get<int>();
        int attack = data.at("attack").get<int>();
        int defense = data.at("defense").get<int>();
        int health = data.at("health").get<int>();
        
        int damage_min = data.at("damage_min").get<int>();
        int damage_max = data.at("damage_max").get<int>();
        
        int speed = data.at("speed").get<int>();
        
        if (data.contains("shoots")) {
            int shoots = data.at("shoots").get<int>();
            return std::make_shared<RangeUnit>(name, tier, attack, defense, health, damage_min, damage_max, speed, count, shoots);
        } else {
            return std::make_shared<Unit>(name, tier, attack, defense, health, damage_min, damage_max, speed, count);
        }
    } catch (const std::exception& e) {
        throw std::runtime_error("UnitFactory: missing or invalid field for unit " + name + " (" + e.what() + ")");
    }
}
