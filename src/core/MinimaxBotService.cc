/**
 * @file MinimaxBotService.cc
 * @brief Implementation of the alpha-beta minimax battle AI.
 * @author Łukasz Szydlik
 */
#include "MinimaxBotService.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../models/Buff.h"
#include "../models/Hero.h"
#include "../models/Hex.h"
#include "../models/Unit.h"
#include "ActionGenerator.h"
#include "GameManager.h"
#include "SpellResolver.h"

namespace core {

using models::Hero;
using models::Hex;
using models::Unit;

namespace {

constexpr double K_INF = std::numeric_limits<double>::infinity( );
constexpr double K_WIN = 1.0e9;     // terminal bonus/penalty for a wipe
constexpr double K_W_MANA = 10.0;   // value of one mana point
constexpr double K_W_POS = 5.0;     // penalty per hex between us and the enemy
constexpr int K_MAX_MOVES_KEPT = 6; // pure (non-attacking) moves kept per node
// Safety cap on branching. Every melee approach hex is kept (attack
// direction matters), so this is set generously and only trims runaway
// nodes; ordering ensures the strongest actions survive a trim.
constexpr int K_MAX_CHILDREN = 40;
constexpr long K_NODE_BUDGET = 300000; // safety cutoff for the whole search

int hexDistance( int aq, int ar, int bq, int br ) {
    const int as = -aq - ar;
    const int bs = -bq - br;
    return std::max( { std::abs( aq - bq ), std::abs( ar - br ), std::abs( as - bs ) } );
}

// Combat worth of a stack: total remaining HP weighted by unit quality.
double stackValue( const Unit& u ) {
    const double hp_total =
        static_cast<double>( ( u.getCount( ) - 1 ) * u.getHealth( ) + u.getHealthLeft( ) );
    double quality = u.getAttack( ) + u.getDefense( ) +
                     ( u.getDamageMin( ) + u.getDamageMax( ) ) / 2.0 + u.getSpeed( );
    if ( u.isRanged( ) && u.getAmmo( ) > 0 ) {
        quality *= 1.3; // shooters are worth more
    }
    double value = hp_total * std::max( 1.0, quality );
    if ( u.hasBuff( models::BuffType::BLIND ) || u.getSpeed( ) == 0 ) {
        value *= 0.5; // can't act -> less of a threat / asset
    }
    return value;
}

// Preference order used to order children so alpha-beta prunes well.
int categoryRank( ActionType type ) {
    switch ( type ) {
    case ActionType::CAST_SPELL:
        return 0;
    case ActionType::RANGED_ATTACK:
        return 1;
    case ActionType::MELEE_ATTACK:
        return 2;
    case ActionType::MOVE:
        return 3;
    case ActionType::WAIT:
        return 4;
    case ActionType::DEFEND:
        return 5;
    }
    return 6;
}

int nearestEnemyDistance( GameManager& gm, const Unit& actor, int q, int r ) {
    int best = 999;
    for ( Unit* u : gm.getAllUnits( ) ) {
        if ( u == nullptr || u->getCount( ) <= 0 ) {
            continue;
        }
        if ( ! gm.areEnemies( actor, *u ) ) {
            continue;
        }
        best = std::min( best, hexDistance( q, r, u->getQ( ), u->getR( ) ) );
    }
    return best;
}

// Hexes a unit (head + tail for 2-hex units) would occupy at a head hex,
// using the unit's current facing for the tail side.
std::vector<std::pair<int, int>> bodyHexesAt( const Unit& u, int head_q, int head_r ) {
    std::vector<std::pair<int, int>> body;
    body.emplace_back( head_q, head_r );
    if ( u.getSize( ) == 2 ) {
        const int tail_dq = u.isFacingLeft( ) ? 1 : -1;
        body.emplace_back( head_q + tail_dq, head_r );
    }
    return body;
}

// How many enemy stacks OTHER than @p target would be adjacent to @p actor
// if it struck from (@p dest_q, @p dest_r) -- i.e. how exposed that attack
// direction leaves us to other attackers next turn. Lower is safer.
int exposureAfterApproach( GameManager& gm, const Unit& actor, const Unit& target, int dest_q,
                                  int dest_r ) {
    const std::vector<std::pair<int, int>> attacker_body = bodyHexesAt( actor, dest_q, dest_r );
    int exposed = 0;
    for ( Unit* u : gm.getAllUnits( ) ) {
        if ( u == nullptr || u->getCount( ) <= 0 || u == &target || u == &actor ) {
            continue;
        }
        if ( ! gm.areEnemies( actor, *u ) ) {
            continue;
        }
        const std::vector<std::pair<int, int>> enemy_body =
            bodyHexesAt( *u, u->getQ( ), u->getR( ) );
        bool adjacent = false;
        for ( const auto& a : attacker_body ) {
            for ( const auto& b : enemy_body ) {
                if ( hexDistance( a.first, a.second, b.first, b.second ) == 1 ) {
                    adjacent = true;
                    break;
                }
            }
            if ( adjacent ) {
                break;
            }
        }
        if ( adjacent ) {
            ++exposed;
        }
    }
    return exposed;
}

Unit* findUnitAt( GameManager& gm, int q, int r ) {
    try {
        const Hex& hex = gm.getBoard( ).getHex( q, r, -q - r );
        if ( hex.hasUnit( ) ) {
            return hex.getUnit( ).get( );
        }
    } catch ( const std::out_of_range& ) {}
    return nullptr;
}

bool sideWiped( GameManager& gm ) {
    bool blue_alive = false;
    bool red_alive = false;
    for ( Unit* u : gm.getAllUnits( ) ) {
        if ( u == nullptr || u->getCount( ) <= 0 ) {
            continue;
        }
        const int side = gm.sideOfUnit( *u );
        if ( side == 0 ) {
            blue_alive = true;
        } else if ( side == 1 ) {
            red_alive = true;
        }
    }
    return ! blue_alive || ! red_alive;
}

} // namespace

MinimaxBotService::MinimaxBotService( GameManager& model, int depth )
    : model_( model ), depth_( std::max( 1, depth ) ) {}

double MinimaxBotService::evaluate( GameManager& state, int our_side ) const {
    double score = 0.0;
    int our_alive = 0;
    int enemy_alive = 0;

    for ( Unit* u : state.getAllUnits( ) ) {
        if ( u == nullptr || u->getCount( ) <= 0 ) {
            continue;
        }
        const int side = state.sideOfUnit( *u );
        const double value = stackValue( *u );
        if ( side == our_side ) {
            score += value;
            ++our_alive;
            // Encourage closing the distance with melee stacks.
            if ( ! u->isRanged( ) ) {
                score -= K_W_POS *
                         nearestEnemyDistance( state, *u, u->getQ( ), u->getR( ) );
            }
        } else if ( side >= 0 ) {
            score -= value;
            ++enemy_alive;
        }
    }

    if ( enemy_alive == 0 ) {
        score += K_WIN;
    }
    if ( our_alive == 0 ) {
        score -= K_WIN;
    }

    const Hero& our_hero = ( our_side == 0 ) ? state.getBlueHero( ) : state.getRedHero( );
    const Hero& enemy_hero = ( our_side == 0 ) ? state.getRedHero( ) : state.getBlueHero( );
    score += K_W_MANA * ( our_hero.getCurrentMana( ) - enemy_hero.getCurrentMana( ) );

    return score;
}

std::vector<ActionCommand> MinimaxBotService::generateAndPrune( GameManager& state,
                                                                        ActionGenerator& generator,
                                                                        Unit& actor ) const {
    const std::vector<ActionCommand> all = generator.generate( actor );

    std::vector<ActionCommand> kept;        // melee (all approaches) + ranged + casts
    std::unordered_map<int, ActionCommand> cast_by_spell;
    std::unordered_map<int, double> cast_score;
    std::vector<ActionCommand> moves;
    bool has_wait = false;
    bool has_defend = false;

    for ( const ActionCommand& a : all ) {
        switch ( a.type_ ) {
        case ActionType::MELEE_ATTACK:
            // Keep EVERY approach hex -- the direction we strike from
            // decides which other enemies can reach us next turn, and the
            // deeper search is what tells those approaches apart.
            if ( a.target_ != nullptr ) {
                kept.push_back( a );
            }
            break;
        case ActionType::RANGED_ATTACK:
            kept.push_back( a );
            break;
        case ActionType::CAST_SPELL: {
            const int key = static_cast<int>( a.spellId_ );
            const double s = ( a.target_ != nullptr ) ? stackValue( *a.target_ ) : 0.0;
            const auto it = cast_score.find( key );
            if ( it == cast_score.end( ) || s > it->second ) {
                cast_score[key] = s;
                cast_by_spell[key] = a; // best target per spell
            }
            break;
        }
        case ActionType::MOVE:
            moves.push_back( a );
            break;
        case ActionType::WAIT:
            has_wait = true;
            break;
        case ActionType::DEFEND:
            has_defend = true;
            break;
        }
    }

    for ( const auto& [key, command] : cast_by_spell ) {
        kept.push_back( command );
    }

    std::sort( moves.begin( ), moves.end( ),
               [&state, &actor]( const ActionCommand& a, const ActionCommand& b ) {
                   return nearestEnemyDistance( state, actor, a.destQ_, a.destR_ ) <
                          nearestEnemyDistance( state, actor, b.destQ_, b.destR_ );
               } );
    const int move_n = std::min<int>( K_MAX_MOVES_KEPT, static_cast<int>( moves.size( ) ) );
    for ( int i = 0; i < move_n; ++i ) {
        kept.push_back( moves[i] );
    }

    if ( has_wait ) {
        ActionCommand wait_cmd;
        wait_cmd.type_ = ActionType::WAIT;
        kept.push_back( wait_cmd );
    }
    if ( has_defend ) {
        ActionCommand defend_cmd;
        defend_cmd.type_ = ActionType::DEFEND;
        kept.push_back( defend_cmd );
    }

    // Order children for alpha-beta: best category first, and within a
    // category higher target value first -- with melee broken further by
    // the safest approach (fewest other enemies left adjacent to us).
    // Priorities are precomputed once so the heavier melee exposure check
    // isn't repeated in every comparison.
    std::vector<double> priority( kept.size( ) );
    for ( std::size_t i = 0; i < kept.size( ); ++i ) {
        const ActionCommand& c = kept[i];
        double p = ( c.target_ != nullptr ) ? stackValue( *c.target_ ) : 0.0;
        if ( c.type_ == ActionType::MELEE_ATTACK && c.target_ != nullptr ) {
            p -= exposureAfterApproach( state, actor, *c.target_, c.destQ_, c.destR_ );
        } else if ( c.type_ == ActionType::MOVE ) {
            p = -nearestEnemyDistance( state, actor, c.destQ_, c.destR_ );
        }
        priority[i] = p;
    }

    std::vector<int> order( kept.size( ) );
    for ( std::size_t i = 0; i < kept.size( ); ++i ) {
        order[i] = static_cast<int>( i );
    }
    std::sort( order.begin( ), order.end( ), [&]( int a, int b ) {
        const int ra = categoryRank( kept[a].type_ );
        const int rb = categoryRank( kept[b].type_ );
        if ( ra != rb ) {
            return ra < rb;
        }
        return priority[a] > priority[b];
    } );

    std::vector<ActionCommand> ordered;
    const int limit = std::min<int>( K_MAX_CHILDREN, static_cast<int>( order.size( ) ) );
    ordered.reserve( limit );
    for ( int i = 0; i < limit; ++i ) {
        ordered.push_back( kept[order[i]] );
    }
    return ordered;
}

ActionCommand MinimaxBotService::translate( const ActionCommand& source,
                                                    GameManager& destination ) const {
    ActionCommand command = source;
    if ( source.target_ != nullptr ) {
        command.target_ =
            findUnitAt( destination, source.target_->getQ( ), source.target_->getR( ) );
    }
    return command;
}

bool MinimaxBotService::applyAction( GameManager& state, SpellResolver& resolver,
                                            const ActionCommand& command ) const {
    Unit* actor = state.getCurrentUnit( );
    if ( actor == nullptr ) {
        return false;
    }
    try {
        switch ( command.type_ ) {
        case ActionType::MOVE: {
            Hex& dest = state.getBoard( ).getHex(
                command.destQ_, command.destR_, -command.destQ_ - command.destR_ );
            state.move( *actor, dest );
            return true;
        }
        case ActionType::MELEE_ATTACK: {
            if ( command.target_ == nullptr ) {
                return false;
            }
            Hex& approach = state.getBoard( ).getHex(
                command.destQ_, command.destR_, -command.destQ_ - command.destR_ );
            state.attack( *actor, *command.target_, approach );
            return true;
        }
        case ActionType::RANGED_ATTACK: {
            if ( command.target_ == nullptr ) {
                return false;
            }
            Hex& from =
                state.getBoard( ).getHex( actor->getQ( ), actor->getR( ), actor->getS( ) );
            state.attack( *actor, *command.target_, from );
            return true;
        }
        case ActionType::WAIT:
            state.wait( *actor );
            return true;
        case ActionType::DEFEND:
            state.defend( *actor );
            return true;
        case ActionType::CAST_SPELL: {
            if ( command.target_ == nullptr ) {
                return false;
            }
            Hero* caster = state.getCasterFor( *actor );
            if ( caster == nullptr ) {
                return false;
            }
            resolver.tryCast( command.spellId_, *caster, *command.target_ );
            return true;
        }
        }
    } catch ( const std::exception& ) {
        return false;
    }
    return false;
}

double MinimaxBotService::alphaBeta( GameManager& state, int depth, double alpha, double beta,
                                            int our_side ) {
    if ( --nodeBudget_ <= 0 || depth <= 0 || sideWiped( state ) ) {
        return evaluate( state, our_side );
    }

    Unit* actor = state.getCurrentUnit( );
    if ( actor == nullptr ) {
        return evaluate( state, our_side );
    }

    SpellResolver resolver( state );
    ActionGenerator generator( state, resolver );
    const std::vector<ActionCommand> actions = generateAndPrune( state, generator, *actor );
    if ( actions.empty( ) ) {
        return evaluate( state, our_side );
    }

    const bool maximizing = ( state.sideOfUnit( *actor ) == our_side );
    double value = maximizing ? -K_INF : K_INF;
    bool any_child = false;

    for ( const ActionCommand& action : actions ) {
        std::unique_ptr<GameManager> child = state.clone( );
        SpellResolver child_resolver( *child );
        if ( ! applyAction( *child, child_resolver, translate( action, *child ) ) ) {
            continue;
        }
        any_child = true;
        const double child_value =
            alphaBeta( *child, depth - 1, alpha, beta, our_side );
        if ( maximizing ) {
            value = std::max( value, child_value );
            alpha = std::max( alpha, value );
        } else {
            value = std::min( value, child_value );
            beta = std::min( beta, value );
        }
        if ( beta <= alpha ) {
            break;
        }
    }

    return any_child ? value : evaluate( state, our_side );
}

std::optional<ActionCommand> MinimaxBotService::decideAction( Unit& active_unit ) {
    const int our_side = model_.sideOfUnit( active_unit );
    if ( our_side < 0 ) {
        return std::nullopt;
    }

    nodeBudget_ = K_NODE_BUDGET;
    std::unique_ptr<GameManager> root = model_.clone( );
    Unit* actor = root->getCurrentUnit( );
    if ( actor == nullptr ) {
        return std::nullopt;
    }

    SpellResolver root_resolver( *root );
    ActionGenerator root_generator( *root, root_resolver );
    const std::vector<ActionCommand> actions =
        generateAndPrune( *root, root_generator, *actor );
    if ( actions.empty( ) ) {
        return std::nullopt;
    }

    double best_value = -K_INF;
    double alpha = -K_INF;
    const ActionCommand* best = nullptr;

    for ( const ActionCommand& action : actions ) {
        std::unique_ptr<GameManager> child = root->clone( );
        SpellResolver child_resolver( *child );
        if ( ! applyAction( *child, child_resolver, translate( action, *child ) ) ) {
            continue;
        }
        const double value = alphaBeta( *child, depth_ - 1, alpha, K_INF, our_side );
        if ( best == nullptr || value > best_value ) {
            best_value = value;
            best = &action;
        }
        alpha = std::max( alpha, best_value );
    }

    if ( best == nullptr ) {
        return std::nullopt;
    }

    // Map the chosen action from the root clone back onto the live model
    // (positions are identical, so match the target by its head hex).
    ActionCommand result = *best;
    if ( best->target_ != nullptr ) {
        result.target_ =
            findUnitAt( model_, best->target_->getQ( ), best->target_->getR( ) );
        if ( result.target_ == nullptr ) {
            return std::nullopt;
        }
    }
    return result;
}

} // namespace core
