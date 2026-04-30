# HeroesAI

## Project Description

**HeroesAI** is an academic C++ project that re-implements a turn-based, hex-grid
tactical-battle subsystem inspired by classic *Heroes of Might and Magic* combat.
The final application will simulate two opposing armies (each commanded by a hero)
fighting on an 11 x 15 hexagonal battlefield, with movement, melee and ranged
attacks, retaliations, morale rolls, status buffs, and basic unit AI.

This repository currently contains the **initial application skeleton** — the
runtime entry point, build automation, third-party library integration, and an
initial battery of unit tests — and is being submitted for the *Szkielet
aplikacji* (Application Skeleton) milestone.

## Project Context

| | |
|---|---|
| **Project Name**       | HeroesAI |
| **Team Members**       | Dominik Śledziewski, Łukasz Szydlik |
| **Milestone**          | Application Skeleton (Phase 1) |
| **Build System**       | CMake (>= 3.20) with `FetchContent` |
| **Testing Framework**  | GoogleTest (fetched at configure time) |
| **Key Libraries**      | C++ Standard Library (C++23), SFML 3.0.1, nlohmann/json 3.11.3 |
| **Language Standard**  | ISO C++23 |

## Milestone Status — Application Skeleton

This submission focuses on the **structural foundations** of the project rather
than gameplay completeness. Specifically, it demonstrates:

- A clean, layered directory structure (`models/`, `core/`, `views/`,
  `presenters/`) that mirrors the eventual MVP architecture.
- A **cross-platform build configuration** based on CMake — the same script
  builds on Linux (`g++`/`clang++`) and Windows (MSVC) with no manual edits.
- **Automatic third-party dependency acquisition** through CMake `FetchContent`:
  SFML, nlohmann/json, and GoogleTest are all downloaded and configured by the
  build itself, with no system-wide installation required.
- A **unit-testing skeleton** wired into the build via `enable_testing()` and
  `gtest_discover_tests()`, including sample tests for the model and core
  layers (army, board, hero, hex, unit, unit factory, action manager, round
  manager, game manager).
- An **asset pipeline** that copies game assets next to the built executable as
  a post-build step.

Although the repository already contains substantial gameplay logic, all of the
above structural elements are independently verifiable and constitute the
deliverable for this milestone.

## Prerequisites

### Linux

- `g++` >= 13 (or `clang++` >= 17) supporting **C++23**
- `cmake` >= 3.20
- `git` (for `FetchContent` to clone SFML, json, and GoogleTest)
- SFML system dependencies: `libfreetype-dev`, `libxrandr-dev`, `libxcursor-dev`,
  `libudev-dev`, `libopenal-dev`, `libflac-dev`, `libvorbis-dev`,
  `libgl1-mesa-dev`

On Debian/Ubuntu this can be installed with:

```bash
sudo apt update
sudo apt install -y build-essential cmake git \
    libfreetype-dev libxrandr-dev libxcursor-dev libudev-dev \
    libopenal-dev libflac-dev libvorbis-dev libgl1-mesa-dev
```

### Windows

- **Visual Studio 2022** with the *"Desktop development with C++"* workload
  (provides MSVC and the Windows SDK)
- `cmake` >= 3.20 (bundled with Visual Studio or installable separately)
- `git`

No manual SFML or json installation is required: CMake fetches and builds them.

## Directory Structure

All directory names are strictly lowercase, conforming to the project coding
standard.

```
zpr/
|-- CMakeLists.txt          # Cross-platform build script
|-- README.md
|-- .gitignore
|-- .clang-format           # Formatting rules (1TBS, 4-space indent)
|-- .clang-tidy             # Naming-convention enforcement
|-- assets/                 # Game assets (images, animations, JSON data)
|   |-- units.json
|   |-- ui/
|   |-- heroes/
|   `-- ...
|-- src/
|   |-- Main.cc            # Application entry point
|   |-- models/             # Domain layer: Unit, Hero, Army, Board, Hex, Buff
|   |   `-- test/           # Unit tests for the model layer
|   |-- core/               # Game logic: GameManager, RoundManager, ActionManager
|   |   `-- test/           # Unit tests for the core layer
|   |-- views/              # Rendering / SFML adapter and animations
|   `-- presenters/         # MVP presenters wiring views to the model
`-- build/                  # Out-of-source build directory (created on build)
```

## Build Instructions

The same CMake project builds on Linux and Windows. All commands are issued
from the repository root.

### Linux (g++ or clang++)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

The resulting executable is `build/HeroesAI` and the test binary is
`build/HeroesAITests`.

### Windows (MSVC, from a *Developer Command Prompt for VS 2022*)

```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

The resulting executable is `build\Release\HeroesAI.exe` and the test binary
is `build\Release\HeroesAITests.exe`.

## Running Tests

After a successful build, the GoogleTest suite can be executed in either of two
equivalent ways.

### Run via CTest (recommended — discovers and reports each test)

```bash
ctest --test-dir build --output-on-failure
```

### Run the test executable directly

Linux:

```bash
./build/HeroesAITests
```

Windows:

```bat
build\Release\HeroesAITests.exe
```

A successful run prints a green `[  PASSED  ]` summary for every test case in
the model and core layers, demonstrating that GoogleTest is correctly
integrated into the build.

## Coding Standards

The project adheres to the following strict coding standard, automated as far
as possible by `.clang-format` and `.clang-tidy` checked in at the repository
root:

- **Encoding**: source code is strictly **ASCII** (plain). UTF-8 is permitted
  only in documentation and translation files.
- **Language**: all identifiers and comments are in **English**.
- **Indentation**: 4 spaces per level, no tabs.
- **Brace style**: **1TBS** — opening brace on the same line as the statement.
- **Naming conventions**:
  - Types (classes, structs, enums, typedefs, template parameters):
    `PascalCase`.
  - Functions and methods: `camelCase`.
  - Class member variables: `camelCase` with a **trailing underscore**
    (e.g. `unitCount_`).
  - Local variables and function parameters: `snake_case`.
  - Enum elements and constants: `UPPER_SNAKE_CASE`.
  - Namespaces: lowercase, exactly matching the corresponding subdirectory
    name (e.g. `models::`, `core::`, `views::`, `presenters::`).
  - Global variables: `PascalCase` (use is strongly discouraged — singletons
    are preferred).
- **File and directory names**: lowercase letters and underscores only.
- **Allowed source extensions**: `.cpp`, `.hpp`, `.py`, `.as`, `.mxml`.
- **Documentation**: every source/header file begins with a Doxygen file-level
  comment block, and every class is preceded by a Doxygen class-level comment
  block describing its single explicit responsibility.

The full configuration can be reviewed in
[.clang-format](.clang-format) and [.clang-tidy](.clang-tidy).
