/*  Party Bar Detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonLZA_PartyBarDetector_H
#define PokemonAutomation_PokemonLZA_PartyBarDetector_H

#include <array>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Common/Cpp/Color.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonTools/VisualDetector.h"
#include "PokemonLZA_OverworldPartySelectionDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Counts the filled slots of the overworld party bar (bottom left, 6
//  slots). The bar is on screen when its D-pad marker shows
//  (OverworldPartySelectionDetector). A filled slot has an HP bar under
//  its icon: green, yellow or red by its HP, over a dark trough. The slot
//  whose partner is sent out (the D-pad down marker's) shows a Poke Ball
//  and no HP bar: it counts too. An empty slot shows only its card.
//  Boxes measured on 1920x1080 frames (console B, 2026-11-14): the bars
//  on rows 1028-1031, slot i from x = 86 + 76.4 * i, 52 wide.
class PartyBarDetector : public StaticScreenDetector{
public:
    PartyBarDetector(Color color = COLOR_RED, VideoOverlay* overlay = nullptr);

    virtual void make_overlays(VideoOverlaySet& items) const override;

    //  Whether the party bar is on screen; then filled_slots() is its count.
    virtual bool detect(const ImageViewRGB32& screen) override;

    virtual void reset_state() override{
        m_selection.reset_state();
        m_filled_slots = 0;
    }

    //  The filled slots of the bar on the last detect()'s screen: 0 when
    //  no bar was on it.
    int filled_slots() const{ return m_filled_slots; }

private:
    //  Whether slot `index`'s HP bar shows: the bar's box is mostly HP
    //  color or darker than the card above it (the trough).
    bool slot_has_hp_bar(const ImageViewRGB32& screen, size_t index) const;

    OverworldPartySelectionDetector m_selection;
    //  Each slot's HP bar and, above it, the card between icon and bar.
    std::array<ImageFloatBox, 6> m_bar_boxes;
    std::array<ImageFloatBox, 6> m_card_boxes;
    int m_filled_slots = 0;
};

//  The filled slots of the party bar on `screen`: 0 when no bar is on it.
int party_bar_filled_slots(const ImageViewRGB32& screen);


//  Registers the party bar detector's tests on the PartyBarDetector frames.
void add_tests_PartyBarDetector(UnitTestDatabase& database);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
