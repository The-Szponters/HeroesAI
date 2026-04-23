#include "ActionManager.hpp"
#include <algorithm>
#include <queue>
#include <set>
#include <map>
#include <random>

std::vector<Hex*> ActionManager::get_available_destinations(const Unit& unit, const Board& board) const {
    std::vector<Hex*> destinations;
    std::set<std::tuple<int, int, int>> visited;
    
    std::queue<std::pair<std::tuple<int, int, int>, int>> q;
    
    try {
        const Hex& start_hex = board.get_hex(unit.get_q(), unit.get_r(), unit.get_s());
        q.push({{start_hex.get_q(), start_hex.get_r(), start_hex.get_s()}, unit.get_speed()});
        visited.insert({start_hex.get_q(), start_hex.get_r(), start_hex.get_s()});
    } catch (const std::out_of_range&) {
        return destinations; // Unit not on board
    }
    
    const int dq[] = {1, 1, 0, -1, -1, 0};
    const int dr[] = {0, -1, -1, 0, 1, 1};
    const int ds[] = {-1, 0, 1, 1, 0, -1};
    
    while (!q.empty()) {
        auto [current_coords, current_speed] = q.front();
        q.pop();
        int cq = std::get<0>(current_coords);
        int cr = std::get<1>(current_coords);
        int cs = std::get<2>(current_coords);
        
        try {
            const Hex& hex = board.get_hex(cq, cr, cs);
            if (!hex.has_unit() || (cq == unit.get_q() && cr == unit.get_r() && cs == unit.get_s())) {
                if (!(cq == unit.get_q() && cr == unit.get_r() && cs == unit.get_s())) {
                    destinations.push_back(const_cast<Hex*>(&hex));
                }
                
                if (current_speed > 0) {
                    for (int i = 0; i < 6; ++i) {
                        int nq = cq + dq[i];
                        int nr = cr + dr[i];
                        int ns = cs + ds[i];
                        
                        if (visited.find({nq, nr, ns}) == visited.end()) {
                            try {
                                const Hex& nhex = board.get_hex(nq, nr, ns);
                                if (!nhex.has_unit()) { // We can only walk through empty hexes in this simple implementation
                                    visited.insert({nq, nr, ns});
                                    q.push({{nq, nr, ns}, current_speed - 1});
                                }
                            } catch (const std::out_of_range&) {}
                        }
                    }
                }
            }
        } catch (const std::out_of_range&) {}
    }
    
    return destinations;
}

std::vector<std::pair<Unit*, Hex*>> ActionManager::get_available_attacks(const Unit& unit, const Board& board) const {
    std::vector<std::pair<Unit*, Hex*>> attacks;
    
    
    const int dq[] = {1, 1, 0, -1, -1, 0};
    const int dr[] = {0, -1, -1, 0, 1, 1};
    const int ds[] = {-1, 0, 1, 1, 0, -1};
    
    for (int i = 0; i < 6; ++i) {
        int nq = unit.get_q() + dq[i];
        int nr = unit.get_r() + dr[i];
        int ns = unit.get_s() + ds[i];
        
        try {
            const Hex& hex = board.get_hex(nq, nr, ns);
            if (hex.has_unit()) {
                std::shared_ptr<Unit> target = hex.get_unit();
                if (target.get() != &unit) {
                    attacks.push_back({target.get(), const_cast<Hex*>(&hex)});
                }
            }
        } catch (const std::out_of_range&) {}
    }
    return attacks;
}

void ActionManager::move(Unit& unit, Hex& dest_hex, Board& board) {
    if (dest_hex.has_unit()) {
        throw std::runtime_error("Destination hex already has a unit");
    }

    try {
        Hex& start_hex = board.get_hex(unit.get_q(), unit.get_r(), unit.get_s());
        
        std::shared_ptr<Unit> shared_u;
        if (start_hex.has_unit()) {
            shared_u = start_hex.get_unit();
            if (shared_u.get() == &unit) {
                start_hex.remove_unit();
            } else {
                shared_u.reset();
            }
        }
        
        if (shared_u && shared_u.get() == &unit) {
            dest_hex.set_unit(shared_u);
        }
        
        unit.set_position(dest_hex.get_q(), dest_hex.get_r(), dest_hex.get_s());
    } catch(std::out_of_range&) { }
}

void ActionManager::attack(Unit& attacker, Unit& defender, Hex& attack_from_hex, Board& board) {
    if (attack_from_hex.has_unit() && attack_from_hex.get_unit().get() != &attacker) {
        throw std::runtime_error("Attack hex already has a unit");
    }

    try {
        Hex& current = board.get_hex(attacker.get_q(), attacker.get_r(), attacker.get_s());
        if (&current != &attack_from_hex) {
            move(attacker, attack_from_hex, board);
        }
    } catch(std::out_of_range&) { }
    
    int damage = calculate_damage(attacker, defender);
    defender.take_damage(damage);
}

void ActionManager::defend(Unit& unit) {
    unit.apply_buff(BuffFactory::create_defend_buff());
}

int ActionManager::calculate_damage(const Unit& attacker, const Unit& defender) const {
    if (attacker.get_count() <= 0) return 0;
    
    int total_base_damage = 0;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(attacker.get_damage_min(), attacker.get_damage_max());
    
    for (int i = 0; i < attacker.get_count(); ++i) {
        total_base_damage += distrib(gen);
    }
    
    double modifier = 1.0;
    int attack_stat = attacker.get_attack();
    int defense_stat = defender.get_defense();
    
    if (attack_stat > defense_stat) {
        modifier += 0.05 * (attack_stat - defense_stat);
        if (modifier > 4.0) modifier = 4.0;
    } else if (attack_stat < defense_stat) {
        modifier -= 0.025 * (defense_stat - attack_stat);
        if (modifier < 0.3) modifier = 0.3;
    }
    
    return static_cast<int>(total_base_damage * modifier);
}
