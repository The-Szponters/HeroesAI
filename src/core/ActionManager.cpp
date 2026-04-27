#include "ActionManager.hpp"
#include <algorithm>
#include <limits>
#include <queue>
#include <set>
#include <map>
#include <random>
#include <tuple>

namespace {

// Tail offset for a 2-hex unit, given its facing.
// Right army (q >= 7, facing left) → tail extends RIGHT (dq=+1).
// Left  army (q <  7, facing right) → tail extends LEFT  (dq=-1).
std::tuple<int, int, int> tail_delta(const Unit& u) {
    if (u.is_facing_left()) return { 1, 0, -1};
    return {-1, 0,  1};
}

bool is_self_hex(const Unit& u, int q, int r, int s) {
    if (q == u.get_q() && r == u.get_r() && s == u.get_s()) return true;
    if (u.get_size() == 2) {
        auto [dq, dr, ds] = tail_delta(u);
        if (q == u.get_q() + dq && r == u.get_r() + dr && s == u.get_s() + ds) return true;
    }
    return false;
}

// Returns true if (q,r,s) AND (for size==2) its tail are valid, on-board,
// and either empty or occupied only by `mover` itself.
bool can_occupy(const Unit& mover, int q, int r, int s, const Board& board) {
    auto check = [&](int hq, int hr, int hs) {
        try {
            const Hex& h = board.get_hex(hq, hr, hs);
            if (h.has_unit() && !is_self_hex(mover, hq, hr, hs)) return false;
            return true;
        } catch (const std::out_of_range&) { return false; }
    };

    if (!check(q, r, s)) return false;
    if (mover.get_size() == 2) {
        auto [dq, dr, ds] = tail_delta(mover);
        if (!check(q + dq, r + dr, s + ds)) return false;
    }
    return true;
}

std::vector<std::tuple<int,int,int>> body_hexes(const Unit& u) {
    std::vector<std::tuple<int,int,int>> v;
    v.emplace_back(u.get_q(), u.get_r(), u.get_s());
    if (u.get_size() == 2) {
        auto [dq, dr, ds] = tail_delta(u);
        v.emplace_back(u.get_q() + dq, u.get_r() + dr, u.get_s() + ds);
    }
    return v;
}

bool are_units_adjacent(const Unit& a, const Unit& b) {
    for (const auto& [aq, ar, as] : body_hexes(a)) {
        for (const auto& [bq, br, bs] : body_hexes(b)) {
            const int d = std::max({std::abs(aq - bq), std::abs(ar - br), std::abs(as - bs)});
            if (d == 1) return true;
        }
    }
    return false;
}

} // namespace

int ActionManager::hex_distance(const Unit& a, const Unit& b) {
    int best = std::numeric_limits<int>::max();
    for (const auto& [aq, ar, as] : body_hexes(a)) {
        for (const auto& [bq, br, bs] : body_hexes(b)) {
            const int d = std::max({std::abs(aq - bq), std::abs(ar - br), std::abs(as - bs)});
            if (d < best) best = d;
        }
    }
    return best;
}

bool ActionManager::is_blocked_by_adjacent_enemy(const Unit& unit,
                                                 const EnemyPredicate& is_enemy,
                                                 const Board& board) const {
    static constexpr int dq[] = { 1,  1,  0, -1, -1,  0};
    static constexpr int dr[] = { 0, -1, -1,  0,  1,  1};
    static constexpr int ds[] = {-1,  0,  1,  1,  0, -1};

    for (const auto& [oq, orr, os] : body_hexes(unit)) {
        for (int i = 0; i < 6; ++i) {
            try {
                const Hex& nhex = board.get_hex(oq + dq[i], orr + dr[i], os + ds[i]);
                if (!nhex.has_unit()) continue;
                const std::shared_ptr<Unit>& neighbour = nhex.get_unit();
                if (neighbour.get() == &unit) continue;
                if (is_enemy && is_enemy(*neighbour)) return true;
            } catch (const std::out_of_range&) {}
        }
    }
    return false;
}

