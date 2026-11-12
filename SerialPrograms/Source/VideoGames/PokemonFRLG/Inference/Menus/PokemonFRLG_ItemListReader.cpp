/*  Item List Reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonFramework/Language.h"
#include "CommonTools/OCR/OCR_SmallDictionaryMatcher.h"
#include "Tests/ImageUnitTest.h"
#include "Tests/TestUtils.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Tests.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_DigitReader.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_OcrPreprocessing.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Settings.h"
#include "PokemonFRLG_ItemListReader.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

//  Measured on a capture of the Items pocket (console B, 2026-11-01): text
//  rows from y 13 to 21, 16 apart; names from x 97, "x" at 198, the count
//  to 221.
constexpr double ROW_TOP = 11;
constexpr double ROW_STEP = 16;
constexpr double ROW_HEIGHT = 12;

//  Each item's slug and its English name.
constexpr std::pair<std::string_view, std::string_view> ENGLISH_ITEM_NAMES[] = {
    {"potion", "Potion"},
    {"antidote", "Antidote"},
    {"parlyz-heal", "Parlyz Heal"},
    {"awakening", "Awakening"},
    {"poke-ball", "Poke Ball"},
    {"repel", "Repel"},
    {"super-repel", "Super Repel"},
    {"max-repel", "Max Repel"},
    {"escape-rope", "Escape Rope"},
    {"x-special", "X Special"},
    {"x-speed", "X Speed"},
    {"x-accuracy", "X Accuracy"},
    {"guard-spec", "Guard Spec."},
    {"bicycle", "Bicycle"},
    //  Route 17's hidden items. Without them a RARE CANDY read
    //  "FARE CANCY" and matched CANCEL: the pocket's read ended
    //  above it, and 11.02 counted no RARE CANDY in a bag with 2
    //  (console B, 2026-11-12).
    {"rare-candy", "Rare Candy"},
    {"max-elixir", "Max Elixir"},
    {"cancel", "Cancel"},
};

class ItemNameMatcher : public OCR::SmallDictionaryMatcher{
public:
    static constexpr double MAX_LOG10P = -1.40;
    static constexpr double MAX_LOG10P_SPREAD = 0.50;

    static const ItemNameMatcher& instance(){
        static const ItemNameMatcher matcher;
        return matcher;
    }

    std::string match(Language language, const std::string& text) const{
        if (!languages()[language]){
            return "";
        }
        OCR::StringMatchResult result = match_substring(language, text, MAX_LOG10P_SPREAD);
        result.clear_beyond_log10p(MAX_LOG10P);
        return result.results.empty() ? "" : result.results.begin()->second.token;
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
    ItemNameMatcher()
        : SmallDictionaryMatcher(dictionary())
    {}

    static JsonObject dictionary(){
        JsonObject english;
        for (const auto& [slug, name] : ENGLISH_ITEM_NAMES){
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

//  A row from its name's and its count's reads: a row with a count that
//  matched CANCEL is an item whose name can't be read, as the list's
//  CANCEL shows no count.
BagRow bag_row(std::string item, int count){
    if (item == "cancel"){
        return count > 0 ? BagRow{"", count} : BagRow{std::move(item), 0};
    }
    return BagRow{std::move(item), count};
}

//  Where row `row` of the bag shows its item count.
ImageFloatBox bag_item_count_box(int row){
    return ImageFloatBox(208.0 / 240, (ROW_TOP + ROW_STEP * row) / 160, 15.0 / 240, ROW_HEIGHT / 160);
}

} // namespace


ImageFloatBox item_list_name_box(int row){
    return ImageFloatBox(96.0 / 240, (ROW_TOP + ROW_STEP * row) / 160, 72.0 / 240, ROW_HEIGHT / 160);
}

std::string read_item_list_name(Logger& logger, Language language, const ImageViewRGB32& screen, int row){
    return ItemNameMatcher::instance().read(logger, language, extract_box_reference(game_box(screen), item_list_name_box(row)));
}

bool item_list_reader_knows(std::string_view item){
    return std::any_of(std::begin(ENGLISH_ITEM_NAMES), std::end(ENGLISH_ITEM_NAMES), [&](const auto& entry){
        return entry.first == item;
    });
}

BagRow read_bag_row(Logger& logger, Language language, const ImageViewRGB32& screen, int shown){
    std::string item = read_item_list_name(logger, language, screen, shown);
    const ImageViewRGB32 count = extract_box_reference(game_box(screen), bag_item_count_box(shown));
    //  Not Tesseract on each waterfilled digit: the font joins some strokes
    //  only at a corner, so the 5 and the 3 fell apart into pieces and read
    //  as nothing (console B, 2026-11-02). The stat boxes' templates are
    //  the same digits on the same cream.
    return bag_row(std::move(item), read_digits_waterfill_template(logger, count, DigitTemplateType::StatBox));
}

std::vector<BagRow> read_bag_pocket(Logger& logger, Language language, const ImageViewRGB32& screen){
    std::vector<BagRow> rows;
    for (int row = 0; row < ITEM_LIST_ROWS; row++){
        BagRow read = read_bag_row(logger, language, screen, row);
        if (read.item == "cancel"){
            break;
        }
        rows.push_back(std::move(read));
    }
    return pocket_rows(std::move(rows));
}

std::vector<BagRow> pocket_rows(std::vector<BagRow> reads){
    const auto cancel = std::find_if(reads.begin(), reads.end(), [](const BagRow& row){
        return row.item == "cancel";
    });
    reads.erase(cancel, reads.end());
    while (!reads.empty() && reads.back().item.empty()){
        reads.pop_back();
    }
    return reads;
}


namespace{

//  The language a test image's name ends with: <anything>_<language code>.
Language language_of_test_image(const std::string& image){
    const std::vector<std::string> words = parse_words(Filesystem::Path(image).stem().string());
    return words.size() < 2 ? Language::None : language_code_to_enum(words.back());
}

//  A capture of a bag pocket, read from the top. Its golden file holds each
//  row's item and count, one per line.
class Test_BagPocket : public ImageUnitTest{
public:
    Test_BagPocket(const std::string& image)
        : ImageUnitTest(std::format("PokemonFRLG::ItemListReader - {}", image), {image})
    {}

    virtual UnitTestResult run_on_images(
        Logger& logger, CancellableScope& scope,
        const std::vector<ImageRGB32>& images
    ) const override{
        const Language language = language_of_test_image(image_paths()[0]);
        if (language == Language::None || language == Language::EndOfList){
            return "Error: filename must be <anything>_<language code>.";
        }
        std::vector<std::string> labels;
        std::vector<std::string> values;
        const std::vector<BagRow> rows = read_bag_pocket(logger, language, images[0]);
        for (size_t c = 0; c < rows.size(); c++){
            labels.push_back(std::format("row {} item", c + 1));
            labels.push_back(std::format("row {} count", c + 1));
            values.push_back(rows[c].item.empty() ? "(unread)" : rows[c].item);
            values.push_back(std::to_string(rows[c].count));
        }
        return check_against_golden_file(UNIT_TEST_RESOURCE_PATH() + image_paths()[0], labels, values);
    }
};

//  11.02's counts (console B, 2026-11-12): OCR read the RARE CANDY's row
//  "FARE CANCY", which matched CANCEL with no Rare Candy in the dictionary,
//  and the pocket's read ended above it. A row with a count is never the
//  CANCEL, whatever its name matched.
class Test_Route17ItemNames : public UnitTest{
public:
    Test_Route17ItemNames()
        : UnitTest("PokemonFRLG::ItemListReader - 11.02 the RARE CANDY's row is not the CANCEL")
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const ItemNameMatcher& matcher = ItemNameMatcher::instance();
        for (const std::string& text : {"FARE CANCY", "RARE CANOY", "RARE CANDY"}){
            const std::string item = matcher.match(Language::English, text);
            if (item != "rare-candy"){
                return std::format("Expected \"{}\" read as rare-candy, got \"{}\".", text, item);
            }
        }
        if (matcher.match(Language::English, "MAX ELIXIR") != "max-elixir" || matcher.match(Language::English, "CANCEL") != "cancel"){
            return "Expected MAX ELIXIR and CANCEL read as themselves.";
        }
        const BagRow counted = bag_row("cancel", 2);
        if (!counted.item.empty() || counted.count != 2){
            return std::format("Expected a CANCEL with 2 read as an unread item x2, got \"{}\" x{}.", counted.item, counted.count);
        }
        if (bag_row("cancel", -1).item != "cancel"){
            return "Expected a CANCEL with no count kept as the CANCEL.";
        }
        return true;
    }
};

//  11.03's count (console B, 2026-11-12): the FULL RESTORE's row matched
//  several names with no Full Restore in the dictionary, read as nothing,
//  and the count saw 0 after the Safari Zone's ball was picked up. The
//  other names the story counts or buys, and those it keeps rows for,
//  are each read as themselves, and the shorter names they contain stay
//  apart.
class Test_StoryItemNames : public UnitTest{
public:
    Test_StoryItemNames()
        : UnitTest("PokemonFRLG::ItemListReader - 11.03 the FULL RESTORE's row is read")
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const ItemNameMatcher& matcher = ItemNameMatcher::instance();
        for (const auto& [text, expected] : {
            std::pair<std::string_view, std::string_view>{"FULL RESTORE", "full-restore"},
            {"SUPER POTION", "super-potion"},
            {"MOON STONE", "moon-stone"},
            {"NUGGET", "nugget"},
            {"POTION", "potion"},
            {"REPEL", "repel"},
            {"SUPER REPEL", "super-repel"},
            {"MAX REPEL", "max-repel"},
        }){
            const std::string item = matcher.match(Language::English, std::string(text));
            if (item != expected){
                return std::format("Expected \"{}\" read as {}, got \"{}\".", text, expected, item);
            }
        }
        return true;
    }
};

//  A capture of a Mart's BUY list, read from the top to its CANCEL. Its
//  golden file holds each row's item, one per line.
class Test_MartList : public ImageUnitTest{
public:
    Test_MartList(const std::string& image)
        : ImageUnitTest(std::format("PokemonFRLG::ItemListReader - {}", image), {image})
    {}

    virtual UnitTestResult run_on_images(
        Logger& logger, CancellableScope& scope,
        const std::vector<ImageRGB32>& images
    ) const override{
        const Language language = language_of_test_image(image_paths()[0]);
        if (language == Language::None || language == Language::EndOfList){
            return "Error: filename must be <anything>_<language code>.";
        }
        std::vector<std::string> labels;
        std::vector<std::string> values;
        for (int row = 0; row < ITEM_LIST_ROWS; row++){
            const std::string item = read_item_list_name(logger, language, images[0], row);
            labels.push_back(std::format("row {}", row + 1));
            values.push_back(item.empty() ? "(nothing)" : item);
            if (item.empty() || item == "cancel"){
                break;
            }
        }
        return check_against_golden_file(UNIT_TEST_RESOURCE_PATH() + image_paths()[0], labels, values);
    }
};

} // namespace

void add_tests_ItemListReader(UnitTestDatabase& database){
    //  FireRed on console B, 2026-11-02, after the Viridian Mart: the 5 and
    //  the 3 read as nothing before the digit templates.
    database.add<Test_BagPocket>("PokemonFRLG/ItemListReader/ItemsPocket_eng.png");
    //  Console B, 2026-11-03, the save before Cerulean's rival: a MOON STONE,
    //  which the reader has no name for, above the PARLYZ HEAL and 15 POTIONs.
    //  Stopping at it read nothing, and the rival was fought with 0 Potions.
    database.add<Test_BagPocket>("PokemonFRLG/ItemListReader/ItemsPocketMoonStone_eng.png");
    //  The Viridian Mart's BUY list, the same day, the cursor on POTION.
    database.add<Test_MartList>("PokemonFRLG/ItemListReader/MartBuyList_eng.png");
    database.add<Test_Route17ItemNames>();
    database.add<Test_StoryItemNames>();
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
