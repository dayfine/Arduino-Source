/*  PC Screen Reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <string>
#include <vector>
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTools/ImageStats.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonTools/Images/SolidColorTest.h"
#include "Tests/ImageUnitTest.h"
#include "Tests/TestUtils.h"
#include "VideoGames/PokemonFRLG/Inference/Box/PokemonFRLG_BoxCellOccupiedDetector.h"
#include "VideoGames/PokemonFRLG/Inference/Box/PokemonFRLG_BoxSelectionModeDetector.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_StartMenuDetector.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_DigitReader.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Settings.h"
#include "PokemonFRLG_PcScreenReader.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

//  pokefirered: TOTAL_BOXES_COUNT, IN_BOX_COLUMNS.
constexpr size_t BOX_COUNT = 14;
constexpr size_t BOX_COLUMNS = 6;
//  The party panel: the lead, five slots, CANCEL.
constexpr size_t PARTY_PANEL_ROWS = 7;

ImageFloatBox gba(double x, double y, double width, double height){
    return ImageFloatBox(x / 240, y / 160, width / 240, height / 160);
}

//  Patches measured on the captures (see the header), each plain on its
//  screen: inside the PC list's window right of LOG OFF; inside the
//  storage menu's window past the list's; the PKMN DATA panel's frame
//  (151, 155, 182); a Pokemon's menu right of MARK; the YES/NO box above
//  YES; the box chooser's yellow (255, 223, 94) right of the box's name;
//  the party panel's teal (50, 157, 160) below the lead; the message box
//  under its second line.
const ImageFloatBox PC_LIST_PATCH = gba(100, 58, 20, 14);
const ImageFloatBox STORAGE_MENU_PATCH = gba(128, 58, 18, 30);
const ImageFloatBox DATA_PANEL_PATCH = gba(3, 100, 4, 40);
const ImageFloatBox MON_MENU_PATCH = gba(205, 72, 25, 14);
const ImageFloatBox YES_NO_PATCH = gba(200, 86, 32, 4);
const ImageFloatBox CHOOSER_PATCH = gba(160, 68, 30, 18);
const ImageFloatBox PARTY_PANEL_PATCH = gba(122, 96, 12, 36);
const ImageFloatBox MESSAGE_PATCH = gba(10, 148, 160, 4);
//  The box chooser's box number, after "BOX".
const ImageFloatBox CHOOSER_BOX_NUMBER = gba(144, 74, 16, 12);

//  The share of an arrow's box an arrow darkens (0.31 to 0.37 measured;
//  text and frames nearby under 0.25).
constexpr double ARROW_DARK = 0.25;
//  The share of a party slot's hand area the hand's white covers (0.10 to
//  0.16 measured, the top right slot's cut by the screen's edge; a
//  Pokemon's icon at most 0.03).
constexpr double HAND_WHITE = 0.06;
//  The game screen's average under this (of 765): a fade.
constexpr double DARK_SCREEN = 150;

double dark_share(const ImageViewRGB32& game, const ImageFloatBox& box){
    const ImageViewRGB32 image = extract_box_reference(game, box);
    size_t dark = 0;
    for (size_t y = 0; y < image.height(); y++){
        for (size_t x = 0; x < image.width(); x++){
            const Color pixel(image.pixel(x, y));
            dark += pixel.red() + pixel.green() + pixel.blue() < 330 ? 1 : 0;
        }
    }
    const size_t area = image.width() * image.height();
    return area == 0 ? 0 : double(dark) / double(area);
}

double white_share(const ImageViewRGB32& game, const ImageFloatBox& box){
    const ImageViewRGB32 image = extract_box_reference(game, box);
    size_t white = 0;
    for (size_t y = 0; y < image.height(); y++){
        for (size_t x = 0; x < image.width(); x++){
            const Color pixel(image.pixel(x, y));
            const uint8_t low = std::min({pixel.red(), pixel.green(), pixel.blue()});
            const uint8_t high = std::max({pixel.red(), pixel.green(), pixel.blue()});
            white += low >= 235 && high - low < 20 ? 1 : 0;
        }
    }
    const size_t area = image.width() * image.height();
    return area == 0 ? 0 : double(white) / double(area);
}

//  The one row of `rows` whose arrow box (x, y + `step` * row) holds an
//  arrow; nothing if none or several do.
std::optional<size_t> arrow_row(const ImageViewRGB32& game, double x, double y, double step, size_t rows){
    std::optional<size_t> found;
    for (size_t row = 0; row < rows; row++){
        if (dark_share(game, gba(x, y + step * double(row), 7, 10)) < ARROW_DARK){
            continue;
        }
        if (found){
            return std::nullopt;
        }
        found = row;
    }
    return found;
}

//  The party panel's slot under the hand: the lead's hand area above its
//  box; a right slot's above and on its box, 24 apart (the top one cut by
//  the screen's top); CANCEL's above CANCEL.
std::optional<size_t> party_panel_cursor(const ImageViewRGB32& game){
    std::vector<ImageFloatBox> areas{gba(92, 36, 28, 26)};
    for (size_t slot = 1; slot < PARTY_PANEL_ROWS - 1; slot++){
        const double bottom = 24.0 * double(slot - 1) + 11;
        const double top = std::max(0.0, bottom - 25);
        areas.push_back(gba(138, top, 28, bottom - top));
    }
    areas.push_back(gba(138, 118, 28, 24));
    std::optional<size_t> best;
    double best_share = HAND_WHITE;
    for (size_t slot = 0; slot < areas.size(); slot++){
        const double share = white_share(game, areas[slot]);
        if (share >= best_share){
            best_share = share;
            best = slot;
        }
    }
    return best;
}

PcView box_screen(Logger& logger, const ImageViewRGB32& screen, const ImageViewRGB32& game){
    if (is_white(extract_box_reference(game, MON_MENU_PATCH))){
        return {.screen = PcScreen::MON_MENU, .cursor = arrow_row(game, 159, 44, 16, 5)};
    }
    if (is_white(extract_box_reference(game, YES_NO_PATCH))){
        return {.screen = PcScreen::YES_NO, .cursor = arrow_row(game, 191, 91, 14, 2)};
    }
    if (is_solid(extract_box_reference(game, CHOOSER_PATCH), {0.445, 0.390, 0.165}, 0.08, 30)){
        //  The chooser's yellow is the stat boxes' (DigitTemplateType::StatBox).
        const int number = read_digits_waterfill_template(
            logger, extract_box_reference(game, CHOOSER_BOX_NUMBER), DigitTemplateType::StatBox
        );
        PcView view{.screen = PcScreen::BOX_CHOOSER};
        if (number >= 1 && size_t(number) <= BOX_COUNT){
            view.box = size_t(number - 1);
        }
        return view;
    }
    if (is_solid(extract_box_reference(game, PARTY_PANEL_PATCH), {0.137, 0.426, 0.437}, 0.10, 80)){
        return {.screen = PcScreen::PARTY_PANEL, .cursor = party_panel_cursor(game)};
    }
    PcView view{.screen = PcScreen::BOX_GRID};
    const std::optional<BoxSelection> selection = BoxSelectionModeDetector(COLOR_RED).detect(screen);
    if (selection && selection->location == BoxSelectionLocation::Grid){
        view.cursor = selection->row * BOX_COLUMNS + selection->column;
        view.occupied = BoxCellOccupiedDetector(COLOR_RED).detect(screen);
    }
    return view;
}

}


const char* pc_screen_name(PcScreen screen){
    switch (screen){
    case PcScreen::OVERWORLD:       return "overworld";
    case PcScreen::TEXT:            return "text";
    case PcScreen::PC_LIST:         return "PC list";
    case PcScreen::STORAGE_MENU:    return "storage menu";
    case PcScreen::PARTY_PANEL:     return "party panel";
    case PcScreen::BOX_GRID:        return "box grid";
    case PcScreen::MON_MENU:        return "Pokemon's menu";
    case PcScreen::BOX_CHOOSER:     return "box chooser";
    case PcScreen::YES_NO:          return "YES/NO";
    case PcScreen::OTHER:           return "other";
    }
    return "?";
}

PcView read_pc_screen(Logger& logger, const ImageViewRGB32& screen){
    const ImageViewRGB32 game = extract_box_reference(screen, GameSettings::instance().GAME_BOX);
    if (is_solid(extract_box_reference(game, DATA_PANEL_PATCH), {0.311, 0.319, 0.370}, 0.06, 80)){
        return box_screen(logger, screen, game);
    }
    if (is_white(extract_box_reference(game, PC_LIST_PATCH))){
        const bool storage = is_white(extract_box_reference(game, STORAGE_MENU_PATCH));
        const std::optional<size_t> cursor = arrow_row(game, 9, 12, 16, storage ? 5 : 4);
        if (cursor){
            return {.screen = storage ? PcScreen::STORAGE_MENU : PcScreen::PC_LIST, .cursor = cursor};
        }
    }
    if (is_white(extract_box_reference(game, MESSAGE_PATCH), 500, 40)){
        return {.screen = PcScreen::TEXT};
    }
    if (StartMenuDetector(COLOR_RED).detect(screen) || image_stats(game).average.sum() < DARK_SCREEN){
        return {};
    }
    return {.screen = PcScreen::OVERWORLD};
}


namespace{

std::string screen_slug(PcScreen screen){
    switch (screen){
    case PcScreen::OVERWORLD:       return "overworld";
    case PcScreen::TEXT:            return "text";
    case PcScreen::PC_LIST:         return "pc-list";
    case PcScreen::STORAGE_MENU:    return "storage-menu";
    case PcScreen::PARTY_PANEL:     return "party-panel";
    case PcScreen::BOX_GRID:        return "box-grid";
    case PcScreen::MON_MENU:        return "mon-menu";
    case PcScreen::BOX_CHOOSER:     return "box-chooser";
    case PcScreen::YES_NO:          return "yes-no";
    case PcScreen::OTHER:           return "other";
    }
    return "?";
}

std::string number_slug(const std::optional<size_t>& value){
    return value ? std::to_string(*value) : "none";
}

//  A capture named <anything>_<screen>_<cursor>_<box>, the cursor and the
//  box from 0 or "none", e.g. StorageDeposit_storage-menu_1_none.png.
class Test_PcScreen : public ImageUnitTest{
public:
    Test_PcScreen(const std::string& image)
        : ImageUnitTest("PokemonFRLG::PcScreenReader - " + image, {image})
    {}

    virtual UnitTestResult run_on_images(
        Logger& logger, CancellableScope& scope,
        const std::vector<ImageRGB32>& images
    ) const override{
        const std::vector<std::string> words = parse_words(Filesystem::Path(image_paths()[0]).stem().string());
        if (words.size() < 4){
            return "Error: filename must be <anything>_<screen>_<cursor>_<box>.";
        }
        const PcView view = read_pc_screen(logger, images[0]);
        TEST_RESULT_COMPONENT_EQUAL_STR(screen_slug(view.screen), words[words.size() - 3], "screen");
        TEST_RESULT_COMPONENT_EQUAL_STR(number_slug(view.cursor), words[words.size() - 2], "cursor");
        TEST_RESULT_COMPONENT_EQUAL_STR(number_slug(view.box), words.back(), "box");
        return true;
    }
};

}

void add_tests_PcScreenReader(UnitTestDatabase& database){
    //  FireRed on console B, 2026-11-02, Route 4's Pokemon Center: a deposit
    //  of the Magikarp (party slot 3), then of the Rattata (slot 2). A press
    //  sent before a menu was drawn went to the message behind it, so the
    //  messages must not read as the menus they come before.
    for (const char* image : {
        "PcBoot_text_none_none.png",
        "PcList_pc-list_0_none.png",
        "PcAccessed_text_none_none.png",
        "StorageOpened_text_none_none.png",
        "StorageWithdraw_storage-menu_0_none.png",
        "StorageDeposit_storage-menu_1_none.png",
        "PartyLead_party-panel_0_none.png",
        "PartySlot3_party-panel_2_none.png",
        //  The step presses A only on this menu's top row: RELEASE is on it.
        "MagikarpMenu_mon-menu_0_none.png",
        "RattataMenu_mon-menu_0_none.png",
        "DepositWhichBox_box-chooser_none_0.png",
        "MagikarpStored_party-panel_2_none.png",
        "RattataStored_party-panel_1_none.png",
        "ContinueDeposit_yes-no_1_none.png",
        //  WITHDRAW opened by a press sent too early: the hand on the box's name.
        "WithdrawBox_box-grid_none_none.png",
        "ContinueWithdraw_yes-no_1_none.png",
        "Overworld_overworld_none_none.png",
        "StartMenu_other_none_none.png",
    }){
        database.add<Test_PcScreen>(std::string("PokemonFRLG/PcScreenReader/") + image);
    }
}



}
}
}