bool ActionManager::can_shoot(const Unit& attacker, const Unit& defender,
                              const EnemyPredicate& is_enemy, const Board& board) const {
    if (!attacker.is_ranged() || attacker.get_ammo() <= 0) return false;
    if (are_units_adjacent(attacker, defender)) return false;
    if (is_blocked_by_adjacent_enemy(attacker, is_enemy, board)) return false;
    return true;
}

std::vector<const Hex*> ActionManager::find_path(const Unit& unit, const Hex& dest_hex, const Board& board) const {
    // Plain BFS over the same hex adjacency the destination scan uses, but
    // bounded by the unit's speed and constrained to hexes the unit can occupy
    // as a HEAD position.  We keep `parent` indexed by (q,r,s) → predecessor
    // tuple so we can reconstruct an actual chain (not just the set of
    // reachable destinations).

    using Coord = std::tuple<int, int, int>;
    const Coord start{unit.get_q(), unit.get_r(), unit.get_s()};
    const Coord goal {dest_hex.get_q(), dest_hex.get_r(), dest_hex.get_s()};

    if (start == goal) {
        try {
            const Hex& s = board.get_hex(std::get<0>(start), std::get<1>(start), std::get<2>(start));
            return {&s};
        } catch (const std::out_of_range&) { return {}; }
    }

    std::map<Coord, Coord>     parent;
    std::map<Coord, int>       dist;
    std::queue<Coord>          q;
    q.push(start);
    dist[start] = 0;

    static constexpr int dq[] = {1, 1, 0, -1, -1, 0};
    static constexpr int dr[] = {0, -1, -1, 0, 1, 1};
    static constexpr int ds[] = {-1, 0, 1, 1, 0, -1};

    bool found = false;
    while (!q.empty() && !found) {
        const Coord cur = q.front(); q.pop();
        const int   d   = dist[cur];
        if (d >= unit.get_speed()) continue;

        for (int i = 0; i < 6; ++i) {
            const int nq = std::get<0>(cur) + dq[i];
            const int nr = std::get<1>(cur) + dr[i];
            const int ns = std::get<2>(cur) + ds[i];
            const Coord next{nq, nr, ns};
            if (dist.count(next)) continue;
            if (!can_occupy(unit, nq, nr, ns, board)) continue;

            dist[next] = d + 1;
            parent[next] = cur;
            if (next == goal) { found = true; break; }
            q.push(next);
        }
    }

    if (!found) return {};

    // Reconstruct path goal → start, then reverse.
    std::vector<const Hex*> chain;
    Coord cur = goal;
    while (true) {
        try {
            chain.push_back(&board.get_hex(std::get<0>(cur), std::get<1>(cur), std::get<2>(cur)));
        } catch (const std::out_of_range&) { return {}; }
        if (cur == start) break;
        const auto it = parent.find(cur);
        if (it == parent.end()) return {};
        cur = it->second;
    }
    std::reverse(chain.begin(), chain.end());
    return chain;
}

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

            const bool at_start = (cq == unit.get_q() && cr == unit.get_r() && cs == unit.get_s());

            // The current candidate hex must be reachable as a HEAD position;
            // for size==2 that means head + tail are both clear of other units.
            const bool valid_stand = at_start || can_occupy(unit, cq, cr, cs, board);

            if (valid_stand) {
                if (!at_start) {
                    destinations.push_back(const_cast<Hex*>(&hex));
                }

                if (current_speed > 0) {
                    for (int i = 0; i < 6; ++i) {
                        int nq = cq + dq[i];
                        int nr = cr + dr[i];
                        int ns = cs + ds[i];

                        if (visited.find({nq, nr, ns}) != visited.end()) continue;
                        try {
                            (void)board.get_hex(nq, nr, ns);
                        } catch (const std::out_of_range&) { continue; }

                        // Walk-through still requires that the hex (and tail
                        // for 2-hex creatures) is clear of OTHER units.
                        if (!can_occupy(unit, nq, nr, ns, board)) continue;

                        visited.insert({nq, nr, ns});
                        q.push({{nq, nr, ns}, current_speed - 1});
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

    // Origins from which the attacker threatens neighbours: head, plus tail for 2-hex.
    std::vector<std::tuple<int,int,int>> origins;
    origins.emplace_back(unit.get_q(), unit.get_r(), unit.get_s());
    if (unit.get_size() == 2) {
        auto [tdq, tdr, tds] = tail_delta(unit);
        origins.emplace_back(unit.get_q() + tdq, unit.get_r() + tdr, unit.get_s() + tds);
    }

    std::set<Unit*> seen;
    for (const auto& [oq, orr, os] : origins) {
        for (int i = 0; i < 6; ++i) {
            int nq = oq + dq[i];
            int nr = orr + dr[i];
            int ns = os + ds[i];
            try {
                const Hex& hex = board.get_hex(nq, nr, ns);
                if (!hex.has_unit()) continue;
                std::shared_ptr<Unit> target = hex.get_unit();
                if (target.get() == &unit) continue;
                if (seen.insert(target.get()).second) {
                    attacks.push_back({target.get(), const_cast<Hex*>(&hex)});
                }
            } catch (const std::out_of_range&) {}
        }
    }
    return attacks;
}

void ActionManager::move(Unit& unit, Hex& dest_hex, Board& board) {
    Hex& start_hex = board.get_hex(unit.get_q(), unit.get_r(), unit.get_s());
    std::shared_ptr<Unit> unit_ptr = start_hex.get_unit();
    if (!unit_ptr || unit_ptr.get() != &unit) {
        throw std::logic_error("Unit coordinates and Board state are out of sync");
    }

    if (unit.get_size() == 2) {
        auto [dq, dr, ds] = tail_delta(unit);

        // Clear the old tail if it is on the board (edge-placed units may have
        // their tail off the board — treat it as a no-op in that case).
        try {
            Hex& start_tail = board.get_hex(start_hex.get_q() + dq,
                                            start_hex.get_r() + dr,
                                            start_hex.get_s() + ds);
            start_tail.remove_unit();
        } catch (const std::out_of_range&) {}

        Hex& dest_tail = board.get_hex(dest_hex.get_q() + dq,
                                       dest_hex.get_r() + dr,
                                       dest_hex.get_s() + ds);

        const bool dest_blocked =
            dest_hex.has_unit()  && dest_hex.get_unit().get()  != &unit;
        const bool tail_blocked =
            dest_tail.has_unit() && dest_tail.get_unit().get() != &unit;
        if (dest_blocked || tail_blocked) {
            throw std::runtime_error("Destination hex already has a unit");
        }

        start_hex.remove_unit();
        dest_hex.set_unit(unit_ptr);
        dest_tail.set_unit(unit_ptr);
        unit.set_position(dest_hex.get_q(), dest_hex.get_r(), dest_hex.get_s());
        return;
    }

    if (dest_hex.has_unit()) {
        throw std::runtime_error("Destination hex already has a unit");
    }
    dest_hex.set_unit(unit_ptr);
    start_hex.remove_unit();
    unit.set_position(dest_hex.get_q(), dest_hex.get_r(), dest_hex.get_s());
}

bool ActionManager::attack(Unit& attacker, Unit& defender, Hex& attack_from_hex, Board& board) {
    if (attack_from_hex.has_unit() && attack_from_hex.get_unit().get() != &attacker) {
        throw std::runtime_error("Attack hex already has a unit");
    }

    try {
        Hex& current = board.get_hex(attacker.get_q(), attacker.get_r(), attacker.get_s());
        if (&current != &attack_from_hex) {
            move(attacker, attack_from_hex, board);
        }
    } catch (std::out_of_range&) {}

    // Defender turns toward the side the hit comes from.
    if (attacker.get_q() < defender.get_q()) {
        defender.set_visual_facing_left(true);
    } else if (attacker.get_q() > defender.get_q()) {
        defender.set_visual_facing_left(false);
    }

    // Ranged units fighting in melee deal half damage (HoMM3 melee penalty).
    int damage = calculate_damage(attacker, defender);
    if (attacker.is_ranged()) damage /= 2;
    defender.take_damage(damage);

    if (defender.get_count() == 0) {
        try {
            Hex& def_hex = board.get_hex(defender.get_q(), defender.get_r(), defender.get_s());
            def_hex.unit_died();
            if (defender.get_size() == 2) {
                auto [dq, dr, ds] = tail_delta(defender);
                try {
                    Hex& def_tail = board.get_hex(defender.get_q() + dq,
                                                  defender.get_r() + dr,
                                                  defender.get_s() + ds);
                    // Keep corpse metadata on both occupied hexes for 2-hex units.
                    def_tail.unit_died();
                } catch (std::out_of_range&) {}
            }
        } catch (std::out_of_range&) {}
        return true;
    }

    // Retaliation: defender hits back once per round if it survived and is
    // adjacent to the attacker (HoMM3 standard counter-attack).
    if (!defender.has_retaliated_this_round() && are_units_adjacent(attacker, defender)) {
        // Retaliation direction follows the same rule for the struck attacker.
        if (defender.get_q() < attacker.get_q()) {
            attacker.set_visual_facing_left(true);
        } else if (defender.get_q() > attacker.get_q()) {
            attacker.set_visual_facing_left(false);
        }

        const int counter = calculate_damage(defender, attacker);
        attacker.take_damage(counter);
        defender.set_retaliated(true);

        if (attacker.get_count() == 0) {
            try {
                Hex& atk_hex = board.get_hex(attacker.get_q(), attacker.get_r(), attacker.get_s());
                atk_hex.unit_died();
                if (attacker.get_size() == 2) {
                    auto [dq, dr, ds] = tail_delta(attacker);
                    try {
                        Hex& atk_tail = board.get_hex(attacker.get_q() + dq,
                                                      attacker.get_r() + dr,
                                                      attacker.get_s() + ds);
                        // Keep corpse metadata on both occupied hexes for 2-hex units.
                        atk_tail.unit_died();
                    } catch (std::out_of_range&) {}
                }
            } catch (std::out_of_range&) {}
        }
    }

    return false;
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
    int attack_stat  = attacker.get_attack();
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

bool ActionManager::shoot(Unit& attacker, Unit& defender, Board& board) {
    if (!attacker.is_ranged() || attacker.get_ammo() <= 0) {
        throw std::logic_error("shoot() called on a unit that cannot shoot");
    }

    // Face the target — purely visual; ranged units never move.
    if (attacker.get_q() < defender.get_q()) {
        attacker.set_visual_facing_left(false);
    } else if (attacker.get_q() > defender.get_q()) {
        attacker.set_visual_facing_left(true);
    }

    int damage = calculate_damage(attacker, defender);
    if (hex_distance(attacker, defender) > 10) {
        damage /= 2;
    }
    defender.take_damage(damage);
    attacker.decrement_ammo();

    if (defender.get_count() == 0) {
        try {
            Hex& def_hex = board.get_hex(defender.get_q(), defender.get_r(), defender.get_s());
            def_hex.unit_died();
            if (defender.get_size() == 2) {
                auto [dq, dr, ds] = tail_delta(defender);
                try {
                    Hex& def_tail = board.get_hex(defender.get_q() + dq,
                                                  defender.get_r() + dr,
                                                  defender.get_s() + ds);
                    def_tail.unit_died();
                } catch (std::out_of_range&) {}
            }
        } catch (std::out_of_range&) {}
        return true;
    }
    return false;
}
