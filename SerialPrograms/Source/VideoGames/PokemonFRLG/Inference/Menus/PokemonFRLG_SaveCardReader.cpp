/*  Save Card Reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <string_view>
#include <format>
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
            std::pair<std::string_view, std::string_view>{"pallet-town", "Pallet Town"},
            {"viridian-city", "Viridian City"},
            {"viridian-forest", "Viridian Forest"},
            {"pewter-city", "Pewter City"},
            {"route-1", "Route 1"},
            {"route-2", "Route 2"},
            {"route-3", "Route 3"},
            {"route-4", "Route 4"},
            {"cerulean-city", "Cerulean City"},
            {"route-24", "Route 24"},
            {"route-25", "Route 25"},
            {"route-10", "Route 10"},
            {"lavender-town", "Lavender Town"},
            {"vermilion-city", "Vermilion City"},
            {"celadon-city", "Celadon City"},
            {"rocket-hideout", "Rocket Hideout"},
            {"cinnabar-island", "Cinnabar Island"},
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

} // namespace


ImageFloatBox save_card_title_box(){
    return ImageFloatBox(8.0 / 240, 9.0 / 160, 114.0 / 240, 13.0 / 160);
}

std::string read_save_card_location(Logger& logger, Language language, const ImageViewRGB32& screen){
    const ImageViewRGB32 game = extract_box_reference(screen, GameSettings::instance().GAME_BOX);
    return PlaceNameMatcher::instance().read(logger, language, extract_box_reference(game, save_card_title_box()));
}


namespace{

//  A capture of the save card over "Would you like to save the game?",
//  named <anything>_<language code>_<place slug>, e.g.
//  Bedroom_eng_pallet-town.png.
class Test_SaveCard : public ImageUnitTest{
public:
    Test_SaveCard(const std::string& image)
        : ImageUnitTest(std::format("PokemonFRLG::SaveCardReader - {}", image), {image})
    {}

    virtual UnitTestResult run_on_images(
        Logger& logger, CancellableScope& scope,
        const std::vector<ImageRGB32>& images
    ) const override{
        const std::vector<std::string> words = parse_words(Filesystem::Path(image_paths()[0]).stem().string());
        if (words.size() < 3){
            return "Error: filename must be <anything>_<language code>_<place slug>.";
        }
        const std::string& language_word = words[words.size() - 2];
        const Language language = language_code_to_enum(language_word);
        if (language == Language::None || language == Language::EndOfList){
            return std::format("Error: invalid language word in filename: {}", language_word);
        }

        const std::string read = read_save_card_location(logger, language, images[0]);
        TEST_RESULT_COMPONENT_EQUAL_STR(read, words.back(), "place");
        return true;
    }
};

} // namespace

void add_tests_SaveCardReader(UnitTestDatabase& database){
    //  FireRed on console B, 2026-11-02. The card covers the same corner of
    //  the screen everywhere; the places behind it differ.
    database.add<Test_SaveCard>("PokemonFRLG/SaveCardReader/Bedroom_eng_pallet-town.png");
    database.add<Test_SaveCard>("PokemonFRLG/SaveCardReader/Outdoors_eng_pallet-town.png");
    database.add<Test_SaveCard>("PokemonFRLG/SaveCardReader/OaksLab_eng_pallet-town.png");
    database.add<Test_SaveCard>("PokemonFRLG/SaveCardReader/Forest_eng_viridian-forest.png");
    database.add<Test_SaveCard>("PokemonFRLG/SaveCardReader/PokemonCenter_eng_pewter-city.png");
    database.add<Test_SaveCard>("PokemonFRLG/SaveCardReader/OneBadge_eng_pewter-city.png");
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
