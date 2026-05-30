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
  src/Main.cc
  src/models/UnitFactory.cc
  src/models/SpellRegistry.cc
  src/core/ActionManager.cc
  src/core/SpellResolver.cc
  src/core/ActionGenerator.cc
  src/core/RandomBotService.cc
  src/core/EasyBotService.cc
  src/core/RoundManager.cc
  src/core/GameManager.cc
  src/core/Settings.cc
  src/core/SceneManager.cc
  src/core/MainMenuScene.cc
  src/core/ArmySetupScene.cc
  src/core/BattleScene.cc
  src/presenters/BattlePresenter.cc
  src/presenters/MainMenuPresenter.cc
  src/presenters/ArmySetupPresenter.cc
  src/views/DefParser.cc
  src/views/DefManager.cc
  src/views/AnimationController.cc
  src/views/SfmlBattleView.cc
  src/views/SfmlMainMenuView.cc
  src/views/SfmlArmySetupView.cc
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

# settings.cfg is copied by a dedicated ALL target rather than a
# POST_BUILD step. POST_BUILD only runs when HeroesAI is relinked, so
# editing only settings.cfg (no source change) would never trigger the
# copy. An ALL custom target runs on every `cmake --build`, and
# copy_if_different keeps it cheap (only copies when the file changed).
add_custom_target(copy_settings ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/settings.cfg" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/settings.cfg"
    COMMENT "Copying settings.cfg into the runtime directory")
add_dependencies(copy_settings ${PROJECT_NAME})


include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(HeroesAITests
  src/models/test/ModelsTest.cc
  src/models/test/UnitFactoryTest.cc
  src/models/test/BoardTest.cc
  src/models/test/ArmyTest.cc
  src/models/test/HeroTest.cc
  src/models/UnitFactory.cc
  src/models/SpellRegistry.cc
  src/core/RoundManager.cc
  src/core/test/RoundManagerTest.cc
  src/core/ActionManager.cc
  src/core/test/ActionManagerTest.cc
  src/core/SpellResolver.cc
  src/core/ActionGenerator.cc
  src/core/RandomBotService.cc
  src/core/test/ActionGeneratorTest.cc
  src/core/GameManager.cc
  src/core/test/GameManagerTest.cc
)
target_include_directories(HeroesAITests PRIVATE src/models)
target_link_libraries(HeroesAITests GTest::gtest_main nlohmann_json::nlohmann_json)

include(GoogleTest)
gtest_discover_tests(HeroesAITests)
