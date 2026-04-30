/**
 * @file GameManager.cc
 * @brief Implementation of the battle facade and morale rolls.
 */
#include "GameManager.h"
#include <algorithm>
#include <random>
#include <tuple>

namespace core {

using models::Board;
using models::Hero;
using models::Hex;
using models::Unit;

namespace {

std::tuple<int, int, int> tail_delta( const Unit& u ) {
    if ( u.is_facing_left( ) )
        return { 1, 0, -1 };
    return { -1, 0, 1 };
}

void place_unit_on_board( Board& board, const std::shared_ptr<Unit>& unit ) {
    if ( ! unit )
        return;
    try {
        board.get_hex( unit->get_q( ), unit->get_r( ), unit->get_s( ) ).set_unit( unit );
    } catch ( std::out_of_range& ) {}
    if ( unit->get_size( ) == 2 ) {
        auto [dq, dr, ds] = tail_delta( *unit );
        try {
            board.get_hex( unit->get_q( ) + dq, unit->get_r( ) + dr, unit->get_s( ) + ds )
                .set_unit( unit );
        } catch ( std::out_of_range& ) {}
    }
}

} // namespace

GameManager::GameManager( Hero blue_hero, Hero red_hero )
    : blue_hero( std::move( blue_hero ) ),
      red_hero( std::move( red_hero ) ),
      round_manager( all_units_in_battle ) {
    for ( const auto& unit_ptr : this->blue_hero.get_army( ).get_units( ) ) {
        if ( unit_ptr ) {
            all_units_in_battle.push_back( unit_ptr.get( ) );
            place_unit_on_board( board, unit_ptr );
        }
    }

    for ( const auto& unit_ptr : this->red_hero.get_army( ).get_units( ) ) {
        if ( unit_ptr ) {
            all_units_in_battle.push_back( unit_ptr.get( ) );
            place_unit_on_board( board, unit_ptr );
        }
    }

    round_manager.start_round( );
}

Unit* GameManager::get_current_unit( ) {
    const std::size_t before = round_manager.get_units_left_in_round( ).size( );
    Unit* u = round_manager.get_current_unit( );
    if ( u != nullptr && before == 0 ) {
        ++round_number;
    }

    if ( round_number != morale_round_tracked ) {
        morale_rolled_units_this_round.clear( );
        morale_round_tracked = round_number;
    }

    roll_morale_for_active( u );
    return u;
}

void GameManager::roll_morale_for_active( Unit* unit ) {
    if ( unit == nullptr ) {
        last_morale_rolled_unit = nullptr;
        morale_triggered_this_turn = false;
        return;
    }
    if ( unit == last_morale_rolled_unit ) {
        return;
    }

    if ( morale_rolled_units_this_round.count( unit ) > 0 ) {
        morale_triggered_this_turn = false;
        last_morale_rolled_unit = unit;
        return;
    }

    static thread_local std::mt19937 rng{ std::random_device{ }( ) };
    std::uniform_int_distribution<int> dist( 0, 99 );
    morale_triggered_this_turn = ( dist( rng ) < 10 );
    last_morale_rolled_unit = unit;
    morale_rolled_units_this_round.insert( unit );
}

void GameManager::consume_turn_or_burn_morale_bonus( ) {
    if ( morale_triggered_this_turn ) {
        morale_triggered_this_turn = false;
        return;
    }
    round_manager.end_current_unit_turn( );
    last_morale_rolled_unit = nullptr;
}

std::vector<Unit*> GameManager::get_units_left_in_round( ) const {
    return round_manager.get_units_left_in_round( );
}

std::vector<Unit*> GameManager::get_unit_queue_in_round( ) const {
    return round_manager.get_unit_queue_in_round( );
}

std::vector<Hex*> GameManager::get_available_destinations( const Unit& unit ) const {
    return action_manager.get_available_destinations( unit, board );
}

std::vector<const Hex*> GameManager::find_path( const Unit& unit, const Hex& dest_hex ) const {
    return action_manager.find_path( unit, dest_hex, board );
}

std::vector<Unit*> GameManager::peek_next_round_order( ) const {
    std::vector<Unit*> next;
    next.reserve( all_units_in_battle.size( ) );
    for ( Unit* u : all_units_in_battle ) {
        if ( u != nullptr && u->get_count( ) > 0 )
            next.push_back( u );
    }
    std::sort( next.begin( ), next.end( ), []( const Unit* a, const Unit* b ) {
        if ( a->get_speed( ) != b->get_speed( ) )
            return a->get_speed( ) > b->get_speed( );
        return a < b;
    } );
    return next;
}

std::vector<std::pair<Unit*, Hex*>> GameManager::get_available_attacks( const Unit& unit ) const {
    std::vector<std::pair<Unit*, Hex*>> filtered;
    for ( const auto& [target, hex] : action_manager.get_available_attacks( unit, board ) ) {
        if ( target == nullptr || hex == nullptr ) {
            continue;
        }
        if ( are_enemies( unit, *target ) ) {
            filtered.push_back( { target, hex } );
        }
    }
    return filtered;
}

bool GameManager::hero_contains_unit( const Hero& hero, const Unit& unit ) const {
    for ( const auto& candidate : hero.get_army( ).get_units( ) ) {
        if ( candidate && candidate.get( ) == &unit ) {
            return true;
        }
    }
    return false;
}

bool GameManager::are_allies( const Unit& first, const Unit& second ) const {
    const bool first_blue = hero_contains_unit( blue_hero, first );
    const bool second_blue = hero_contains_unit( blue_hero, second );
    if ( first_blue && second_blue ) {
        return true;
    }

    const bool first_red = hero_contains_unit( red_hero, first );
    const bool second_red = hero_contains_unit( red_hero, second );
    if ( first_red && second_red ) {
        return true;
    }

    return false;
}

bool GameManager::are_enemies( const Unit& first, const Unit& second ) const {
    const bool first_known =
        hero_contains_unit( blue_hero, first ) || hero_contains_unit( red_hero, first );
    const bool second_known =
        hero_contains_unit( blue_hero, second ) || hero_contains_unit( red_hero, second );
    if ( ! first_known || ! second_known ) {
        return &first != &second;
    }
    return ! are_allies( first, second );
}

bool GameManager::can_attack( const Unit& attacker, const Hex& target_hex ) const {
    const auto attacks = get_available_attacks( attacker );
    Unit* clicked_target = nullptr;
    if ( target_hex.has_unit( ) ) {
        clicked_target = target_hex.get_unit( ).get( );
    }

    for ( const auto& [target, hex] : attacks ) {
        if ( target == nullptr ) {
            continue;
        }

        if ( clicked_target != nullptr && target == clicked_target ) {
            return true;
        }
        if ( hex == &target_hex ) {
            return true;
        }
    }
    return false;
}

bool GameManager::can_move( const Unit& unit, const Hex& dest_hex ) const {
    const auto destinations = get_available_destinations( unit );
    for ( const Hex* hex : destinations ) {
        if ( hex == &dest_hex ) {
            return true;
        }
    }
    return false;
}

void GameManager::next_turn( ) {}

void GameManager::move( Unit& unit, Hex& dest_hex ) {
    action_manager.move( unit, dest_hex, board );
    consume_turn_or_burn_morale_bonus( );
}

void GameManager::attack( Unit& attacker, Unit& defender, Hex& attack_from_hex ) {
    const auto enemy_predicate = [this, &attacker]( const Unit& other ) {
        return are_enemies( attacker, other );
    };

    bool defender_died = false;
    if ( action_manager.can_shoot( attacker, defender, enemy_predicate, board ) ) {
        defender_died = action_manager.shoot( attacker, defender, board );
    } else {
        defender_died = action_manager.attack( attacker, defender, attack_from_hex, board );
    }

    if ( defender_died ) {
        remove_dead_unit( defender );
    }
    consume_turn_or_burn_morale_bonus( );
}

bool GameManager::will_shoot( const Unit& attacker, const Unit& defender ) const {
    const auto enemy_predicate = [this, &attacker]( const Unit& other ) {
        return are_enemies( attacker, other );
    };
    return action_manager.can_shoot( attacker, defender, enemy_predicate, board );
}

void GameManager::wait( Unit& unit ) {
    morale_triggered_this_turn = false;
    last_morale_rolled_unit = nullptr;
    round_manager.wait_current_unit( );
}

void GameManager::defend( Unit& unit ) {
    action_manager.defend( unit );
    consume_turn_or_burn_morale_bonus( );
}

void GameManager::remove_dead_unit( Unit& unit ) {
    auto it = std::find( all_units_in_battle.begin( ), all_units_in_battle.end( ), &unit );
    if ( it != all_units_in_battle.end( ) ) {
        all_units_in_battle.erase( it );
    }
}

} // namespace core
