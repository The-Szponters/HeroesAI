#pragma once
#include <functional>

enum class BuffType {
    Defend,
    Slow,
    Blind
};

struct Buff {
    BuffType type;
    int duration;

    std::function<int(int)> modify_attack = [](int x) { return x; };
    std::function<int(int)> modify_defense = [](int x) { return x; };
    std::function<int(int)> modify_speed = [](int x) { return x; };
    std::function<int(int)> modify_damage_min = [](int x) { return x; };
    std::function<int(int)> modify_damage_max = [](int x) { return x; };
};

class BuffFactory {
public:
    static Buff create_defend_buff() {
        Buff b;
        b.type = BuffType::Defend;
        b.duration = 1;
        b.modify_defense = [](int def) { return def + 5; };
        return b;
    }

    static Buff create_slow_buff() {
        Buff b;
        b.type = BuffType::Slow;
        b.duration = 3;
        b.modify_speed = [](int spd) { return spd / 2; };
        return b;
    }

    static Buff create_blind_buff() {
        Buff b;
        b.type = BuffType::Blind;
        b.duration = 3;
        b.modify_speed = [](int spd) { return 0; };
        return b;
    }
};
