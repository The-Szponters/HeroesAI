/**
 * @file Army.h
 * @brief Container for a hero's deployable unit stacks.
 */
#pragma once
#include "Unit.h"
#include <memory>
#include <vector>

namespace models {

/**
 * @brief A hero's army -- an ordered, capacity-limited collection of
 *        unit stacks (maximum seven, matching the classic HoMM rule).
 *
 * Owns the unit instances via shared_ptr; references to them may be
 * held weakly by Hex tiles on the battlefield.
 */
class Army {
public:
    Army( ) = default;
    ~Army( ) = default;

    bool addUnit( std::shared_ptr<Unit> unit ) {
        if ( units_.size( ) < 7 && unit != nullptr ) {
            units_.push_back( std::move( unit ) );
            return true;
        }
        return false;
    }

    void removeUnit( size_t index ) {
        if ( index < units_.size( ) ) {
            units_.erase( units_.begin( ) + index );
        }
    }

    const std::vector<std::shared_ptr<Unit>>& getUnits( ) const { return units_; }

private:
    std::vector<std::shared_ptr<Unit>> units_;
};

} // namespace models