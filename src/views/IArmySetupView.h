/**
 * @file IArmySetupView.h
 * @brief Abstract view interface used by the army setup presenter.
 * @author Dominik Sledziewski
 */
#pragma once

#include <string>

namespace views {

/**
 * @brief Renderer-agnostic interface implemented by every army-setup view.
 *
 * The presenter owns this interface and never depends on SFML directly,
 * keeping it possible to substitute a headless test view for unit
 * testing of presenter logic.
 */
class IArmySetupView {
public:
    virtual ~IArmySetupView( ) = default;

    virtual void showMessage( const std::string& msg ) = 0;
};

} // namespace views
