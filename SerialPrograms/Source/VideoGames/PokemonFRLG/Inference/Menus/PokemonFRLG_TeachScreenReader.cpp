/*  Teach Screen Reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <optional>
#include <utility>
#include "Common/Cpp/Color.h"
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTools/ImageStats.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonTools/OCR/OCR_SmallDictionaryMatcher.h"
#include "Tests/ImageUnitTest.h"
#include "Tests/TestUtils.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Settings.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Tests.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_OcrPreprocessing.h"
#include "PokemonFRLG_ItemListReader.h"
#include "PokemonFRLG_SummaryMovesReader.h"
#include "PokemonFRLG_TeachScreenReader.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

//  A box in the game's 240x160 pixels.
struct GameBox{
    double x;
    double y;
    double width;
    double height;
};

ImageViewRGB32 game_box(const ImageViewRGB32& screen){
    return extract_box_reference(screen, GameSettings::instance().GAME_BOX);
}

ImageViewRGB32 game_crop(const ImageViewRGB32& screen, const GameBox& box){
    return extract_box_reference(
        game_box(screen),
        ImageFloatBox(box.x / 240, box.y / 160, box.width / 240, box.height / 160)
    );
}

//  The box is all one color, near `color`.
bool solid(const ImageViewRGB32& screen, const GameBox& box, const FloatPixel& color){
    const ImageStats stats = image_stats(game_crop(screen, box));
    return euclidean_distance(stats.average, color) < 40 && stats.stddev.sum() < 40;
}

//  Measured on the captures (console B, 2026-11-03). Left of the title, the
//  frame's color tells the screen: the bag's orange, the TM CASE's mint,
//  the party menu's dark teal.
constexpr GameBox CORNER{6, 14, 8, 4};
const FloatPixel BAG_ORANGE(255, 210, 94);
const FloatPixel TM_CASE_MINT(193, 234, 207);
const FloatPixel PARTY_TEAL(18, 109, 107);
const FloatPixel WHITE(255, 255, 255);

//  The item menu's box, between its rows (OPEN at y 109, REGISTER 125,
//  CANCEL 141); blue text box or list below it when it is closed.
constexpr GameBox ITEM_MENU_GAP_1{186, 118, 40, 4};
constexpr GameBox ITEM_MENU_GAP_2{186, 134, 40, 4};
//  Over the party menu: the YES/NO box above the message box (YES at
//  y 77, NO at 93); the message box's right part, past the prompt box's
//  end (x 180) and its CANCEL; the prompt box under its text.
constexpr GameBox YES_NO_TOP{180, 72, 30, 4};
constexpr GameBox MESSAGE_RIGHT{200, 150, 30, 3};
constexpr GameBox PROMPT_BOTTOM{60, 150, 80, 3};

//  The cursors' dark arrows, 5 pixels wide and 9 high, rows 16 apart: the
//  bag's at x 90, y 12; the TM CASE's at x 81, y 12; the item menu's at
//  x 177, y 108; the party YES/NO's at x 169, y 76.
struct ArrowColumn{
    double x;
    double top;
    size_t rows;
};
constexpr double ARROW_ROW_STEP = 16;

std::optional<ArrowColumn> arrow_column(TeachScreen screen){
    switch (screen){
    case TeachScreen::BAG:          return ArrowColumn{89, 11, TEACH_LIST_ROWS};
    case TeachScreen::TM_CASE:      return ArrowColumn{80, 11, TM_CASE_ROWS};
    case TeachScreen::ITEM_MENU:    return ArrowColumn{176, 107, ITEM_MENU_ROWS};
    case TeachScreen::PARTY_YES_NO: return ArrowColumn{168, 75, 2};
    default:                        return std::nullopt;
    }
}

//  The share of the arrow's cell that is dark: about 0.35 with the arrow,
//  nothing without (the cell is left of the rows' text).
double dark_share(const ImageViewRGB32& cell){
    size_t dark = 0;
    for (size_t y = 0; y < cell.height(); y++){
        for (size_t x = 0; x < cell.width(); x++){
            const Color pixel(cell.pixel(x, y));
            dark += pixel.red() + pixel.green() + pixel.blue() < 330 ? 1 : 0;
        }
    }
    const size_t total = cell.width() * cell.height();
    return total == 0 ? 0 : double(dark) / double(total);
}


//  Small OCR dictionaries of English names: slug -> how the game writes it.
class NameMatcher : public OCR::SmallDictionaryMatcher{
public:
    NameMatcher(std::initializer_list<std::pair<const char*, const char*>> names)
        : SmallDictionaryMatcher(dictionary(names))
    {}

    //  The best match's log10p and slug, if any.
    std::optional<std::pair<double, std::string>> match(
        Logger& logger, Language language, const ImageViewRGB32& image,
        const std::vector<OCR::TextColorRange>& filters
    ) const{
        if (!languages()[language]){
            return std::nullopt;
        }
        const ImageRGB32 ready = preprocess_for_ocr(image);
        const OCR::StringMatchResult result = match_substring_from_image_multifiltered(
            &logger, language, ready, filters, -1.40, 0.50, 0.01, 0.50
        );
        if (result.results.empty()){
            return std::nullopt;
        }
        return std::make_pair(result.results.begin()->first, result.results.begin()->second.token);
    }
    std::string read(
        Logger& logger, Language language, const ImageViewRGB32& image,
        const std::vector<OCR::TextColorRange>& filters
    ) const{
        const std::optional<std::pair<double, std::string>> best = match(logger, language, image, filters);
        return best ? best->second : "";
    }

private:
    static JsonObject dictionary(std::initializer_list<std::pair<const char*, const char*>> names){
        JsonObject english;
        for (const auto& [slug, name] : names){
            JsonArray candidates;
            candidates.push_back(name);
            english[slug] = std::move(candidates);
        }
        JsonObject json;
        json["eng"] = std::move(english);
        return json;
    }
};

const NameMatcher& pocket_matcher(){
    static const NameMatcher matcher{
        {"items", "Items"},
        {"key-items", "Key Items"},
        {"poke-balls", "Poke Balls"},
    };
    return matcher;
}

//  The key items a story meets up to the Rocket Hideout (pokefirered
//  src/data/items.h).
const NameMatcher& key_item_matcher(){
    static const NameMatcher matcher{
        {"tm-case", "TM Case"},
        {"teachy-tv", "Teachy TV"},
        {"helix-fossil", "Helix Fossil"},
        {"dome-fossil", "Dome Fossil"},
        {"old-amber", "Old Amber"},
        {"oaks-parcel", "Oak's Parcel"},
        {"town-map", "Town Map"},
        {"bike-voucher", "Bike Voucher"},
        {"bicycle", "Bicycle"},
        {"ss-ticket", "S.S. Ticket"},
        {"old-rod", "Old Rod"},
        {"vs-seeker", "VS Seeker"},
        {"fame-checker", "Fame Checker"},
        {"berry-pouch", "Berry Pouch"},
        {"coin-case", "Coin Case"},
        {"cancel", "Cancel"},
    };
    return matcher;
}

//  A phrase from each message, as the captures show it.
const NameMatcher& text_matcher(){
    static const NameMatcher matcher{
        {"teach-which", "Teach which"},
        {"wants-to-learn", "wants to learn the"},
        {"knows-four", "knows four moves"},
        {"delete-question", "Should a move be deleted"},
        {"which-move", "Which move should be forgotten"},
        {"stop-question", "Stop trying to teach"},
        {"did-not-learn", "did not learn the"},
        {"not-compatible", "are not compatible"},
    };
    return matcher;
}

TeachText teach_text(const std::string& slug){
    static const std::vector<std::pair<std::string, TeachText>> texts{
        {"teach-which", TeachText::TEACH_WHICH},
        {"wants-to-learn", TeachText::WANTS_TO_LEARN},
        {"knows-four", TeachText::KNOWS_FOUR},
        {"delete-question", TeachText::DELETE_QUESTION},
        {"which-move", TeachText::WHICH_MOVE},
        {"stop-question", TeachText::STOP_QUESTION},
        {"did-not-learn", TeachText::DID_NOT_LEARN},
        {"not-compatible", TeachText::NOT_COMPATIBLE},
    };
    for (const auto& [name, text] : texts){
        if (name == slug){
            return text;
        }
    }
    return TeachText::UNKNOWN;
}

//  The text's lines: the pocket's title from x 16, white on the bag's
//  bar; a list row's name; the TM CASE's move after "No03 " (x 88 to 111);
//  the message box's two lines, y 124 and 139, from x 8; the prompt's line
//  at y 141.
constexpr GameBox POCKET_TITLE{8, 10, 76, 12};
constexpr double TM_MOVE_LEFT = 110;
constexpr GameBox MESSAGE_LINE_1{6, 121, 226, 13};
constexpr GameBox MESSAGE_LINE_2{6, 136, 226, 13};
constexpr GameBox PROMPT_LINE{6, 138, 170, 13};

//  A list's rows: dark text, and the cursor's row in blue (32, 80, 224).
const std::vector<OCR::TextColorRange>& LIST_TEXT_FILTERS(){
    static const std::vector<OCR::TextColorRange> filters = [](){
        std::vector<OCR::TextColorRange> all = DARK_TEXT_FILTERS();
        all.emplace_back(combine_rgb(0, 0, 128), combine_rgb(128, 144, 255));
        return all;
    }();
    return filters;
}

TeachScreen read_party_screen(const ImageViewRGB32& screen){
    if (solid(screen, YES_NO_TOP, WHITE) && solid(screen, MESSAGE_RIGHT, WHITE)){
        return TeachScreen::PARTY_YES_NO;
    }
    if (solid(screen, MESSAGE_RIGHT, WHITE)){
        return TeachScreen::PARTY_TEXT;
    }
    if (solid(screen, PROMPT_BOTTOM, WHITE)){
        return TeachScreen::PARTY_PROMPT;
    }
    return TeachScreen::OTHER;
}

}


const char* teach_screen_name(TeachScreen screen){
    switch (screen){
    case TeachScreen::OTHER:        return "other";
    case TeachScreen::BAG:          return "bag";
    case TeachScreen::ITEM_MENU:    return "item_menu";
    case TeachScreen::TM_CASE:      return "tm_case";
    case TeachScreen::PARTY_PROMPT: return "party_prompt";
    case TeachScreen::PARTY_TEXT:   return "party_text";
    case TeachScreen::PARTY_YES_NO: return "party_yes_no";
    case TeachScreen::KNOWN_MOVES:  return "known_moves";
    }
    return "?";
}

const char* teach_text_name(TeachText text){
    switch (text){
    case TeachText::UNKNOWN:         return "unknown";
    case TeachText::TEACH_WHICH:     return "teach_which";
    case TeachText::WANTS_TO_LEARN:  return "wants_to_learn";
    case TeachText::KNOWS_FOUR:      return "knows_four";
    case TeachText::DELETE_QUESTION: return "delete_question";
    case TeachText::WHICH_MOVE:      return "which_move";
    case TeachText::STOP_QUESTION:   return "stop_question";
    case TeachText::DID_NOT_LEARN:   return "did_not_learn";
    case TeachText::NOT_COMPATIBLE:  return "not_compatible";
    }
    return "?";
}

TeachScreen read_teach_screen(const ImageViewRGB32& screen){
    const bool menu = solid(screen, ITEM_MENU_GAP_1, WHITE) && solid(screen, ITEM_MENU_GAP_2, WHITE);
    if (solid(screen, CORNER, BAG_ORANGE)){
        return menu ? TeachScreen::ITEM_MENU : TeachScreen::BAG;
    }
    if (solid(screen, CORNER, TM_CASE_MINT)){
        return menu ? TeachScreen::ITEM_MENU : TeachScreen::TM_CASE;
    }
    if (solid(screen, CORNER, PARTY_TEAL)){
        return read_party_screen(screen);
    }
    if (read_summary_move_cursor(screen)){
        return TeachScreen::KNOWN_MOVES;
    }
    return TeachScreen::OTHER;
}

std::optional<size_t> read_teach_cursor(TeachScreen screen, const ImageViewRGB32& image){
    if (screen == TeachScreen::KNOWN_MOVES){
        return read_summary_move_cursor(image);
    }
    const std::optional<ArrowColumn> column = arrow_column(screen);
    if (!column){
        return std::nullopt;
    }
    std::optional<size_t> cursor;
    for (size_t row = 0; row < column->rows; row++){
        const GameBox cell{column->x, column->top + ARROW_ROW_STEP * double(row), 7, 10};
        const double share = dark_share(game_crop(image, cell));
        if (share < 0.15){
            continue;
        }
        if (cursor){
            return std::nullopt;
        }
        cursor = row;
    }
    return cursor;
}

std::string read_pocket_name(Logger& logger, Language language, const ImageViewRGB32& screen){
    return pocket_matcher().read(logger, language, game_crop(screen, POCKET_TITLE), BRIGHT_TEXT_FILTERS());
}

std::string read_key_item(Logger& logger, Language language, const ImageViewRGB32& screen, size_t row){
    const ImageViewRGB32 name = extract_box_reference(game_box(screen), item_list_name_box(int(row)));
    return key_item_matcher().read(logger, language, name, LIST_TEXT_FILTERS());
}

std::string read_tm_case_move(Logger& logger, Language language, const ImageViewRGB32& screen, size_t row){
    const ImageFloatBox name = item_list_name_box(int(row));
    const GameBox box{TM_MOVE_LEFT, name.y * 160, 200 - TM_MOVE_LEFT, name.height * 160};
    return read_move_name(logger, language, game_crop(screen, box));
}

TeachText read_teach_text(Logger& logger, Language language, const ImageViewRGB32& screen){
    const TeachScreen kind = read_teach_screen(screen);
    if (kind == TeachScreen::PARTY_PROMPT){
        return teach_text(text_matcher().read(logger, language, game_crop(screen, PROMPT_LINE), DARK_TEXT_FILTERS()));
    }
    if (kind != TeachScreen::PARTY_TEXT && kind != TeachScreen::PARTY_YES_NO){
        return TeachText::UNKNOWN;
    }
    //  The phrase either line matches best.
    std::optional<std::pair<double, std::string>> best;
    for (const GameBox& line : {MESSAGE_LINE_1, MESSAGE_LINE_2}){
        const std::optional<std::pair<double, std::string>> match =
            text_matcher().match(logger, language, game_crop(screen, line), DARK_TEXT_FILTERS());
        if (match && (!best || match->first < best->first)){
            best = match;
        }
    }
    return best ? teach_text(best->second) : TeachText::UNKNOWN;
}

std::vector<std::string> read_teach_rows(Logger& logger, Language language, const ImageViewRGB32& image, TeachScreen screen){
    std::vector<std::string> rows;
    const size_t rows_shown = screen == TeachScreen::BAG ? TEACH_LIST_ROWS : TM_CASE_ROWS;
    for (size_t row = 0; row < rows_shown; row++){
        std::string name = screen == TeachScreen::BAG
            ? read_key_item(logger, language, image, row)
            : read_tm_case_move(logger, language, image, row);
        if (name.empty()){
            break;
        }
        rows.push_back(std::move(name));
        if (rows.back() == "cancel"){
            break;
        }
    }
    return rows;
}


namespace{

//  The language a test image's name ends with: <anything>_<language code>.
Language language_of_test_image(const std::string& image){
    const std::vector<std::string> words = parse_words(Filesystem::Path(image).stem().string());
    return words.size() < 2 ? Language::None : language_code_to_enum(words.back());
}

std::string cursor_value(const std::optional<size_t>& cursor){
    return cursor ? std::to_string(*cursor + 1) : "none";
}

//  What a list or menu shows: its rows from the top to the first not read,
//  and its cursor.
void add_list(
    std::vector<std::string>& labels, std::vector<std::string>& values,
    const std::vector<std::string>& rows, const std::optional<size_t>& cursor
){
    for (size_t c = 0; c < rows.size(); c++){
        labels.push_back(std::format("row {}", c + 1));
        values.push_back(rows[c]);
    }
    labels.push_back("cursor");
    values.push_back(cursor_value(cursor));
}

//  A capture of a screen on the way to a TM taught, named
//  <anything>_<language code>.png. Its golden file holds the screen read,
//  then: in the bag, its pocket, the key items from the top (KEY ITEMS
//  only) and the cursor's row (from 1, or "none"); in the TM CASE, its
//  moves from the top and the cursor's row; in an item menu or YES/NO, the
//  cursor's row; over the party menu, the message; on KNOWN MOVES, the five
//  rows' moves and the cursor's row.
class Test_TeachScreen : public ImageUnitTest{
public:
    Test_TeachScreen(const std::string& image)
        : ImageUnitTest("PokemonFRLG::TeachScreenReader - " + image, {image})
    {}

    virtual UnitTestResult run_on_images(
        Logger& logger, CancellableScope& scope,
        const std::vector<ImageRGB32>& images
    ) const override{
        const Language language = language_of_test_image(image_paths()[0]);
        if (language == Language::None || language == Language::EndOfList){
            return "Error: filename must be <anything>_<language code>.";
        }
        const ImageViewRGB32& screen = images[0];
        const TeachScreen kind = read_teach_screen(screen);
        std::vector<std::string> labels{"screen"};
        std::vector<std::string> values{teach_screen_name(kind)};
        add_details(logger, language, screen, kind, labels, values);
        return check_against_golden_file(UNIT_TEST_RESOURCE_PATH() + image_paths()[0], labels, values);
    }

private:
    static void add_details(
        Logger& logger, Language language, const ImageViewRGB32& screen, TeachScreen kind,
        std::vector<std::string>& labels, std::vector<std::string>& values
    ){
        switch (kind){
        case TeachScreen::BAG:{
            const std::string pocket = read_pocket_name(logger, language, screen);
            labels.push_back("pocket");
            values.push_back(pocket.empty() ? "none" : pocket);
            const std::vector<std::string> rows = pocket == "key-items"
                ? read_teach_rows(logger, language, screen, kind)
                : std::vector<std::string>{};
            add_list(labels, values, rows, read_teach_cursor(kind, screen));
            return;
        }
        case TeachScreen::TM_CASE:
            add_list(labels, values, read_teach_rows(logger, language, screen, kind), read_teach_cursor(kind, screen));
            return;
        case TeachScreen::ITEM_MENU:
            add_list(labels, values, {}, read_teach_cursor(kind, screen));
            return;
        case TeachScreen::PARTY_YES_NO:
            labels.push_back("cursor");
            values.push_back(cursor_value(read_teach_cursor(kind, screen)));
            [[fallthrough]];
        case TeachScreen::PARTY_PROMPT:
        case TeachScreen::PARTY_TEXT:
            labels.push_back("text");
            values.push_back(teach_text_name(read_teach_text(logger, language, screen)));
            return;
        case TeachScreen::KNOWN_MOVES:
            for (size_t row = 0; row < SUMMARY_MOVE_ROWS; row++){
                const std::string move = read_summary_move_name(logger, language, screen, row);
                labels.push_back(std::format("move {}", row + 1));
                values.push_back(move.empty() ? "none" : move);
            }
            labels.push_back("cursor");
            values.push_back(cursor_value(read_teach_cursor(kind, screen)));
            return;
        case TeachScreen::OTHER:
            return;
        }
    }
};

}

void add_tests_TeachScreenReader(UnitTestDatabase& database){
    //  FireRed on console B, 2026-11-03: TM03 taught to the lead from the
    //  bag, B on the move to forget, and YES to stop.
    for (const char* image : {
        "01_BagItems_eng.png",
        "02_BagKeyItems_eng.png",
        "03_KeyItemsTmCase_eng.png",
        "04_TmCaseMenu_eng.png",
        "05_TmCaseList_eng.png",
        "06_Tm03Menu_eng.png",
        "07_TeachWhich_eng.png",
        "08_WantsToLearn_eng.png",
        "09_KnowsFour_eng.png",
        "10_DeleteQuestion_eng.png",
        "11_WhichMove_eng.png",
        "12_KnownMoves_eng.png",
        "13_StopQuestion_eng.png",
        "14_DidNotLearn_eng.png",
    }){
        database.add<Test_TeachScreen>(std::string("PokemonFRLG/TeachScreenReader/") + image);
    }
}



}
}
}
