/*  Summary Moves Reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <string_view>
#include <utility>
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonTools/OCR/OCR_SmallDictionaryMatcher.h"
#include "Tests/ImageUnitTest.h"
#include "Tests/TestUtils.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Tests.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_DigitReader.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_OcrPreprocessing.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_StatsReader.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_SummaryDetector.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Settings.h"
#include "PokemonFRLG_SummaryMovesReader.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

//  Measured on captures of the page (console B, 2026-11-02): names from
//  x 163, y 24 to 32 (with the shadow); the PP's digits y 34 to 43, the
//  current PP's to x 216, "/" from 217 to 224, the full PP's from 224.
constexpr double ROW_STEP = 28;
constexpr double NAME_TOP = 22;
constexpr double PP_TOP = 33;
//  A move's type badge, x 123 to 154 and y 22 to 33 on the top row: an
//  empty slot shows the pane's light background there instead.
constexpr double BADGE_LEFT = 125;
constexpr double BADGE_TOP = 24;
constexpr double BADGE_WIDTH = 27;
constexpr double BADGE_HEIGHT = 8;
//  The cursor box's left edge, x 120 to 121, from y 20 to 44 on the top
//  row: (255, 72, 0).
constexpr double CURSOR_EDGE_LEFT = 120;
constexpr double CURSOR_EDGE_TOP = 24;
constexpr double CURSOR_EDGE_HEIGHT = 16;

//  The level-up moves of Squirtle's, Magikarp's and Oddish's lines (pret
//  pokefirered src/data/pokemon/level_up_learnsets.h), then the TMs'
//  and HMs' moves not among them (src/data/items.h; Hyper Beam, Protect
//  and Rain Dance are above), as the game writes them
//  (src/data/text/move_names.h).
constexpr std::pair<std::string_view, std::string_view> MOVE_NAMES[] = {
    {"tackle", "Tackle"},
    {"tail-whip", "Tail Whip"},
    {"bubble", "Bubble"},
    {"withdraw", "Withdraw"},
    {"water-gun", "Water Gun"},
    {"bite", "Bite"},
    {"rapid-spin", "Rapid Spin"},
    {"protect", "Protect"},
    {"rain-dance", "Rain Dance"},
    {"skull-bash", "Skull Bash"},
    {"hydro-pump", "Hydro Pump"},
    {"splash", "Splash"},
    {"flail", "Flail"},
    {"thrash", "Thrash"},
    {"dragon-rage", "Dragon Rage"},
    {"leer", "Leer"},
    {"twister", "Twister"},
    {"dragon-dance", "Dragon Dance"},
    {"absorb", "Absorb"},
    {"sweet-scent", "Sweet Scent"},
    {"poison-powder", "PoisonPowder"},
    {"stun-spore", "Stun Spore"},
    {"sleep-powder", "Sleep Powder"},
    {"acid", "Acid"},
    {"moonlight", "Moonlight"},
    {"petal-dance", "Petal Dance"},
    {"aromatherapy", "Aromatherapy"},
    {"mega-drain", "Mega Drain"},
    {"magical-leaf", "Magical Leaf"},
    {"hyper-beam", "Hyper Beam"},
    {"focus-punch", "Focus Punch"},
    {"dragon-claw", "Dragon Claw"},
    {"water-pulse", "Water Pulse"},
    {"calm-mind", "Calm Mind"},
    {"roar", "Roar"},
    {"toxic", "Toxic"},
    {"hail", "Hail"},
    {"bulk-up", "Bulk Up"},
    {"bullet-seed", "Bullet Seed"},
    {"hidden-power", "Hidden Power"},
    {"sunny-day", "Sunny Day"},
    {"taunt", "Taunt"},
    {"ice-beam", "Ice Beam"},
    {"blizzard", "Blizzard"},
    {"light-screen", "Light Screen"},
    {"giga-drain", "Giga Drain"},
    {"safeguard", "Safeguard"},
    {"frustration", "Frustration"},
    {"solar-beam", "SolarBeam"},
    {"iron-tail", "Iron Tail"},
    {"thunderbolt", "Thunderbolt"},
    {"thunder", "Thunder"},
    {"earthquake", "Earthquake"},
    {"return", "Return"},
    {"dig", "Dig"},
    {"psychic", "Psychic"},
    {"shadow-ball", "Shadow Ball"},
    {"brick-break", "Brick Break"},
    {"double-team", "Double Team"},
    {"reflect", "Reflect"},
    {"shock-wave", "Shock Wave"},
    {"flamethrower", "Flamethrower"},
    {"sludge-bomb", "Sludge Bomb"},
    {"sandstorm", "Sandstorm"},
    {"fire-blast", "Fire Blast"},
    {"rock-tomb", "Rock Tomb"},
    {"aerial-ace", "Aerial Ace"},
    {"torment", "Torment"},
    {"facade", "Facade"},
    {"secret-power", "Secret Power"},
    {"rest", "Rest"},
    {"attract", "Attract"},
    {"thief", "Thief"},
    {"steel-wing", "Steel Wing"},
    {"skill-swap", "Skill Swap"},
    {"snatch", "Snatch"},
    {"overheat", "Overheat"},
    {"cut", "Cut"},
    {"fly", "Fly"},
    {"surf", "Surf"},
    {"strength", "Strength"},
    {"flash", "Flash"},
    {"rock-smash", "Rock Smash"},
    {"waterfall", "Waterfall"},
};

class MoveNameMatcher : public OCR::SmallDictionaryMatcher{
public:
    static constexpr double MAX_LOG10P = -1.40;
    static constexpr double MAX_LOG10P_SPREAD = 0.50;

    static const MoveNameMatcher& instance(){
        static const MoveNameMatcher matcher;
        return matcher;
    }

    std::string read(Logger& logger, Language language, const ImageViewRGB32& image) const{
        if (!languages()[language]){
            return "";
        }
        const ImageRGB32 ready = preprocess_for_ocr(image);
        const OCR::StringMatchResult result = match_substring_from_image_multifiltered(
            &logger, language, ready, DARK_TEXT_FILTERS(),
            MAX_LOG10P, MAX_LOG10P_SPREAD, 0.01, 0.50
        );
        return result.results.empty() ? "" : result.results.begin()->second.token;
    }

private:
    MoveNameMatcher()
        : SmallDictionaryMatcher(dictionary())
    {}

    static JsonObject dictionary(){
        JsonObject english;
        for (const auto& [slug, name] : MOVE_NAMES){
            JsonArray names;
            names.push_back(std::string(name));
            english[std::string(slug)] = std::move(names);
        }
        JsonObject json;
        json["eng"] = std::move(english);
        return json;
    }
};

ImageViewRGB32 game_box(const ImageViewRGB32& screen){
    return extract_box_reference(screen, GameSettings::instance().GAME_BOX);
}

bool cursor_red(uint32_t pixel){
    const int r = (pixel >> 16) & 0xff;
    const int g = (pixel >> 8) & 0xff;
    const int b = pixel & 0xff;
    return r > 200 && g < 140 && b < 90;
}

//  Whether row `row` holds a move: its type badge shows, mostly darker
//  than the pane's light background.
bool row_has_move(const ImageViewRGB32& screen, size_t row){
    const ImageFloatBox box(
        BADGE_LEFT / 240, (BADGE_TOP + ROW_STEP * double(row)) / 160, BADGE_WIDTH / 240, BADGE_HEIGHT / 160
    );
    const ImageViewRGB32 badge = extract_box_reference(game_box(screen), box);
    size_t dark = 0;
    for (size_t y = 0; y < badge.height(); y++){
        for (size_t x = 0; x < badge.width(); x++){
            const uint32_t pixel = badge.pixel(x, y);
            dark += ((pixel >> 16) & 0xff) + ((pixel >> 8) & 0xff) + (pixel & 0xff) < 600 ? 1 : 0;
        }
    }
    //  Measured: a third to three quarters of a badge; none of an empty slot.
    return dark * 5 > badge.width() * badge.height();
}

int read_pp(Logger& logger, const ImageViewRGB32& screen, const ImageFloatBox& box){
    const int pp = read_digits_waterfill_template(logger, extract_box_reference(game_box(screen), box), DigitTemplateType::StatBox);
    //  A move has at most 64 PP (40 with three PP Ups).
    return pp >= 0 && pp <= 64 ? pp : -1;
}

//  Where row `row` (from 0) shows its move's name, its current PP and its
//  full PP, as fractions of the game screen.
ImageFloatBox summary_move_name_box(size_t row){
    return ImageFloatBox(161.0 / 240, (NAME_TOP + ROW_STEP * double(row)) / 160, 76.0 / 240, 12.0 / 160);
}
ImageFloatBox summary_move_pp_box(size_t row){
    return ImageFloatBox(204.0 / 240, (PP_TOP + ROW_STEP * double(row)) / 160, 13.0 / 240, 11.0 / 160);
}
ImageFloatBox summary_move_max_pp_box(size_t row){
    return ImageFloatBox(224.0 / 240, (PP_TOP + ROW_STEP * double(row)) / 160, 15.0 / 240, 11.0 / 160);
}

} // namespace


std::string read_summary_move_name(Logger& logger, Language language, const ImageViewRGB32& screen, size_t row){
    return read_move_name(logger, language, extract_box_reference(game_box(screen), summary_move_name_box(row)));
}

std::string read_move_name(Logger& logger, Language language, const ImageViewRGB32& name){
    return MoveNameMatcher::instance().read(logger, language, name);
}

std::vector<SummaryMove> read_summary_moves(Logger& logger, Language language, const ImageViewRGB32& screen){
    std::vector<SummaryMove> moves;
    for (size_t row = 0; row < SUMMARY_MOVE_ROWS - 1; row++){
        //  The game packs the moves to the top: the first empty slot ends them.
        if (!row_has_move(screen, row)){
            break;
        }
        moves.push_back(SummaryMove{
            .move = read_summary_move_name(logger, language, screen, row),
            .pp = read_pp(logger, screen, summary_move_pp_box(row)),
            .max_pp = read_pp(logger, screen, summary_move_max_pp_box(row)),
        });
    }
    return moves;
}

std::optional<size_t> read_summary_move_cursor(const ImageViewRGB32& screen){
    const ImageViewRGB32 game = game_box(screen);
    std::optional<size_t> cursor;
    for (size_t row = 0; row < SUMMARY_MOVE_ROWS; row++){
        const ImageFloatBox box(
            CURSOR_EDGE_LEFT / 240, (CURSOR_EDGE_TOP + ROW_STEP * double(row)) / 160,
            2.0 / 240, CURSOR_EDGE_HEIGHT / 160
        );
        const ImageViewRGB32 edge = extract_box_reference(game, box);
        size_t red = 0;
        for (size_t y = 0; y < edge.height(); y++){
            for (size_t x = 0; x < edge.width(); x++){
                red += cursor_red(edge.pixel(x, y)) ? 1 : 0;
            }
        }
        //  Most of the edge: the box is drawn there and nowhere else.
        if (red * 2 > edge.width() * edge.height()){
            if (cursor){
                return std::nullopt;
            }
            cursor = row;
        }
    }
    return cursor;
}


namespace{

//  A capture of a summary page, named <anything>_<language code>.png. Its
//  golden file holds the page shown; on the first page the species and the
//  level; on KNOWN MOVES, or a move being picked, each move's name, PP and
//  full PP ("unknown" for a move with no name read), then the cursor's
//  row (from 1) or "none".
class Test_SummaryScreen : public ImageUnitTest{
public:
    Test_SummaryScreen(const std::string& image)
        : ImageUnitTest(std::format("PokemonFRLG::SummaryMovesReader - {}", image), {image})
    {}

    virtual UnitTestResult run_on_images(
        Logger& logger, CancellableScope& scope,
        const std::vector<ImageRGB32>& images
    ) const override{
        const std::vector<std::string> words = parse_words(Filesystem::Path(image_paths()[0]).stem().string());
        const Language language = words.size() < 2 ? Language::None : language_code_to_enum(words.back());
        if (language == Language::None || language == Language::EndOfList){
            return "Error: filename must be <anything>_<language code>.";
        }
        const ImageViewRGB32& screen = images[0];
        std::vector<std::string> labels;
        std::vector<std::string> values;
        const bool info = SummaryDetector(COLOR_RED).detect(screen);
        const bool skills = SummaryPage2Detector(COLOR_RED).detect(screen);
        const bool moves = SummaryPage3Detector(COLOR_RED).detect(screen);
        //  Picking a move turns the page's dot orange: no page detector sees it.
        const std::optional<size_t> cursor = read_summary_move_cursor(screen);
        labels.push_back("page");
        values.push_back(info ? "info" : skills ? "skills" : moves ? "moves" : cursor ? "picking" : "none");
        if (info){
            PokemonFRLG_Stats stats;
            StatsReader().read_page1(logger, language, screen, stats);
            labels.push_back("species");
            values.push_back(stats.name.empty() ? "none" : stats.name);
            labels.push_back("level");
            values.push_back(stats.level ? std::to_string(*stats.level) : "none");
        }
        if (moves || cursor){
            const std::vector<SummaryMove> known = read_summary_moves(logger, language, screen);
            for (size_t c = 0; c < known.size(); c++){
                labels.push_back(std::format("move {}", c + 1));
                values.push_back(known[c].move.empty() ? "unknown" : known[c].move);
                labels.push_back(std::format("move {} PP", c + 1));
                values.push_back(std::to_string(known[c].pp));
                labels.push_back(std::format("move {} full PP", c + 1));
                values.push_back(std::to_string(known[c].max_pp));
            }
            labels.push_back("cursor");
            values.push_back(cursor ? std::to_string(*cursor + 1) : "none");
        }
        return check_against_golden_file(UNIT_TEST_RESOURCE_PATH() + image_paths()[0], labels, values);
    }
};

} // namespace

void add_tests_SummaryMovesReader(UnitTestDatabase& database){
    //  FireRed on console B, 2026-11-02: the story's Squirtle at Lv16, not
    //  evolved, with WATER GUN where TACKLE was: the battle had forgotten the
    //  top move for Water Gun instead of BUBBLE.
    database.add<Test_SummaryScreen>("PokemonFRLG/SummaryMovesReader/Info_eng.png");
    database.add<Test_SummaryScreen>("PokemonFRLG/SummaryMovesReader/Skills_eng.png");
    database.add<Test_SummaryScreen>("PokemonFRLG/SummaryMovesReader/KnownMoves_eng.png");
    //  A on KNOWN MOVES: the cursor box on the top move.
    database.add<Test_SummaryScreen>("PokemonFRLG/SummaryMovesReader/MoveDetail_eng.png");
    //  The party menu: no summary page.
    database.add<Test_SummaryScreen>("PokemonFRLG/SummaryMovesReader/PartyMenu_eng.png");
    //  FireRed on console B, 2026-11-03: the Oddish caught for Cut, Lv12, with
    //  ABSORB, SWEET SCENT and two empty slots.
    database.add<Test_SummaryScreen>("PokemonFRLG/SummaryMovesReader/Oddish_KnownMoves_eng.png");
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
