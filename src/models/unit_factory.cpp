#include "unit_factory.hpp"
#include <fstream>
#include <stdexcept>

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

std::unique_ptr<Unit> UnitFactory::create_unit(UnitID id, int count) {
    auto it = unit_data.find(id);
    if (it == unit_data.end()) {
        throw std::runtime_error("UnitFactory: JSON data not loaded or unit not found: " + unit_id_to_string(id));
    }
    
    const auto& data = it->second;
    std::string name = unit_id_to_string(id);
    int tier = data.value("tier", 1);
    int attack = data.value("attack", 1);
    int defense = data.value("defense", 1);
    int health = data.value("health", 1);
    
    int damage = data.value("damage_max", 1);
    
    int speed = data.value("speed", 1);
    int size = data.value("size", 1);
    
    if (data.contains("shoots")) {
        int shoots = data["shoots"];
        return std::make_unique<RangeUnit>(name, tier, attack, defense, health, damage, speed, count, shoots);
    } else {
        return std::make_unique<Unit>(name, tier, attack, defense, health, damage, speed, count);
    }
}
