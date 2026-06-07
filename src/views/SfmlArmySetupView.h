/**
 * @file SfmlArmySetupView.h
 * @brief Concrete SFML-backed army setup view.
 * @author Dominik Sledziewski
 */
#pragma once

#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "DefManager.h"
#include "IArmySetupView.h"

namespace presenters {
class ArmySetupPresenter;
}

namespace views {

/**
 * @brief SFML-based renderer for the army setup screen.
 *
 * Draws a background, 14 unit slots (7 per army) and a "Back" button.
 * Each slot opens a unit-picker overlay listing every known unit (in
 * UnitID order) plus an "Empty" cell to clear the slot. Count is
 * adjusted with per-slot +/- buttons.
 *
 * The view holds only presentation state -- the army data lives in the
 * presenter (which mutates the shared Settings reference).
 */
class SfmlArmySetupView : public IArmySetupView {
public:
    explicit SfmlArmySetupView( sf::RenderWindow& window );

    bool isOpen( ) const;
    void processEvents( presenters::ArmySetupPresenter& presenter );
    void render( presenters::ArmySetupPresenter& presenter );

    void showMessage( const std::string& msg ) override;

private:
    struct SlotUi {
        sf::FloatRect bounds_;
        sf::FloatRect countBounds_;
        sf::FloatRect minusBounds_;
        sf::FloatRect plusBounds_;
        bool hovered_ = false;
        bool countHovered_ = false;
        bool minusHovered_ = false;
        bool plusHovered_ = false;
    };

    struct UnitEntry {
        std::string displayName_;
        std::string assetFilename_;
        bool hasPortrait_ = false;
        sf::IntRect portraitRect_;
    };

    // Per-side header controls: a player-type button and four hero-stat
    // boxes (attack, defense, power, knowledge).
    struct SideControls {
        sf::FloatRect playerButton_;
        bool playerHovered_ = false;
        std::array<sf::FloatRect, 4> statBounds_;
        std::array<bool, 4> statHovered_{ };
    };

    enum class EditKind {
        None,
        Count,
        HeroStat
    };

    void loadAssets( );
    void layout( );
    bool routeClick( float x, float y, presenters::ArmySetupPresenter& presenter );
    void updateHover( float x, float y, bool picker_open );
    void startEditingCount( int side, int slot_index, int current_value );
    void startEditingHeroStat( int side, int stat_index, int current_value );
    void commitEditing( presenters::ArmySetupPresenter& presenter );
    void cancelEditing( );
    bool isEditingCount( int side, int slot_index ) const;
    bool isEditingHeroStat( int side, int stat_index ) const;

    void drawBackground( );
    void drawSideControls( int side, presenters::ArmySetupPresenter& presenter );
    void drawSlot( const SlotUi& slot_ui,
                       int side,
                       int slot_index,
                       presenters::ArmySetupPresenter& presenter );
    void drawBackButton( );
    void drawPickerOverlay( presenters::ArmySetupPresenter& presenter );
    void drawUnitIcon( const UnitEntry& entry,
                            const sf::FloatRect& target,
                            sf::Color tint = sf::Color::White );
    void drawMessage( );

    sf::RenderWindow& window_;
    float screenWidth_;
    float screenHeight_;

    sf::Texture backgroundTexture_;
    std::unique_ptr<sf::Sprite> backgroundSprite_;
    bool backgroundLoaded_ = false;

    sf::Texture backButtonTexture_;
    std::unique_ptr<sf::Sprite> backButtonSprite_;
    sf::FloatRect backButtonBounds_;
    bool backButtonHovered_ = false;
    bool backButtonLoaded_ = false;

    std::array<SlotUi, 7> leftSlots_;
    std::array<SlotUi, 7> rightSlots_;
    std::array<SideControls, 2> sideControls_;

    sf::FloatRect pickerPanelBounds_;
    std::vector<sf::FloatRect> pickerCellBounds_;
    sf::FloatRect pickerEmptyCellBounds_;
    int pickerHoveredCell_ = -2;

    sf::Font font_;
    bool fontLoaded_ = false;
    std::unique_ptr<sf::Text> headerText_;
    std::unique_ptr<sf::Text> slotNameText_;
    std::unique_ptr<sf::Text> slotCountText_;
    std::unique_ptr<sf::Text> slotButtonText_;
    std::unique_ptr<sf::Text> pickerLabelText_;
    std::unique_ptr<sf::Text> messageText_;

    DefManager defManager_;
    sf::Texture portraitAtlasTexture_;
    bool portraitAtlasLoaded_ = false;
    std::vector<UnitEntry> unitCatalog_;

    EditKind editKind_ = EditKind::None;
    int editSide_ = -1;
    int editIndex_ = -1;
    std::string editingBuffer_;
    sf::Clock editingClock_;

    std::string latestMessage_;
};

} // namespace views
