# Minimax + Alpha-Beta Bot — Summary

Minimax + alpha-beta bot is implemented, built (Debug **and** Release), tested
(59/59), and smoke-tested at depth 4.

## Depth & heuristic (the recommendation, now implemented)

- **Depth 4**, configurable via `"depth"` in the `player` block; clamped to ≥1,
  plus a **300k-node safety budget** so it can never stall indefinitely.

### Heuristic (evaluation function)

The evaluator scores a leaf state from the searching side's point of view. It is
deliberately **material-dominated** — in HoMM-style combat, how much army each
side has left is by far the strongest predictor of who wins, so positional
factors are only light tie-breakers.

**Per-stack value.** Each living stack is worth `effectiveHP × quality`:

```
effectiveHP = (count - 1) * maxHealth + healthLeft   // total HP remaining in the stack
quality     = attack + defense + (damageMin + damageMax) / 2 + speed
```

- **Why HP × quality, not just HP or just count?** A stack's worth is roughly
  "how much punishment it can absorb" (HP) times "how dangerous each surviving
  creature is" (quality). HP alone would value a wall of weak pikemen the same as
  an equal-HP wall of champions; count alone ignores wounded stacks. The product
  captures both, and since both armies are scored identically, only the
  *difference* matters — the absolute scale is irrelevant.
- **`quality` mixes offense, durability and tempo:** attack + defense + average
  damage + speed. Speed is included because a faster stack acts sooner and more
  often — genuine tempo value in an initiative-ordered game.
- **Shooter bonus ×1.3** (only while `ammo > 0`): ranged stacks deal damage
  without suffering retaliation and can strike from safety, so a live shooter is
  worth more than its raw stats imply. The bonus vanishes once ammo runs out (it
  must then fight in melee).
- **Disabled penalty ×0.5** when the stack is **blinded** (`hasBuff(BLIND)`) or
  has **speed 0**: a unit that cannot act is half the asset/threat it would
  otherwise be. This is the one effect not already captured by the stat totals —
  Bless / Curse / Stone Skin / Bloodlust etc. are *already* reflected, because
  `getAttack() / getDefense() / getDamage*()` return the buffed values, so the
  evaluator sees them for free.

**Aggregate score** (from our side):

```
score  =  Σ stackValue(ourStacks)  −  Σ stackValue(enemyStacks)
       +  (enemy wiped ?  +1e9 : 0)        // terminal win
       +  (we are wiped ?  −1e9 : 0)        // terminal loss
       +  10  * (ourHeroMana − enemyHeroMana)        // future spell potential
       −  5   * Σ dist(ourMeleeStack → nearestEnemy)  // encourage engaging
```

- **Terminal ±1e9** dwarfs every other term, so the search always takes a
  guaranteed wipe and always avoids being wiped, no matter the small material
  trades along the way.
- **Mana term (weight 10/point)** mildly prefers keeping mana (unspent casts are
  latent value) — small enough that it never overrides a real material swing.
- **Positional term (weight 5/hex)**, applied only to *our melee* stacks, gently
  penalizes standing far from the enemy. Without it, at shallow depth the bot can
  "dither" (no material change is visible within the horizon, so nothing it does
  looks better than anything else); this nudges melee stacks to close distance so
  contact — and real material evaluation — happens inside the search horizon. It
  is weak on purpose: one point of stack value usually outweighs many hexes.

### Pruning & move ordering

The raw legal-action set is huge (every reachable move hex, every spell × every
valid target — easily 80–150 actions), which would make depth-4 search
intractable. Two mechanisms tame it.

**1. Branching reduction** — `generateAndPrune()` collapses the full action list
into a curated subset, with a generous safety cap of **40 children per node**
(`K_MAX_CHILDREN`) that only trims runaway cases:

