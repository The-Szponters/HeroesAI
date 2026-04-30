/**
 * @file Hex.h
 * @brief Single battlefield cell using cube (q, r, s) coordinates.
 */
#pragma once
#include <stdexcept>
#include <memory>
#include <algorithm>
#include "Unit.h"

namespace models {

/**
 * @brief One hex tile on the battlefield.
 *
 * Stores its immutable cube coordinates, an optional weak reference
 * to the Unit currently standing on it, and a list of dead units
 * that have fallen on this tile.
 */
class Hex
{
public:
    Hex(int q, int r, int s) : q(q), r(r), s(s ){
        if( !is_valid() ){
            throw std::invalid_argument("q + r + s must sum to 0" );
        }
    }
    Hex(int q, int r, int s, std::weak_ptr<Unit> unit) : q(q), r(r), s(s), unit(std::move(unit) ){
        if( !is_valid() ){
            throw std::invalid_argument("q + r + s must sum to 0" );
        }
    }
    Hex(const Hex& other) = default;
    ~Hex() = default;

    const int get_q() const { return q; }
    const int get_r() const { return r; }
    const int get_s() const { return s; }

    std::shared_ptr<Unit> get_unit() const { 
        auto shared_unit = unit.lock( );
        if( !shared_unit ){
            throw std::runtime_error("Hex does not contain a unit" );
        }
        return shared_unit; 
    }

    bool operator==(const Hex& other) const {
        return q == other.q && r == other.r && s == other.s;
    }
    const int distance_to(const Hex& other) const {
        return std::max({
            std::abs(q - other.q), 
            std::abs(r - other.r), 
            std::abs(s - other.s)
        } );
    }

    bool has_unit() const {
        return !unit.expired( );
    }

    void set_unit(std::weak_ptr<Unit> new_unit ){
        unit = std::move(new_unit );
    }

    void remove_unit( ){
        unit.reset( );
    }

    void unit_died( ){
        if( !unit.expired() ){
            dead_units.push_back(unit );
            unit.reset( );
        }
    }

    const std::vector<std::weak_ptr<Unit>>& get_dead_units() const {
        return dead_units;
    }

private:
    const int q;
    const int r;
    const int s;
    std::weak_ptr<Unit> unit;
    std::vector<std::weak_ptr<Unit>> dead_units;
    bool is_valid() const {
        return q + r + s == 0;
    }
};

}  // namespace models