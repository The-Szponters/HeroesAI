/**
 * @file ArmySetupPresenter.h
 * @brief Mediator handling army setup interactions.
 * @author Dominik Sledziewski
 */
#pragma once

#include "../core/Settings.h"
#include "../views/IArmySetupView.h"

namespace presenters {

/**
 * @brief Translates army-setup UI events into mutations of the shared
 *        Settings rosters.
 *
 * The presenter mutates the Settings reference handed to it. It also
 * tracks the unit-picker overlay state (which slot is being edited) so
 * the view can render the overlay accordingly. On "Back" the presenter
 * persists the current Settings to disk and signals the scene wrapper
 * to switch back to the main menu.
 */
class ArmySetupPresenter {
public:
    ArmySetupPresenter( views::IArmySetupView& view, core::Settings& settings );

    void start( );

    void onSlotClicked( int side, int slot_index );
    void onPickerCellClicked( int unit_index );
    void onPickerCancelled( );
    void onCountChanged( int side, int slot_index, int delta );
    void setCount( int side, int slot_index, int value );
    void onBackClicked( );

    // Cycles a side's controller: human -> random -> easy -> minimax -> ...
    void cyclePlayerType( int side );
    // Sets one hero primary stat (stat_index 0=attack,1=defense,2=power,
    // 3=knowledge), clamped to a sane range.
    void setHeroStat( int side, int stat_index, int value );

    bool isBackRequested( ) const;

    const core::ArmyConfig& leftArmy( ) const;
    const core::ArmyConfig& rightArmy( ) const;

    core::PlayerType playerType( int side ) const;
    const core::HeroConfig& heroConfig( int side ) const;

    bool pickerOpen( ) const;
    int pickerSide( ) const;
    int pickerSlot( ) const;

private:
    core::ArmyConfig& armyFor( int side );

    views::IArmySetupView& view_;
    core::Settings& settings_;

    bool pickerOpen_ = false;
    int pickerSide_ = 0;
    int pickerSlot_ = 0;
    bool backRequested_ = false;
};

} // namespace presenters
