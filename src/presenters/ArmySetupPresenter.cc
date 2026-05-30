/**
 * @file ArmySetupPresenter.cc
 * @brief Implementation of the army setup presenter.
 * @author Łukasz Szydlik
 */
#include "ArmySetupPresenter.h"

#include <algorithm>

#include "../models/UnitId.h"

namespace presenters {

namespace {

constexpr int K_MAX_COUNT = 99999;
constexpr int K_UNIT_COUNT = 42;

} // namespace

ArmySetupPresenter::ArmySetupPresenter( views::IArmySetupView& view, core::Settings& settings )
    : view_( view ), settings_( settings ) {}

void ArmySetupPresenter::start( ) {
    view_.showMessage( "" );
    pickerOpen_ = false;
    backRequested_ = false;
}

void ArmySetupPresenter::onSlotClicked( int side, int slot_index ) {
    if ( slot_index < 0 || slot_index >= static_cast<int>( core::K_ARMY_SLOT_COUNT ) ) {
        return;
    }
    pickerOpen_ = true;
    pickerSide_ = side;
    pickerSlot_ = slot_index;
}

void ArmySetupPresenter::onPickerCellClicked( int unit_index ) {
    if ( ! pickerOpen_ ) {
        return;
    }
    core::ArmyConfig& army = armyFor( pickerSide_ );
    core::ArmySlot& slot = army[pickerSlot_];

    if ( unit_index < 0 ) {
        slot.unitId_.reset( );
        slot.count_ = 0;
    } else if ( unit_index < K_UNIT_COUNT ) {
        slot.unitId_ = static_cast<models::UnitID>( unit_index );
        if ( slot.count_ <= 0 ) {
            slot.count_ = 1;
        }
    }
    pickerOpen_ = false;
}

void ArmySetupPresenter::onPickerCancelled( ) {
    pickerOpen_ = false;
}

void ArmySetupPresenter::onCountChanged( int side, int slot_index, int delta ) {
    if ( slot_index < 0 || slot_index >= static_cast<int>( core::K_ARMY_SLOT_COUNT ) ) {
        return;
    }
    core::ArmySlot& slot = armyFor( side )[slot_index];
    if ( ! slot.unitId_.has_value( ) ) {
        return;
    }
    slot.count_ = std::clamp( slot.count_ + delta, 0, K_MAX_COUNT );
    if ( slot.count_ == 0 ) {
        slot.unitId_.reset( );
    }
}

void ArmySetupPresenter::setCount( int side, int slot_index, int value ) {
    if ( slot_index < 0 || slot_index >= static_cast<int>( core::K_ARMY_SLOT_COUNT ) ) {
        return;
    }
    core::ArmySlot& slot = armyFor( side )[slot_index];
    if ( ! slot.unitId_.has_value( ) ) {
        return;
    }
    slot.count_ = std::clamp( value, 0, K_MAX_COUNT );
    if ( slot.count_ == 0 ) {
        slot.unitId_.reset( );
    }
}

void ArmySetupPresenter::onBackClicked( ) {
    // Persist only the rosters; window / ai sections in the file stay
    // exactly as the user left them.
    settings_.saveArmiesToFile( "settings.cfg" );
    backRequested_ = true;
}

bool ArmySetupPresenter::isBackRequested( ) const {
    return backRequested_;
}

const core::ArmyConfig& ArmySetupPresenter::leftArmy( ) const {
    return settings_.leftArmy_;
}

const core::ArmyConfig& ArmySetupPresenter::rightArmy( ) const {
    return settings_.rightArmy_;
}

bool ArmySetupPresenter::pickerOpen( ) const {
    return pickerOpen_;
}

int ArmySetupPresenter::pickerSide( ) const {
    return pickerSide_;
}

int ArmySetupPresenter::pickerSlot( ) const {
    return pickerSlot_;
}

core::ArmyConfig& ArmySetupPresenter::armyFor( int side ) {
    return side == 0 ? settings_.leftArmy_ : settings_.rightArmy_;
}

} // namespace presenters
