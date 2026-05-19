/**
 * @file IScene.h
 * @brief Abstract top-level application scene driven by the SceneManager.
 * @author Łukasz Szydlik
 */
#pragma once

namespace views {

/**
 * @brief Identifies a top-level application scene.
 *
 * NONE acts as a sentinel value meaning "no further scene -- terminate
 * the application loop".
 */
enum class SceneId {
    NONE,
    MAIN_MENU,
    BATTLE
};

/**
 * @brief Lifecycle contract for a single application scene.
 *
 * The SceneManager owns the SFML window and drives every active scene
 * through the same three-step cycle: poll input, render a frame, ask
 * whether the scene has finished. When a scene reports completion the
 * manager destroys it and constructs the scene named by nextSceneId().
 */
class IScene {
public:
    virtual ~IScene( ) = default;

    virtual void processEvents( ) = 0;
    virtual void render( ) = 0;
    virtual bool isFinished( ) const = 0;
    virtual SceneId nextSceneId( ) const = 0;
};

} // namespace views
