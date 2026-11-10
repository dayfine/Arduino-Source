/*  Option Menu Reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Common/Cpp/Color.h"
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonTools/OCR/OCR_SmallDictionaryMatcher.h"
#include "Tests/ImageUnitTest.h"
#include "Tests/TestUtils.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_OcrPreprocessing.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Settings.h"
#include "PokemonFRLG_OptionMenuReader.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

//  option_menu.c: the window's top-left at pixel (16, 56), values from its
//  x 130 and y 2 + 13 * row, cleared 70 wide before each is drawn.
constexpr double VALUE_LEFT = 16 + 130 - 2;
constexpr double VALUE_WIDTH = 70 + 4;
constexpr double ROW_TOP = 56 + 2;
constexpr double ROW_STEP = 13;
constexpr double ROW_HEIGHT = 13;

//  Peach letters with red edges, on white or the darkened rows' light gray:
//  red stays high while green and blue stay below the background's. The
//  item names to the left are dark gray, out of the box.
const std::vector<OCR::TextColorRange>& VALUE_TEXT_FILTERS(){
    static const std::vector<OCR::TextColorRange> filters{
        {combine_rgb(150, 0, 0), combine_rgb(255, 200, 160)},
        {combine_rgb(150, 0, 0), combine_rgb(255, 210, 180)},
        {combine_rgb(170, 0, 0), combine_rgb(255, 190, 140)},
    };
    return filters;
}

struct OptionValue{
    std::string_view slug;
    std::string_view english;
};

//  One matcher per item, so a value is only matched against its own row's.
class OptionValueMatcher : public OCR::SmallDictionaryMatcher{
public:
    static constexpr double MAX_LOG10P_SPREAD = 0.50;

    OptionValueMatcher(const std::vector<OptionValue>& values, double max_log10p)
        : SmallDictionaryMatcher(dictionary(values))
        , m_max_log10p(max_log10p)
    {}

    std::string read(Logger& logger, Language language, const ImageViewRGB32& image) const{
        if (!languages()[language]){
            return "";
        }
        const ImageRGB32 ready = preprocess_for_ocr(image);
        const OCR::StringMatchResult result = match_substring_from_image_multifiltered(
            &logger, language, ready, VALUE_TEXT_FILTERS(),
            m_max_log10p, MAX_LOG10P_SPREAD, 0.01, 0.50
        );
        return result.results.empty() ? "" : result.results.begin()->second.token;
    }

private:
    static JsonObject dictionary(const std::vector<OptionValue>& values){
        JsonObject english;
        for (const OptionValue& value : values){
            JsonArray names;
            names.push_back(std::string(value.english));
            english[std::string(value.slug)] = std::move(names);
        }
        JsonObject json;
        json["eng"] = std::move(english);
        return json;
    }

    double m_max_log10p;
};

std::vector<std::string> slugs_of(const std::vector<OptionValue>& values){
    std::vector<std::string> ret;
    for (const OptionValue& value : values){
        ret.emplace_back(value.slug);
    }
    return ret;
}

//  An item's name, its row (option_menu.c MENUITEM_*), its values and their
//  matcher.
struct OptionItem{
    OptionItem(std::string_view name, size_t row, const std::vector<OptionValue>& values, double max_log10p)
        : name(name)
        , row(row)
        , slugs(slugs_of(values))
        , matcher(values, max_log10p)
    {}

    std::string_view name;
    size_t row;
    std::vector<std::string> slugs;
    OptionValueMatcher matcher;
};

const OptionItem& option_item(OptionMenuItem item){
    static const OptionItem text_speed("TEXT SPEED", 0, {{"slow", "Slow"}, {"mid", "Mid"}, {"fast", "Fast"}}, -1.40);
    //  Two letters score at most log10(1/25): an exact "ON" (console B,
    //  2026-11-02) is -1.398, short of the others' -1.40.
    static const OptionItem battle_scene("BATTLE SCENE", 1, {{"on", "On"}, {"off", "Off"}}, -1.30);
    static const OptionItem battle_style("BATTLE STYLE", 2, {{"shift", "Shift"}, {"set", "Set"}}, -1.40);
    switch (item){
    case OptionMenuItem::TEXT_SPEED:
        return text_speed;
    case OptionMenuItem::BATTLE_SCENE:
        return battle_scene;
    case OptionMenuItem::BATTLE_STYLE:
        return battle_style;
    }
    return text_speed;
}

} // namespace


std::string_view option_menu_name(OptionMenuItem item){
    return option_item(item).name;
}

size_t option_menu_row(OptionMenuItem item){
    return option_item(item).row;
}

const std::vector<std::string>& option_menu_values(OptionMenuItem item){
    return option_item(item).slugs;
}

ImageFloatBox option_value_box(size_t row){
    return ImageFloatBox(VALUE_LEFT / 240, (ROW_TOP + ROW_STEP * row) / 160, VALUE_WIDTH / 240, ROW_HEIGHT / 160);
}

std::string read_option_value(Logger& logger, Language language, const ImageViewRGB32& screen, OptionMenuItem item){
    const ImageViewRGB32 game = extract_box_reference(screen, GameSettings::instance().GAME_BOX);
    const ImageViewRGB32 value = extract_box_reference(game, option_value_box(option_menu_row(item)));
    return option_item(item).matcher.read(logger, language, value);
}


namespace{

//  A capture of the OPTION menu, named for what it shows:
//  <anything>_<language code>_<TEXT SPEED>_<BATTLE SCENE>_<BATTLE STYLE>,
//  e.g. CursorTop_eng_fast_on_set.png.
class Test_OptionMenu : public ImageUnitTest{
public:
    Test_OptionMenu(const std::string& image)
        : ImageUnitTest("PokemonFRLG::OptionMenuReader - " + image, {image})
    {}

    virtual UnitTestResult run_on_images(
        Logger& logger, CancellableScope& scope,
        const std::vector<ImageRGB32>& images
    ) const override{
        const std::vector<std::string> words = parse_words(Filesystem::Path(image_paths()[0]).stem().string());
        if (words.size() < 5){
            return "Error: filename must be <anything>_<language code>_<text speed>_<battle scene>_<battle style>.";
        }
        const std::string& language_word = words[words.size() - 4];
        const Language language = language_code_to_enum(language_word);
        if (language == Language::None || language == Language::EndOfList){
            return "Error: invalid language word in filename: " + language_word;
        }

        const OptionMenuItem items[] = {OptionMenuItem::TEXT_SPEED, OptionMenuItem::BATTLE_SCENE, OptionMenuItem::BATTLE_STYLE};
        for (size_t c = 0; c < 3; c++){
            const std::string read = read_option_value(logger, language, images[0], items[c]);
            TEST_RESULT_COMPONENT_EQUAL_STR(read, words[words.size() - 3 + c], option_menu_name(items[c]));
        }
        return true;
    }
};

} // namespace

void add_tests_OptionMenuReader(UnitTestDatabase& database){
    //  FireRed on console B, 2026-11-02: the menu as a new game opens it,
    //  then with the cursor moved to BATTLE STYLE (the other rows darkened),
    //  before and after AutoStory set BATTLE SCENE to OFF.
    database.add<Test_OptionMenu>("PokemonFRLG/OptionMenuReader/CursorTop_eng_fast_on_set.png");
    database.add<Test_OptionMenu>("PokemonFRLG/OptionMenuReader/CursorBattleStyle_eng_fast_on_set.png");
    database.add<Test_OptionMenu>("PokemonFRLG/OptionMenuReader/CursorBattleStyle_eng_fast_off_set.png");
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
