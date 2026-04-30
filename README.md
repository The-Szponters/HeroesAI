# HeroesAI

**Authors:** Dominik Śledziewski, Łukasz Szydlik

## Required Libraries
This project uses CMake and `FetchContent` to automatically download and configure the following libraries during the build process. No manual installation is required:
* **SFML 3.0.1** (Graphics, window, system, audio)
* **nlohmann_json 3.11.3** (JSON data parsing)
* **GoogleTest** (Unit testing framework)

## Build and Run Instructions (Ubuntu 24.04 LTS)

### 1. Install Build Tools and System Dependencies
While CMake fetches the C++ libraries, SFML still requires basic system graphics and audio headers to compile successfully. Run the following command in your terminal:

```bash
sudo apt update
sudo apt install -y build-essential cmake git libfreetype-dev libxrandr-dev libxcursor-dev libudev-dev libopenal-dev libflac-dev libvorbis-dev libgl1-mesa-dev
```

### 2. Build the Project
Configure and compile the project. All dependencies will be fetched and built locally in the project directory:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### 3. Run the Application
The program must be executed from the root project directory so it can correctly load resources from the `assets/` folder:

```bash
./build/HeroesAI
```

## Unit Tests
The project includes automated tests for the models and core game logic. To build and execute the test suite, run:

```bash
./build/HeroesAITests
```