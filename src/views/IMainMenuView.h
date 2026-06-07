/**
 * @file IMainMenuView.h
 * @brief Abstract view interface used by the main menu presenter.
 * @author Dominik Sledziewski
 */
#pragma once

#include <string>

namespace views {

/**
 * @brief Renderer-agnostic interface implemented by every main-menu view.
 *
 * The presenter owns this interface and never depends on SFML directly,
 * keeping it possible to substitute a headless test view for unit
 * testing of presenter logic.
 */
class IMainMenuView {
public:
    virtual ~IMainMenuView( ) = default;

    virtual void showMessage( const std::string& msg ) = 0;
};

} // namespace views
