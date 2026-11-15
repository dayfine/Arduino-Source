/*  Party Bar Detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <string>
#include "CommonFramework/ImageTools/ImageStats.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"
#include "Tests/ImageUnitTest.h"
#include "PokemonLZA_PartyBarDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

namespace{

//  The slot's HP bar: 52 of 1920 wide from x = 86 + 76.4 * i, rows
//  1028-1031 of 1080; the box stays inside the bar.
constexpr double SLOT_X = 0.045;
constexpr double SLOT_PITCH = 0.0398;
constexpr double BAR_WIDTH = 0.026;
constexpr double BAR_Y = 0.9519;
constexpr double BAR_HEIGHT = 0.0037;
//  The card between the icon and the bar: rows 1021-1025.
constexpr double CARD_Y = 0.9454;
constexpr double CARD_HEIGHT = 0.0046;

//  The trough is the card darkened: a black overlay at about half.
constexpr double TROUGH_BRIGHTNESS_RATIO = 0.65;
//  A bar's box reads 0.79 and up (a nearly empty red bar over a bright
//  teal scene); a slot with none 0.16 at most (the Poke Ball of the slot
//  sent out).
constexpr double BAR_SHARE_MIN = 0.5;

//  An HP bar's green, yellow or red.
bool is_hp_color(Color color){
    const uint32_t r = color.red();
    const uint32_t g = color.green();
    const uint32_t b = color.blue();
    const bool green = g > 140 && g > r + 60 && g > b + 60;
    const bool yellow = r > 170 && g > 150 && b < 100;
    const bool red = r > 170 && g < 110 && b < 110;
    return green || yellow || red;
}

double brightness(Color color){
    return (color.red() + color.green() + color.blue()) / 3.0;
}

}


PartyBarDetector::PartyBarDetector(Color color, VideoOverlay* overlay)
    : m_selection(color, overlay)
{
    for (size_t i = 0; i < 6; i++){
        const double x = SLOT_X + SLOT_PITCH * i;
        m_bar_boxes[i] = {x, BAR_Y, BAR_WIDTH, BAR_HEIGHT};
        m_card_boxes[i] = {x, CARD_Y, BAR_WIDTH, CARD_HEIGHT};
    }
}

void PartyBarDetector::make_overlays(VideoOverlaySet& items) const{
    m_selection.make_overlays(items);
    for (size_t i = 0; i < 6; i++){
        items.add(COLOR_GREEN, m_bar_boxes[i]);
    }
}

bool PartyBarDetector::slot_has_hp_bar(const ImageViewRGB32& screen, size_t index) const{
    const ImageViewRGB32 card = extract_box_reference(screen, m_card_boxes[index]);
    const ImageViewRGB32 bar = extract_box_reference(screen, m_bar_boxes[index]);
    const size_t total = bar.width() * bar.height();
    if (total == 0 || card.width() * card.height() == 0){
        return false;
    }
    const double trough_max = image_average(card).sum() / 3.0 * TROUGH_BRIGHTNESS_RATIO;
    size_t count = 0;
    for (size_t y = 0; y < bar.height(); y++){
        for (size_t x = 0; x < bar.width(); x++){
            const Color color(bar.pixel(x, y));
            if (is_hp_color(color) || brightness(color) < trough_max){
                count++;
            }
        }
    }
    return (double)count / (double)total >= BAR_SHARE_MIN;
}

bool PartyBarDetector::detect(const ImageViewRGB32& screen){
    m_filled_slots = 0;
    if (screen.width() == 0 || screen.height() == 0){
        return false;
    }
    if (!m_selection.detect(screen)){
        return false;
    }
    const uint8_t sent_out = m_selection.dpad_down_idx();
    for (size_t i = 0; i < 6; i++){
        if (i == sent_out || slot_has_hp_bar(screen, i)){
            m_filled_slots++;
        }
    }
    return true;
}

int party_bar_filled_slots(const ImageViewRGB32& screen){
    PartyBarDetector detector;
    detector.detect(screen);
    return detector.filled_slots();
}


namespace{

struct PartyBarCase{
    std::string name;
    struct Input{
        std::string image;  //  In CommandLineTests/PokemonLZA/PartyBarDetector/.
    };
    Input input;
    struct Expectation{
        int filled_slots;
    };
    Expectation expectation;
};

const PartyBarCase PARTY_BAR_CASES[] = {
    {
        .name = "five filled, slot 0 selected (D-pad up), the sixth empty",
        .input = {.image = "party5_slot0_selected.png"},
        .expectation = {.filled_slots = 5},
    },
    {
        .name = "five filled, slot 0's partner sent out (Poke Ball, no bar), Mareep's bar red",
        .input = {.image = "party5_slot0_sent_out.png"},
        .expectation = {.filled_slots = 5},
    },
    {
        .name = "five filled, slot 0's partner sent out, over bright ground",
        .input = {.image = "party5_b.png"},
        .expectation = {.filled_slots = 5},
    },
};

class Test_PartyBarDetector : public ImageUnitTest{
public:
    explicit Test_PartyBarDetector(const PartyBarCase& test_case)
        : ImageUnitTest(
            std::format("PokemonLZA::PartyBarDetector - {}", test_case.name),
            {"PokemonLZA/PartyBarDetector/" + test_case.input.image}
        )
        , m_case(test_case)
    {}

    virtual UnitTestResult run_on_images(
        Logger& logger, CancellableScope& scope,
        const std::vector<ImageRGB32>& images
    ) const override{
        const int filled = party_bar_filled_slots(images[0]);
        if (filled != m_case.expectation.filled_slots){
            return std::format("Expected {} filled slots, got {}.", m_case.expectation.filled_slots, filled);
        }
        return true;
    }

private:
    const PartyBarCase& m_case;
};

}


void add_tests_PartyBarDetector(UnitTestDatabase& database){
    for (const PartyBarCase& item : PARTY_BAR_CASES){
        database.add<Test_PartyBarDetector>(item);
    }
}



}
}
}
