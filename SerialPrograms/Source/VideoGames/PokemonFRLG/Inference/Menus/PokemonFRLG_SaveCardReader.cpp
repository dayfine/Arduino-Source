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
#include "Tests/ImageUnitTest.h"
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


namespace{

//  A capture of the save card over "Would you like to save the game?".
class Test_SaveCard : public ImageUnitTest{
public:
    Test_SaveCard(const std::string& image, std::string expected)
        : ImageUnitTest("PokemonFRLG::SaveCardReader - " + image, {image})
        , m_expected(std::move(expected))
    {}

    virtual UnitTestResult run_on_images(
        Logger& logger, CancellableScope& scope,
        const std::vector<ImageRGB32>& images
    ) const override{
        const std::string read = read_save_card_location(logger, Language::English, images[0]);
        if (read != m_expected){
            return "Expected \"" + m_expected + "\", read \"" + read + "\"";
        }
        return true;
    }

private:
    const std::string m_expected;
};

}

void add_tests_SaveCardReader(UnitTestDatabase& database){
    //  Console B, 2026-11-02, FireRed. The card covers the same corner of
    //  the screen everywhere; the places behind it differ.
    for (const auto& [image, place] : {
        std::pair<const char*, const char*>{"PalletTown_Bedroom.png", "pallet-town"},
        {"PalletTown_Outdoors.png", "pallet-town"},
        {"PalletTown_OaksLab.png", "pallet-town"},
        {"ViridianForest.png", "viridian-forest"},
        {"PewterCity_PokemonCenter.png", "pewter-city"},
        {"PewterCity_OneBadge.png", "pewter-city"},
    }){
        database.add<Test_SaveCard>(std::string("PokemonFRLG/SaveCardReader/") + image, place);
    }
}



}
}
}
