/*  Item List Reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include <string_view>
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
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

class ItemNameMatcher : public OCR::SmallDictionaryMatcher{
public:
    static constexpr double MAX_LOG10P = -1.40;
    static constexpr double MAX_LOG10P_SPREAD = 0.50;

    static const ItemNameMatcher& instance(){
        static const ItemNameMatcher matcher;
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
    ItemNameMatcher()
        : SmallDictionaryMatcher(dictionary())
    {}

    static JsonObject dictionary(){
        JsonObject english;
        for (const auto& [slug, name] : {
            std::pair<std::string_view, std::string_view>{"potion", "Potion"},
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
            {"cancel", "Cancel"},
        }){
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

BagRow read_bag_row(Logger& logger, Language language, const ImageViewRGB32& screen, int shown){
    const std::string item = read_item_list_name(logger, language, screen, shown);
    if (item == "cancel"){
        return BagRow{item, 0};
    }
    const ImageViewRGB32 count = extract_box_reference(game_box(screen), bag_item_count_box(shown));
    //  Not Tesseract on each waterfilled digit: the font joins some strokes
    //  only at a corner, so the 5 and the 3 fell apart into pieces and read
    //  as nothing (console B, 2026-11-02). The stat boxes' templates are
    //  the same digits on the same cream.
    return BagRow{item, read_digits_waterfill_template(logger, count, DigitTemplateType::StatBox)};
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
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