- **Melee:** keep **every reachable approach hex** for every target. The
  *direction* a melee stack strikes from decides which other enemy stacks can
  reach it on their turn, so the approaches are genuinely different decisions —
  collapsing them to one would throw away the positioning the bot is supposed to
  reason about. The deeper plies (enemy units acting afterwards) are what tell a
  safe approach from an exposed one.
- **Ranged:** keep all (the generator already yields one shot per target, so the
  count is naturally small).
- **Spells:** keep **one cast per spell id**, the target with the highest
  `stackValue` — i.e. hit/buff the most important stack. This caps casts at
  "number of distinct affordable spells" instead of spells × targets.
- **Moves (non-attacking):** sort by distance to the nearest enemy and keep only
  the **6 closest** (`K_MAX_MOVES_KEPT`). Wandering to far hexes is almost never
  useful, so the search only considers advancing toward the fight.
- **Wait / Defend:** always kept (one each) — cheap, occasionally optimal.

**2. Move ordering for alpha-beta** — the kept actions are then sorted by
**category** (`CAST → RANGED → MELEE → MOVE → WAIT → DEFEND`) and, within a
category, by descending priority: target value for attacks/casts, and for melee
**broken further by approach safety** — approaches that leave us adjacent to
*fewer* other enemies (`exposureAfterApproach()`) are tried first. So even before
the deep search, the bot looks at hitting the most valuable target from the
safest angle first. Alpha-beta prunes far more when the *best* moves are tried
first: high-impact actions raise α / lower β quickly, so weaker siblings are cut
without being explored, and the effective branching factor drops toward √b. This
static safety ordering also means that if the 40-cap ever trims melee, it keeps
the safer approaches. Ordering never affects correctness — only how quickly the
cutoffs happen.

## How it works

- **`GameManager::clone()`** deep-copies the whole battle (polymorphic
  `Unit::clone()` preserving `RangeUnit`, board occupancy rewired to the clones,
  hero mana/cast flags, round queues remapped, round number) and **disables
  morale** so the search is deterministic. The search runs on clones using the
  *real* `move/attack/tryCast` rules, so it can't diverge from actual game
  behavior.
- **`MinimaxBotService : IBot`** drops into the existing `botForUnit` selector —
  same interface as Random/Easy. Side-aware max/min handles the
  initiative-ordered (non-alternating) turn structure; casting (which doesn't
  end a turn) is handled naturally.

## Config

Enable per side in `settings.cfg`:

```json
"player": { "blue": "human", "red": "minimax", "depth": 4 }
```

Values: `human` / `random` / `easy` / `minimax`.

## Two honest caveats

1. **Damage is rolled randomly** inside `ActionManager::calculateDamage`, so the
   search sees noisy values rather than expected damage — material trends still
   dominate, but for sharper play a deterministic "expected damage" path for
   search would be the next improvement.
2. **Depth 4 on full 7-stack armies is heavy** (clone-per-node). The smoke run
   was fine, but if you ever see a noticeable per-move pause on large armies,
   drop `"depth"` to 3 — same code, just shallower.

The `MinimaxBotService` is built directly on the `IBot` / `ActionCommand` /
`ActionGenerator` infrastructure, so it slots in exactly where the plan intended
the Minimax successor to go.

## Key files

- `src/core/MinimaxBotService.h` / `.cc` — alpha-beta search, pruning, ordering,
  evaluation.
- `src/core/GameManager.{h,cc}` — `clone()`, `sideOfUnit()`, deterministic-morale
  flag.
- `src/models/Unit.h` / `src/models/RangeUnit.h` — polymorphic `clone()`.
- `src/core/RoundManager.{h,cc}` — `snapshotUnactivated/Waited()` + `restoreState()`.
- `src/core/PlayerType.h` — `Minimax` value + string conversion.
- `src/core/Settings.{h,cc}` — `minimaxDepth_` + `"depth"` parsing.
- `src/core/test/MinimaxBotTest.cc` — clone-independence + minimax behavior tests.
