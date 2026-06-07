# HeroesAI

A turn-based tactical combat game inspired by the battle screen of the classic
*Heroes of Might and Magic III*. Two heroes face off on a hexagonal battlefield,
each commanding up to seven stacks of creatures and a spell book. Sides can be
controlled by a human player or by one of three AI opponents, including a
depth-limited **minimax** engine with alpha-beta pruning.

**Authors:** Łukasz Szydlik, Dominik Śledziewski

## Features

* Hexagonal battlefield (15 × 11) with cube-coordinate pathfinding.
* Initiative-ordered rounds (faster stacks act first; `Wait` and `Defend` supported).
* Melee and ranged combat with retaliation, morale rolls and a 13-spell spell book.
* Data-driven creature roster (Castle / Inferno / Necropolis factions) loaded from JSON.
* Three AI strategies selectable per side: `random`, `easy` (heuristic) and
  `minimax` (alpha-beta search with move ordering and branch pruning).
* Original-format sprite animation (legacy `.def` files) rendered through SFML.
* Configurable match setup via `settings.cfg` and an in-game army-setup screen.

## Required Libraries

The project uses CMake with `FetchContent` to download and build the following
libraries automatically during configuration — no manual installation is needed:

| Library          | Version  | Purpose                              |
| ---------------- | -------- | ------------------------------------ |
| **SFML**         | 3.0.1    | Graphics, window, system, audio      |
| **nlohmann/json**| 3.11.3   | JSON parsing (creature data, config) |
| **GoogleTest**   | 1.x      | Unit-test framework                  |

> These are third-party libraries. SFML and nlohmann/json are used at runtime;
> GoogleTest is used only by the test target. None of them implements the game
> itself — all game logic is original C++ code.

## Prerequisites

* **CMake** ≥ 3.20
* A **C++23**-capable compiler (tested with `g++` 13).
* SFML's system-level development headers (graphics/audio backends). On
  Debian/Ubuntu:

```bash
sudo apt update
sudo apt install -y build-essential cmake git \
    libfreetype-dev libxrandr-dev libxcursor-dev libxi-dev \
    libudev-dev libopenal-dev libflac-dev libvorbis-dev libgl1-mesa-dev
```

> Tested on **Ubuntu 24.04 LTS** with `g++` 13. On Ubuntu 22.04 LTS install a
> C++23 compiler first (e.g. `sudo apt install g++-13` and pass
> `-DCMAKE_CXX_COMPILER=g++-13`).

## Build

All dependencies are fetched and built locally inside the build directory:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

## Run

Run from the project root so the executable can find the `assets/` folder and
`settings.cfg` (both are also copied next to the binary at build time):

```bash
./build/HeroesAI
```

## Tests

The project ships automated unit tests for the model and game-logic layers
(creatures, board, army, hero, turn ordering, action rules, the action
generator and the minimax bot). Build then run them with either:

```bash
# directly
./build/HeroesAITests

# or through CTest
ctest --test-dir build --output-on-failure
```

## Configuration

`settings.cfg` (JSON) controls the match without rebuilding. Key fields:

* `player.blue` / `player.red` — controller for each side:
  `"human"`, `"random"`, `"easy"` or `"minimax"`.
* `player.depth` — search depth for the minimax bot (≥ 1; higher is stronger
  but slower).
* `window` — resolution, frame-rate limit and title.
* `heroes.blue` / `heroes.red` — primary stats (attack, defense, power, knowledge).
* `left_army` / `right_army` — starting stacks (`unit` name from
  `assets/units.json` and `count`); a `null` unit leaves the slot empty.

## Controls

* **Left click** a highlighted hex to move, or an enemy stack to attack from the
  hovered approach hex.
* **W** — wait, **D** — defend, **spell-book button** — open the spell book.
* **Esc** / on-screen buttons — surrender or quit.

## Project Layout

```
src/
  models/       Game entities: Unit, RangeUnit, Hero, Army, Board, Hex, Buff, Spell
  core/         Game logic: GameManager, RoundManager, ActionManager, AI bots, scenes
  presenters/   MVP presenters mediating between models and views
  views/        SFML rendering, .def sprite parsing, animation, input
  Main.cc       Entry point
assets/         Creature data (units.json), sprites, backgrounds, UI, cursors
settings.cfg    Match configuration
```

The codebase follows a **Model–View–Presenter** structure: `core`/`models` hold
all game state and rules (no rendering dependencies), `views` handle SFML
rendering and input, and `presenters` translate between them.
