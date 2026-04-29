cmake_minimum_required(VERSION 3.20)
project(HeroesAI)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SFML_BUILD_NETWORK OFF CACHE BOOL "" FORCE)
set(SFML_BUILD_AUDIO ON CACHE BOOL "" FORCE)
set(SFML_BUILD_GRAPHICS ON CACHE BOOL "" FORCE)
set(SFML_BUILD_WINDOW ON CACHE BOOL "" FORCE)
set(SFML_INSTALL_PKGCONFIG_FILES OFF CACHE BOOL "" FORCE)

include(FetchContent)
FetchContent_Declare(
    SFML
    GIT_REPOSITORY https://github.com/SFML/SFML.git
  GIT_TAG 3.0.1
)
FetchContent_MakeAvailable(SFML)

include(FetchContent)
FetchContent_Declare(
  json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(json)

add_executable(${PROJECT_NAME}
  src/main.cpp
  src/models/unit_factory.cpp
  src/core/ActionManager.cpp
  src/core/RoundManager.cpp
  src/core/GameManager.cpp
  src/presenters/BattlePresenter.cpp
  src/views/DefParser.cpp
  src/views/DefManager.cpp
  src/views/AnimationController.cpp
  src/views/SfmlBattleView.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
  src/models
  src/core
  src/views
  src/presenters
)

target_link_libraries(${PROJECT_NAME} 
    sfml-graphics 
    sfml-window 
    sfml-system 
    sfml-audio
    nlohmann_json::nlohmann_json
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets")

    
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(HeroesAITests src/models/test/models_test.cpp src/models/test/unit_factory_test.cpp src/models/test/board_test.cpp src/models/test/army_test.cpp src/models/test/hero_test.cpp src/models/unit_factory.cpp src/core/RoundManager.cpp src/core/test/round_manager_test.cpp src/core/ActionManager.cpp src/core/test/action_manager_test.cpp src/core/GameManager.cpp src/core/test/game_manager_test.cpp)
target_include_directories(HeroesAITests PRIVATE src/models)
target_link_libraries(HeroesAITests GTest::gtest_main nlohmann_json::nlohmann_json)

include(GoogleTest)
gtest_discover_tests(HeroesAITests)

