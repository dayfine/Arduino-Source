/*  Party Member Menu Reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <utility>
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonTools/OCR/OCR_SmallDictionaryMatcher.h"
#include "Tests/ImageUnitTest.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_OcrPreprocessing.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Settings.h"
#include "PokemonFRLG_PartyMemberMenuReader.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

//  Measured on a capture of the menu (console B, 2026-11-13, a Pidgey's
//  SUMMARY / FLY / SWITCH / ITEM / CANCEL), in the game's 240x160 pixels:
//  CANCEL's text from y 140 to 150, each row above it 16 higher; the names
//  from x 160, the cursor to their left.
constexpr double ROW_STEP = 16;
constexpr double BOTTOM_ROW_TOP = 140;
constexpr double ROW_HEIGHT = 10;
constexpr double TEXT_LEFT = 160;
constexpr double TEXT_WIDTH = 70;
//  SUMMARY, four field moves, SWITCH, ITEM and CANCEL.
constexpr size_t MAX_ROWS = 8;
//  SWITCH and CANCEL at least, under the field moves (an egg has no ITEM).
constexpr size_t MIN_PLAIN_ROWS_BELOW = 2;

//  The field moves (pokefirered party_menu.c, sFieldMoves), as the menu
//  writes them (src/data/text/move_names.h, in capitals).
constexpr std::pair<std::string_view, std::string_view> FIELD_MOVE_NAMES[] = {
    {"flash", "FLASH"},
    {"cut", "CUT"},
    {"fly", "FLY"},
    {"strength", "STRENGTH"},
    {"surf", "SURF"},
    {"rock-smash", "ROCK SMASH"},
    {"waterfall", "WATERFALL"},
    {"teleport", "TELEPORT"},
    {"dig", "DIG"},
    {"milk-drink", "MILK DRINK"},
    {"soft-boiled", "SOFTBOILED"},
    {"sweet-scent", "SWEET SCENT"},
};

//  The blue of a field move's name: (32, 80, 224) and its darker edges;
//  its shadow, (160, 192, 240), is left out.
const std::vector<OCR::TextColorRange>& BLUE_TEXT_FILTERS(){
    static const std::vector<OCR::TextColorRange> filters{
        {combine_rgb(0, 0, 160), combine_rgb(80, 120, 255)},
        {combine_rgb(0, 0, 128), combine_rgb(112, 152, 255)},
        {combine_rgb(0, 0, 112), combine_rgb(136, 168, 255)},
    };
    return filters;
}

class FieldMoveNameMatcher : public OCR::SmallDictionaryMatcher{
public:
    static constexpr double MAX_LOG10P = -1.40;
    static constexpr double MAX_LOG10P_SPREAD = 0.50;

    static const FieldMoveNameMatcher& instance(){
        static const FieldMoveNameMatcher matcher;
        return matcher;
    }

    std::string read(Logger& logger, Language language, const ImageViewRGB32& image) const{
        if (!languages()[language]){
            return "";
        }
        const ImageRGB32 ready = preprocess_for_ocr(image);
        const OCR::StringMatchResult result = match_substring_from_image_multifiltered(
            &logger, language, ready, BLUE_TEXT_FILTERS(),
            MAX_LOG10P, MAX_LOG10P_SPREAD, 0.01, 0.50
        );
        return result.results.empty() ? "" : result.results.begin()->second.token;
    }

private:
    FieldMoveNameMatcher()
        : SmallDictionaryMatcher(dictionary())
    {}

    static JsonObject dictionary(){
        JsonObject english;
        for (const auto& [slug, name] : FIELD_MOVE_NAMES){
            JsonArray names;
            names.push_back(std::string(name));
            english[std::string(slug)] = std::move(names);
        }
        JsonObject json;
        json["eng"] = std::move(english);
        return json;
    }
};

enum class RowKind{
    NONE,   //  not a row of the menu's white window
    PLAIN,  //  dark gray text: SUMMARY, SWITCH, ITEM, CANCEL
    FIELD,  //  blue text: a field move
};

//  Row `row`, counted up from CANCEL's row 0, as a box of the game screen.
ImageFloatBox row_box(size_t row){
    return ImageFloatBox(
        TEXT_LEFT / 240, (BOTTOM_ROW_TOP - ROW_STEP * double(row)) / 160, TEXT_WIDTH / 240, ROW_HEIGHT / 160
    );
}

bool is_white(uint32_t pixel){
    return ((pixel >> 16) & 0xff) > 192 && ((pixel >> 8) & 0xff) > 192 && (pixel & 0xff) > 176;
}
bool is_gray_text(uint32_t pixel){
    const int r = (pixel >> 16) & 0xff;
    const int g = (pixel >> 8) & 0xff;
    const int b = pixel & 0xff;
    return r < 136 && g < 136 && b < 136 && std::abs(r - b) < 32 && std::abs(g - b) < 32;
}
bool is_blue_text(uint32_t pixel){
    const int r = (pixel >> 16) & 0xff;
    const int g = (pixel >> 8) & 0xff;
    const int b = pixel & 0xff;
    return b > 176 && r < 96 && g < 136;
}

RowKind classify_row(const ImageViewRGB32& row){
    const size_t area = row.width() * row.height();
    if (area == 0){
        return RowKind::NONE;
    }
    size_t white = 0;
    size_t gray = 0;
    size_t blue = 0;
    for (size_t y = 0; y < row.height(); y++){
        for (size_t x = 0; x < row.width(); x++){
            const uint32_t pixel = row.pixel(x, y);
            white += is_white(pixel) ? 1 : 0;
            gray += is_gray_text(pixel) ? 1 : 0;
            blue += is_blue_text(pixel) ? 1 : 0;
        }
    }
    //  Measured: a name covers 5% to 25% of its row, the window's white
    //  the most of the rest; the party's slots behind the window are teal
    //  and blue.
    if (white * 2 < area){
        return RowKind::NONE;
    }
    if (blue * 50 >= area && gray * 4 < blue){
        return RowKind::FIELD;
    }
    if (gray * 50 >= area && blue * 4 < gray){
        return RowKind::PLAIN;
    }
    return RowKind::NONE;
}

} // namespace


std::optional<size_t> PartyMemberMenu::row_of(std::string_view move) const{
    for (size_t c = 0; c < field_moves.size(); c++){
        if (field_moves[c] == move){
            return c + 1;
        }
    }
    return std::nullopt;
}

bool PartyMemberMenu::all_read() const{
    for (const std::string& move : field_moves){
        if (move.empty()){
            return false;
        }
    }
    return true;
}

std::optional<PartyMemberMenu> read_party_member_menu(Logger& logger, Language language, const ImageViewRGB32& screen){
    const ImageViewRGB32 game = extract_box_reference(screen, GameSettings::instance().GAME_BOX);
    //  The rows from CANCEL up, to the first that isn't one.
    std::vector<RowKind> kinds;
    for (size_t row = 0; row < MAX_ROWS; row++){
        const RowKind kind = classify_row(extract_box_reference(game, row_box(row)));
        if (kind == RowKind::NONE){
            break;
        }
        kinds.push_back(kind);
    }
    //  Plain rows (CANCEL, ITEM, SWITCH), the field moves, then SUMMARY.
    size_t plain_below = 0;
    while (plain_below < kinds.size() && kinds[plain_below] == RowKind::PLAIN){
        plain_below++;
    }
    size_t fields = 0;
    while (plain_below + fields < kinds.size() && kinds[plain_below + fields] == RowKind::FIELD){
        fields++;
    }
    const size_t summary_row = plain_below + fields;
    const bool shape = fields == 0
        ? plain_below >= MIN_PLAIN_ROWS_BELOW + 1 && plain_below == kinds.size()
        : plain_below >= MIN_PLAIN_ROWS_BELOW && summary_row + 1 == kinds.size()
            && kinds[summary_row] == RowKind::PLAIN;
    if (!shape){
        return std::nullopt;
    }
    PartyMemberMenu menu;
    //  From the top: the field move under SUMMARY first.
    for (size_t c = 0; c < fields; c++){
        const size_t row = summary_row - 1 - c;
        menu.field_moves.push_back(
            FieldMoveNameMatcher::instance().read(logger, language, extract_box_reference(game, row_box(row)))
        );
    }
    return menu;
}


namespace{

//  A capture, named <anything>_<language code>.png, and what it shows:
//  no member's menu, or one listing `field_moves`.
class Test_PartyMemberMenuReader : public ImageUnitTest{
public:
    Test_PartyMemberMenuReader(const std::string& image, std::optional<std::vector<std::string>> field_moves)
        : ImageUnitTest(std::format("PokemonFRLG::PartyMemberMenuReader - {}", image), {image})
        , m_field_moves(std::move(field_moves))
    {}

    virtual UnitTestResult run_on_images(
        Logger& logger, CancellableScope&, const std::vector<ImageRGB32>& images
    ) const override{
        const std::optional<PartyMemberMenu> menu = read_party_member_menu(logger, Language::English, images[0]);
        if (!m_field_moves){
            return menu ? UnitTestResult("read a member's menu where none shows.") : UnitTestResult(true);
        }
        if (!menu){
            return "the member's menu isn't read.";
        }
        if (menu->field_moves != *m_field_moves){
            std::string read;
            for (const std::string& move : menu->field_moves){
                read += std::format("{}'{}'", read.empty() ? "" : ", ", move);
            }
            return std::format("field moves read: [{}].", read);
        }
        return true;
    }

private:
    std::optional<std::vector<std::string>> m_field_moves;
};

} // namespace

void add_tests_PartyMemberMenuReader(UnitTestDatabase& database){
    //  FireRed on console B, 2026-11-13: the Pidgey's menu, the cursor on
    //  SUMMARY, FLY in blue under it.
    database.add<Test_PartyMemberMenuReader>(
        "PokemonFRLG/PartyMemberMenuReader/PidgeyFly_eng.png", std::vector<std::string>{"fly"}
    );
    //  The party menu before A: no member's menu.
    database.add<Test_PartyMemberMenuReader>(
        "PokemonFRLG/PartyMemberMenuReader/PartyList_eng.png", std::nullopt
    );
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
