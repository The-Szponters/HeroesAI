#pragma once
#include <stdexcept>
#include <memory>
#include <algorithm>
#include "unit.hpp"

class Hex
{
public:
    Hex() = default;
    Hex(int q, int r, int s) : q(q), r(r), s(s) {
        if (!is_valid()) {
            throw std::invalid_argument("q + r + s must sum to 0");
        }
    }
    Hex(int q, int r, int s, const Unit& unit) : q(q), r(r), s(s), unit(std::make_shared<Unit>(unit)) {
        if (!is_valid()) {
            throw std::invalid_argument("q + r + s must sum to 0");
        }
    }
    Hex(const Hex& other) = default;
    ~Hex() = default;
    
    const int get_q() const { return q; }
    const int get_r() const { return r; }
    const int get_s() const { return s; }
    const Unit& get_unit() const { return *unit; }

    bool operator==(const Hex& other) const {
        return q == other.q && r == other.r && s == other.s;
    }
    const int distance_to(const Hex& other) const {
        return std::max({
            std::abs(q - other.q), 
            std::abs(r - other.r), 
            std::abs(s - other.s)
        });
    }
private:
    const int q;
    const int r;
    const int s;
    std::shared_ptr<Unit> unit;
    bool is_valid() const {
        return q + r + s == 0;
    }
};