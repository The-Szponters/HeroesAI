/**
 * @file ActionManager.cc
 * @brief Implementation of pathfinding and combat resolution rules.
 * @author Lukasz Szydlik
 */
#include <algorithm>
#include <limits>
#include <map>
#include <queue>
#include <random>
#include <set>
#include <tuple>

#include "ActionManager.h"

namespace core {

using models::Board;
using models::BuffFactory;
using models::Hex;
using models::Unit;

namespace {

std::tuple<int, int, int> tailDelta( const Unit& u ) {
    if ( u.isFacingLeft( ) ) {
        return { 1, 0, -1 };
}
    return { -1, 0, 1 };
}

bool isSelfHex( const Unit& u, int q, int r, int s ) {
    if ( q == u.getQ( ) && r == u.getR( ) && s == u.getS( ) ) {
        return true;
}
    if ( u.getSize( ) == 2 ) {
        auto [dq, dr, ds] = tailDelta( u );
        if ( q == u.getQ( ) + dq && r == u.getR( ) + dr && s == u.getS( ) + ds ) {
            return true;
}
    }
    return false;
}

bool canOccupy( const Unit& mover, int q, int r, int s, const Board& board ) {
    auto check = [&]( int hq, int hr, int hs ) {
        try {
            const Hex& h = board.getHex( hq, hr, hs );
            if ( h.hasUnit( ) && ! isSelfHex( mover, hq, hr, hs ) ) {
                return false;
}
            return true;
        } catch ( const std::out_of_range& ) {
            return false;
        }
    };

    if ( ! check( q, r, s ) ) {
        return false;
}
    if ( mover.getSize( ) == 2 ) {
        auto [dq, dr, ds] = tailDelta( mover );
        if ( ! check( q + dq, r + dr, s + ds ) ) {
            return false;
}
    }
    return true;
}

std::vector<std::tuple<int, int, int>> bodyHexes( const Unit& u ) {
    std::vector<std::tuple<int, int, int>> v;
    v.emplace_back( u.getQ( ), u.getR( ), u.getS( ) );
    if ( u.getSize( ) == 2 ) {
        auto [dq, dr, ds] = tailDelta( u );
        v.emplace_back( u.getQ( ) + dq, u.getR( ) + dr, u.getS( ) + ds );
    }
    return v;
}

bool areUnitsAdjacent( const Unit& a, const Unit& b ) {
    for ( const auto& [aq, ar, as] : bodyHexes( a ) ) {
        for ( const auto& [bq, br, bs] : bodyHexes( b ) ) {
            const int d =
                std::max( { std::abs( aq - bq ), std::abs( ar - br ), std::abs( as - bs ) } );
            if ( d == 1 ) {
                return true;
}
        }
    }
    return false;
}

} // namespace

int ActionManager::hexDistance( const Unit& a, const Unit& b ) {
    int best = std::numeric_limits<int>::max( );
    for ( const auto& [aq, ar, as] : bodyHexes( a ) ) {
        for ( const auto& [bq, br, bs] : bodyHexes( b ) ) {
            const int d =
                std::max( { std::abs( aq - bq ), std::abs( ar - br ), std::abs( as - bs ) } );
            if ( d < best ) {
                best = d;
}
        }
    }
    return best;
}

bool ActionManager::isBlockedByAdjacentEnemy( const Unit& unit,
                                                  const EnemyPredicate& is_enemy,
                                                  const Board& board ) const {
    static constexpr int DQ[] = { 1, 1, 0, -1, -1, 0 };
    static constexpr int DR[] = { 0, -1, -1, 0, 1, 1 };
    static constexpr int DS[] = { -1, 0, 1, 1, 0, -1 };

    for ( const auto& [oq, orr, os] : bodyHexes( unit ) ) {
        for ( int i = 0; i < 6; ++i ) {
            try {
                const Hex& nhex = board.getHex( oq + DQ[i], orr + DR[i], os + DS[i] );
                if ( ! nhex.hasUnit( ) ) {
                    continue;
}
                const std::shared_ptr<Unit>& neighbour = nhex.getUnit( );
                if ( neighbour.get( ) == &unit ) {
                    continue;
}
                if ( is_enemy && is_enemy( *neighbour ) ) {
                    return true;
}
            } catch ( const std::out_of_range& ) {}
        }
    }
    return false;
}

bool ActionManager::canShoot( const Unit& attacker,
                               const Unit& defender,
                               const EnemyPredicate& is_enemy,
                               const Board& board ) const {
    if ( ! attacker.isRanged( ) || attacker.getAmmo( ) <= 0 ) {
        return false;
}
    if ( areUnitsAdjacent( attacker, defender ) ) {
        return false;
}
    if ( isBlockedByAdjacentEnemy( attacker, is_enemy, board ) ) {
        return false;
}
    return true;
}

std::vector<const Hex*>
ActionManager::findPath( const Unit& unit, const Hex& dest_hex, const Board& board ) const {
    using Coord = std::tuple<int, int, int>;
    const Coord start{ unit.getQ( ), unit.getR( ), unit.getS( ) };
    const Coord goal{ dest_hex.getQ( ), dest_hex.getR( ), dest_hex.getS( ) };

    if ( start == goal ) {
        try {
            const Hex& s =
                board.getHex( std::get<0>( start ), std::get<1>( start ), std::get<2>( start ) );
            return { &s };
        } catch ( const std::out_of_range& ) {
            return { };
        }
    }

    if ( unit.ignoresPathBlockers( ) ) {
        try {
            const Hex& s =
                board.getHex( std::get<0>( start ), std::get<1>( start ), std::get<2>( start ) );
            const Hex& d =
                board.getHex( std::get<0>( goal ), std::get<1>( goal ), std::get<2>( goal ) );
            return { &s, &d };
        } catch ( const std::out_of_range& ) {
            return { };
        }
    }

    std::map<Coord, Coord> parent;
    std::map<Coord, int> dist;
    std::queue<Coord> q;
    q.push( start );
    dist[start] = 0;

    static constexpr int DQ[] = { 1, 1, 0, -1, -1, 0 };
    static constexpr int DR[] = { 0, -1, -1, 0, 1, 1 };
    static constexpr int DS[] = { -1, 0, 1, 1, 0, -1 };

    bool found = false;
    while ( ! q.empty( ) && ! found ) {
        const Coord cur = q.front( );
        q.pop( );
        const int d = dist[cur];
        if ( d >= unit.getSpeed( ) ) {
            continue;
}

        for ( int i = 0; i < 6; ++i ) {
            const int nq = std::get<0>( cur ) + DQ[i];
            const int nr = std::get<1>( cur ) + DR[i];
            const int ns = std::get<2>( cur ) + DS[i];
            const Coord next{ nq, nr, ns };
            if ( dist.count( next ) ) {
                continue;
}
            if ( ! canOccupy( unit, nq, nr, ns, board ) ) {
                continue;
}

            dist[next] = d + 1;
            parent[next] = cur;
            if ( next == goal ) {
                found = true;
                break;
            }
            q.push( next );
        }
    }

    if ( ! found ) {
        return { };
}

    std::vector<const Hex*> chain;
    Coord cur = goal;
    while ( true ) {
        try {
            chain.push_back(
                &board.getHex( std::get<0>( cur ), std::get<1>( cur ), std::get<2>( cur ) ) );
        } catch ( const std::out_of_range& ) {
            return { };
        }
        if ( cur == start ) {
            break;
}
        const auto it = parent.find( cur );
        if ( it == parent.end( ) ) {
            return { };
}
        cur = it->second;
    }
    std::reverse( chain.begin( ), chain.end( ) );
    return chain;
}

std::vector<Hex*> ActionManager::getAvailableDestinations( const Unit& unit,
                                                             const Board& board ) const {
    std::vector<Hex*> destinations;

    if ( unit.ignoresPathBlockers( ) ) {
        const int sq = unit.getQ( );
        const int sr = unit.getR( );
        const int ss = unit.getS( );
        const int range = unit.getSpeed( );
        for ( const Hex& hex : board.getGrid( ) ) {
            const int hq = hex.getQ( );
            const int hr = hex.getR( );
            const int hs = hex.getS( );
            if ( hq == sq && hr == sr && hs == ss ) {
                continue;
}
            const int d =
                std::max( { std::abs( hq - sq ), std::abs( hr - sr ), std::abs( hs - ss ) } );
            if ( d > range ) {
                continue;
}
            if ( ! canOccupy( unit, hq, hr, hs, board ) ) {
                continue;
}
            destinations.push_back( const_cast<Hex*>( &hex ) );
        }
        return destinations;
    }

    std::set<std::tuple<int, int, int>> visited;
    std::queue<std::pair<std::tuple<int, int, int>, int>> q;

    try {
        const Hex& start_hex = board.getHex( unit.getQ( ), unit.getR( ), unit.getS( ) );
        q.push(
            { { start_hex.getQ( ), start_hex.getR( ), start_hex.getS( ) }, unit.getSpeed( ) } );
        visited.insert( { start_hex.getQ( ), start_hex.getR( ), start_hex.getS( ) } );
    } catch ( const std::out_of_range& ) {
        return destinations;
    }

    const int dq[] = { 1, 1, 0, -1, -1, 0 };
    const int dr[] = { 0, -1, -1, 0, 1, 1 };
    const int ds[] = { -1, 0, 1, 1, 0, -1 };

    while ( ! q.empty( ) ) {
        auto [current_coords, current_speed] = q.front( );
        q.pop( );
        int cq = std::get<0>( current_coords );
        int cr = std::get<1>( current_coords );
        int cs = std::get<2>( current_coords );

        try {
            const Hex& hex = board.getHex( cq, cr, cs );

            const bool at_start =
                ( cq == unit.getQ( ) && cr == unit.getR( ) && cs == unit.getS( ) );

            const bool valid_stand = at_start || canOccupy( unit, cq, cr, cs, board );

            if ( valid_stand ) {
                if ( ! at_start ) {
                    destinations.push_back( const_cast<Hex*>( &hex ) );
                }

                if ( current_speed > 0 ) {
                    for ( int i = 0; i < 6; ++i ) {
                        int nq = cq + dq[i];
                        int nr = cr + dr[i];
                        int ns = cs + ds[i];

                        if ( visited.find( { nq, nr, ns } ) != visited.end( ) ) {
                            continue;
}
                        try {
                            (void) board.getHex( nq, nr, ns );
                        } catch ( const std::out_of_range& ) {
                            continue;
                        }

                        if ( ! canOccupy( unit, nq, nr, ns, board ) ) {
                            continue;
}

                        visited.insert( { nq, nr, ns } );
                        q.push( { { nq, nr, ns }, current_speed - 1 } );
                    }
                }
            }
        } catch ( const std::out_of_range& ) {}
    }

    return destinations;
}

std::vector<std::pair<Unit*, Hex*>>
ActionManager::getAvailableAttacks( const Unit& unit, const Board& board ) const {
    std::vector<std::pair<Unit*, Hex*>> attacks;

    const int dq[] = { 1, 1, 0, -1, -1, 0 };
    const int dr[] = { 0, -1, -1, 0, 1, 1 };
    const int ds[] = { -1, 0, 1, 1, 0, -1 };

    std::vector<std::tuple<int, int, int>> origins;
    origins.emplace_back( unit.getQ( ), unit.getR( ), unit.getS( ) );
    if ( unit.getSize( ) == 2 ) {
        auto [tdq, tdr, tds] = tailDelta( unit );
        origins.emplace_back( unit.getQ( ) + tdq, unit.getR( ) + tdr, unit.getS( ) + tds );
    }

    std::set<Unit*> seen;
    for ( const auto& [oq, orr, os] : origins ) {
        for ( int i = 0; i < 6; ++i ) {
            int nq = oq + dq[i];
            int nr = orr + dr[i];
            int ns = os + ds[i];
            try {
                const Hex& hex = board.getHex( nq, nr, ns );
                if ( ! hex.hasUnit( ) ) {
                    continue;
}
                std::shared_ptr<Unit> target = hex.getUnit( );
                if ( target.get( ) == &unit ) {
                    continue;
}
                if ( seen.insert( target.get( ) ).second ) {
                    attacks.push_back( { target.get( ), const_cast<Hex*>( &hex ) } );
                }
            } catch ( const std::out_of_range& ) {}
        }
    }
    return attacks;
}

void ActionManager::move( Unit& unit, Hex& dest_hex, Board& board ) {
    Hex& start_hex = board.getHex( unit.getQ( ), unit.getR( ), unit.getS( ) );
    std::shared_ptr<Unit> unit_ptr = start_hex.getUnit( );
    if ( ! unit_ptr || unit_ptr.get( ) != &unit ) {
        throw std::logic_error( "Unit coordinates and Board state are out of sync" );
    }

    if ( unit.getSize( ) == 2 ) {
        auto [dq, dr, ds] = tailDelta( unit );

        try {
            Hex& start_tail = board.getHex(
                start_hex.getQ( ) + dq, start_hex.getR( ) + dr, start_hex.getS( ) + ds );
            start_tail.removeUnit( );
        } catch ( const std::out_of_range& ) {}

        Hex& dest_tail =
            board.getHex( dest_hex.getQ( ) + dq, dest_hex.getR( ) + dr, dest_hex.getS( ) + ds );

        const bool dest_blocked = dest_hex.hasUnit( ) && dest_hex.getUnit( ).get( ) != &unit;
        const bool tail_blocked = dest_tail.hasUnit( ) && dest_tail.getUnit( ).get( ) != &unit;
        if ( dest_blocked || tail_blocked ) {
            throw std::runtime_error( "Destination hex already has a unit" );
        }

        start_hex.removeUnit( );
        dest_hex.setUnit( unit_ptr );
        dest_tail.setUnit( unit_ptr );
        unit.setPosition( dest_hex.getQ( ), dest_hex.getR( ), dest_hex.getS( ) );
        return;
    }

    if ( dest_hex.hasUnit( ) ) {
        throw std::runtime_error( "Destination hex already has a unit" );
    }
    dest_hex.setUnit( unit_ptr );
    start_hex.removeUnit( );
    unit.setPosition( dest_hex.getQ( ), dest_hex.getR( ), dest_hex.getS( ) );
}

bool ActionManager::attack( Unit& attacker, Unit& defender, Hex& attack_from_hex, Board& board ) {
    if ( attack_from_hex.hasUnit( ) && attack_from_hex.getUnit( ).get( ) != &attacker ) {
        throw std::runtime_error( "Attack hex already has a unit" );
    }

    try {
        Hex& current = board.getHex( attacker.getQ( ), attacker.getR( ), attacker.getS( ) );
        if ( &current != &attack_from_hex ) {
            move( attacker, attack_from_hex, board );
        }
    } catch ( std::out_of_range& ) {}

    if ( attacker.getQ( ) < defender.getQ( ) ) {
        defender.setVisualFacingLeft( true );
    } else if ( attacker.getQ( ) > defender.getQ( ) ) {
        defender.setVisualFacingLeft( false );
    }

    int damage = calculateDamage( attacker, defender );
    if ( attacker.isRanged( ) ) {
        damage /= 2;
}
    defender.takeDamage( damage );

    // Blind break: any successful hit dispels Blind on the defender
    // and primes its next retaliation for 50% attack (spec).
    if ( defender.getCount( ) > 0 && defender.hasBuff( models::BuffType::BLIND ) ) {
        defender.removeBuff( models::BuffType::BLIND );
        defender.setNextRetaliationHalfAttack( true );
    }

    if ( defender.getCount( ) == 0 ) {
        try {
            Hex& def_hex = board.getHex( defender.getQ( ), defender.getR( ), defender.getS( ) );
            def_hex.unitDied( );
            if ( defender.getSize( ) == 2 ) {
                auto [dq, dr, ds] = tailDelta( defender );
                try {
                    Hex& def_tail = board.getHex(
                        defender.getQ( ) + dq, defender.getR( ) + dr, defender.getS( ) + ds );

                    def_tail.unitDied( );
                } catch ( std::out_of_range& ) {}
            }
        } catch ( std::out_of_range& ) {}
        return true;
    }

    if ( ! defender.hasRetaliatedThisRound( ) && areUnitsAdjacent( attacker, defender ) ) {
        if ( defender.getQ( ) < attacker.getQ( ) ) {
            attacker.setVisualFacingLeft( true );
        } else if ( defender.getQ( ) > attacker.getQ( ) ) {
            attacker.setVisualFacingLeft( false );
        }

        int counter = calculateDamage( defender, attacker );
        if ( defender.getNextRetaliationHalfAttack( ) ) {
            counter /= 2;
            defender.setNextRetaliationHalfAttack( false );
        }
        attacker.takeDamage( counter );
        defender.setRetaliated( true );

        if ( attacker.getCount( ) == 0 ) {
            try {
                Hex& atk_hex =
                    board.getHex( attacker.getQ( ), attacker.getR( ), attacker.getS( ) );
                atk_hex.unitDied( );
                if ( attacker.getSize( ) == 2 ) {
                    auto [dq, dr, ds] = tailDelta( attacker );
                    try {
                        Hex& atk_tail = board.getHex( attacker.getQ( ) + dq,
                                                       attacker.getR( ) + dr,
                                                       attacker.getS( ) + ds );

                        atk_tail.unitDied( );
                    } catch ( std::out_of_range& ) {}
                }
            } catch ( std::out_of_range& ) {}
        }
    }

    return false;
}

void ActionManager::defend( Unit& unit ) {
    unit.applyBuff( BuffFactory::createDefendBuff( ) );
}

int ActionManager::calculateDamage( const Unit& attacker, const Unit& defender ) const {
    return calculateDamageWithAttack( attacker, defender, attacker.getAttack( ) );
}

int ActionManager::calculateDamageWithAttack( const Unit& attacker,
                                                      const Unit& defender,
                                                      int attack_override ) const {
    if ( attacker.getCount( ) <= 0 ) {
        return 0;
}

    const int dmg_min = std::max( 0, attacker.getDamageMin( ) );
    const int dmg_max = std::max( dmg_min, attacker.getDamageMax( ) );

    double total_base_damage = 0.0;
    if ( deterministicDamage_ ) {
        // Expected damage: every creature deals the range average.
        total_base_damage =
            attacker.getCount( ) * ( dmg_min + dmg_max ) / 2.0;
    } else {
        std::random_device rd;
        std::mt19937 gen( rd( ) );
        std::uniform_int_distribution<> distrib( dmg_min, dmg_max );
        for ( int i = 0; i < attacker.getCount( ); ++i ) {
            total_base_damage += distrib( gen );
        }
    }

    double modifier = 1.0;
    int attack_stat = attack_override;
    int defense_stat = defender.getDefense( );

    if ( attack_stat > defense_stat ) {
        modifier += 0.05 * ( attack_stat - defense_stat );
        if ( modifier > 4.0 ) {
            modifier = 4.0;
}
    } else if ( attack_stat < defense_stat ) {
        modifier -= 0.025 * ( defense_stat - attack_stat );
        if ( modifier < 0.3 ) {
            modifier = 0.3;
}
    }

    int damage = static_cast<int>( total_base_damage * modifier );

    // Shield: reduce incoming MELEE damage by the defender's buff
    // multiplier (ranged attacks ignore Shield).
    if ( ! attacker.isRanged( ) ) {
        damage = static_cast<int>(
            static_cast<float>( damage ) * defender.getIncomingMeleeMultiplier( ) );
    }
    return damage;
}

bool ActionManager::shoot( Unit& attacker, Unit& defender, Board& board ) {
    if ( ! attacker.isRanged( ) || attacker.getAmmo( ) <= 0 ) {
        throw std::logic_error( "shoot() called on a unit that cannot shoot" );
    }

    if ( attacker.getQ( ) < defender.getQ( ) ) {
        attacker.setVisualFacingLeft( false );
    } else if ( attacker.getQ( ) > defender.getQ( ) ) {
        attacker.setVisualFacingLeft( true );
    }

    int damage = calculateDamage( attacker, defender );
    if ( hexDistance( attacker, defender ) > 10 ) {
        damage /= 2;
    }
    defender.takeDamage( damage );
    attacker.decrementAmmo( );

    // Ranged hits also break Blind (spec: any damage dispels it).
    // No retaliation happens for ranged, so the half-attack flag is
    // effectively moot here, but we still set it for consistency.
    if ( defender.getCount( ) > 0 && defender.hasBuff( models::BuffType::BLIND ) ) {
        defender.removeBuff( models::BuffType::BLIND );
        defender.setNextRetaliationHalfAttack( true );
    }

    if ( defender.getCount( ) == 0 ) {
        try {
            Hex& def_hex = board.getHex( defender.getQ( ), defender.getR( ), defender.getS( ) );
            def_hex.unitDied( );
            if ( defender.getSize( ) == 2 ) {
                auto [dq, dr, ds] = tailDelta( defender );
                try {
                    Hex& def_tail = board.getHex(
                        defender.getQ( ) + dq, defender.getR( ) + dr, defender.getS( ) + ds );
                    def_tail.unitDied( );
                } catch ( std::out_of_range& ) {}
            }
        } catch ( std::out_of_range& ) {}
        return true;
    }
    return false;
}

} // namespace core
