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
    Hex(int q, int r, int s, std::shared_ptr<Unit> unit) : q(q), r(r), s(s), unit(std::move(unit)) {
        if (!is_valid()) {
            throw std::invalid_argument("q + r + s must sum to 0");
        }
    }
    Hex(const Hex& other) = delete;
    ~Hex() = default;
    
    int get_q() const { return q; }
    int get_r() const { return r; }
    int get_s() const { return s; }

    bool operator==(const Hex& other) const {
        return q == other.q && r == other.r && s == other.s;
    }
    int operator-(const Hex& other) const {
        return  std::max({q - other.q, r - other.r, s - other.s});
    }
private:
    int q;
    int r;
    int s;
    std::shared_ptr<Unit> unit;
    bool is_valid() const {
        return q + r + s == 0;
    }
};