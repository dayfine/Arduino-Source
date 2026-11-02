/*  Option Menu Reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Common/Cpp/Color.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonTools/OCR/OCR_SmallDictionaryMatcher.h"
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
    static constexpr double MAX_LOG10P = -1.40;
    static constexpr double MAX_LOG10P_SPREAD = 0.50;

    explicit OptionValueMatcher(const std::vector<OptionValue>& values)
        : SmallDictionaryMatcher(dictionary(values))
    {}

    std::string read(Logger& logger, Language language, const ImageViewRGB32& image) const{
        if (!languages()[language]){
            return "";
        }
        const ImageRGB32 ready = preprocess_for_ocr(image);
        const OCR::StringMatchResult result = match_substring_from_image_multifiltered(
            &logger, language, ready, VALUE_TEXT_FILTERS(),
            MAX_LOG10P, MAX_LOG10P_SPREAD, 0.01, 0.50
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
};

const std::vector<OptionValue>& text_speed_values(){
    static const std::vector<OptionValue> values{{"slow", "Slow"}, {"mid", "Mid"}, {"fast", "Fast"}};
    return values;
}
const std::vector<OptionValue>& battle_style_values(){
    static const std::vector<OptionValue> values{{"shift", "Shift"}, {"set", "Set"}};
    return values;
}
std::vector<std::string> slugs(const std::vector<OptionValue>& values){
    std::vector<std::string> ret;
    for (const OptionValue& value : values){
        ret.emplace_back(value.slug);
    }
    return ret;
}

const OptionValueMatcher& matcher(OptionMenuItem item){
    static const OptionValueMatcher text_speed(text_speed_values());
    static const OptionValueMatcher battle_style(battle_style_values());
    return item == OptionMenuItem::TEXT_SPEED ? text_speed : battle_style;
}

}


const char* option_menu_name(OptionMenuItem item){
    return item == OptionMenuItem::TEXT_SPEED ? "TEXT SPEED" : "BATTLE STYLE";
}

size_t option_menu_row(OptionMenuItem item){
    //  option_menu.c MENUITEM_TEXTSPEED, MENUITEM_BATTLESTYLE.
    return item == OptionMenuItem::TEXT_SPEED ? 0 : 2;
}

const std::vector<std::string>& option_menu_values(OptionMenuItem item){
    static const std::vector<std::string> text_speed = slugs(text_speed_values());
    static const std::vector<std::string> battle_style = slugs(battle_style_values());
    return item == OptionMenuItem::TEXT_SPEED ? text_speed : battle_style;
}

ImageFloatBox option_value_box(size_t row){
    return ImageFloatBox(VALUE_LEFT / 240, (ROW_TOP + ROW_STEP * row) / 160, VALUE_WIDTH / 240, ROW_HEIGHT / 160);
}

std::string read_option_value(Logger& logger, Language language, const ImageViewRGB32& screen, OptionMenuItem item){
    const ImageViewRGB32 game = extract_box_reference(screen, GameSettings::instance().GAME_BOX);
    const ImageViewRGB32 value = extract_box_reference(game, option_value_box(option_menu_row(item)));
    return matcher(item).read(logger, language, value);
}



}
}
}
