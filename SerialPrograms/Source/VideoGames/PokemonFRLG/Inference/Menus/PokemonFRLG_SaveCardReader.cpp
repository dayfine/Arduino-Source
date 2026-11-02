/*  Save Card Reader
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
#include "PokemonFRLG_SaveCardReader.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

//  The title's blue, about (30, 80, 225) on the capture.
const std::vector<OCR::TextColorRange>& BLUE_TEXT_FILTERS(){
    static const std::vector<OCR::TextColorRange> filters{
        {combine_rgb(0, 30, 150), combine_rgb(110, 150, 255)},
        {combine_rgb(0, 0, 120), combine_rgb(140, 180, 255)},
    };
    return filters;
}

class PlaceNameMatcher : public OCR::SmallDictionaryMatcher{
public:
    static constexpr double MAX_LOG10P = -1.40;
    static constexpr double MAX_LOG10P_SPREAD = 0.50;

    static const PlaceNameMatcher& instance(){
        static const PlaceNameMatcher matcher;
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
    PlaceNameMatcher()
        : SmallDictionaryMatcher(dictionary())
    {}

    static JsonObject dictionary(){
        JsonObject english;
        for (const auto& [slug, name] : {
            std::pair<const char*, const char*>{"pallet-town", "Pallet Town"},
            {"viridian-city", "Viridian City"},
            {"viridian-forest", "Viridian Forest"},
            {"pewter-city", "Pewter City"},
            {"route-1", "Route 1"},
            {"route-2", "Route 2"},
        }){
            JsonArray names;
            names.push_back(name);
            english[slug] = std::move(names);
        }
        JsonObject json;
        json["eng"] = std::move(english);
        return json;
    }
};

}


ImageFloatBox save_card_title_box(){
    return ImageFloatBox(8.0 / 240, 9.0 / 160, 114.0 / 240, 13.0 / 160);
}

std::string read_save_card_location(Logger& logger, Language language, const ImageViewRGB32& screen){
    const ImageViewRGB32 game = extract_box_reference(screen, GameSettings::instance().GAME_BOX);
    return PlaceNameMatcher::instance().read(logger, language, extract_box_reference(game, save_card_title_box()));
}



}
}
}
