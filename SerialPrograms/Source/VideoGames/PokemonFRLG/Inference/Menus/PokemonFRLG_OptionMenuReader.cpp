/*  Option Menu Reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "Common/Cpp/Color.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonTools/OCR/OCR_SmallDictionaryMatcher.h"
#include "Tests/ImageUnitTest.h"
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
    const char* slug;
    const char* english;
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
            names.push_back(value.english);
            english[value.slug] = std::move(names);
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
    OptionItem(const char* name, size_t row, const std::vector<OptionValue>& values, double max_log10p)
        : name(name)
        , row(row)
        , slugs(slugs_of(values))
        , matcher(values, max_log10p)
    {}

    const char* name;
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

}


const char* option_menu_name(OptionMenuItem item){
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

//  The values the menu shows for the options AutoStory sets.
struct OptionValues{
    std::string text_speed;
    std::string battle_scene;
    std::string battle_style;
};

std::string to_string(const OptionValues& values){
    return std::format(
        "TEXT SPEED \"{}\", BATTLE SCENE \"{}\", BATTLE STYLE \"{}\"",
        values.text_speed, values.battle_scene, values.battle_style
    );
}

//  A capture of the OPTION menu, read on each row AutoStory sets.
class Test_OptionMenu : public ImageUnitTest{
public:
    Test_OptionMenu(const std::string& image, OptionValues expected)
        : ImageUnitTest("PokemonFRLG::OptionMenuReader - " + image, {image})
        , m_expected(std::move(expected))
    {}

    virtual UnitTestResult run_on_images(
        Logger& logger, CancellableScope& scope,
        const std::vector<ImageRGB32>& images
    ) const override{
        const OptionValues read{
            .text_speed = read_option_value(logger, Language::English, images[0], OptionMenuItem::TEXT_SPEED),
            .battle_scene = read_option_value(logger, Language::English, images[0], OptionMenuItem::BATTLE_SCENE),
            .battle_style = read_option_value(logger, Language::English, images[0], OptionMenuItem::BATTLE_STYLE),
        };
        const std::string expected = to_string(m_expected);
        const std::string got = to_string(read);
        if (got != expected){
            return "Expected " + expected + ", read " + got;
        }
        return true;
    }

private:
    const OptionValues m_expected;
};

}

void add_tests_OptionMenuReader(UnitTestDatabase& database){
    //  Console B, 2026-11-02, FireRed: the menu as a new game opens it, the
    //  cursor on TEXT SPEED's white row.
    database.add<Test_OptionMenu>(
        "PokemonFRLG/OptionMenuReader/Options_Fast_On_Set.png",
        OptionValues{.text_speed = "fast", .battle_scene = "on", .battle_style = "set"}
    );
    //  The cursor moved down to BATTLE STYLE: TEXT SPEED's row is darkened.
    database.add<Test_OptionMenu>(
        "PokemonFRLG/OptionMenuReader/Options_Fast_On_Set_CursorOnBattleStyle.png",
        OptionValues{.text_speed = "fast", .battle_scene = "on", .battle_style = "set"}
    );
    //  After AutoStory set BATTLE SCENE to OFF.
    database.add<Test_OptionMenu>(
        "PokemonFRLG/OptionMenuReader/Options_Fast_Off_Set_CursorOnBattleStyle.png",
        OptionValues{.text_speed = "fast", .battle_scene = "off", .battle_style = "set"}
    );
}



}
}
}
