/*  Save Card Reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cctype>
#include <optional>
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

//  A place the save card can name: the pokefirered map name its maps start
//  with ("Route4" for "Route4" and "Route4_PokemonCenter_1F"), the slug
//  read_save_card_location() returns, and the card's text in English (the
//  region map section's name in pokefirered's region_map_sections.json).
struct SaveCardPlace{
    std::string_view map_prefix;
    std::string_view slug;
    std::string_view card_name;
};

//  Every Kanto place: the save card's names and the maps they are on come
//  from this one table, so a place the story saves in is one the reader
//  knows (19.04 saved in Victory Road, which the reader didn't have:
//  console B, 2026-11-15).
constexpr SaveCardPlace SAVE_CARD_PLACES[] = {
    {"PalletTown", "pallet-town", "Pallet Town"},
    {"ViridianCity", "viridian-city", "Viridian City"},
    {"PewterCity", "pewter-city", "Pewter City"},
    {"CeruleanCity", "cerulean-city", "Cerulean City"},
    {"LavenderTown", "lavender-town", "Lavender Town"},
    {"VermilionCity", "vermilion-city", "Vermilion City"},
    {"CeladonCity", "celadon-city", "Celadon City"},
    {"FuchsiaCity", "fuchsia-city", "Fuchsia City"},
    {"CinnabarIsland", "cinnabar-island", "Cinnabar Island"},
    {"IndigoPlateau", "indigo-plateau", "Indigo Plateau"},
    {"SaffronCity", "saffron-city", "Saffron City"},
    {"Route1", "route-1", "Route 1"},
    {"Route2", "route-2", "Route 2"},
    {"Route3", "route-3", "Route 3"},
    {"Route4", "route-4", "Route 4"},
    {"Route5", "route-5", "Route 5"},
    {"Route6", "route-6", "Route 6"},
    {"Route7", "route-7", "Route 7"},
    {"Route8", "route-8", "Route 8"},
    {"Route9", "route-9", "Route 9"},
    {"Route10", "route-10", "Route 10"},
    {"Route11", "route-11", "Route 11"},
    {"Route12", "route-12", "Route 12"},
    {"Route13", "route-13", "Route 13"},
    {"Route14", "route-14", "Route 14"},
    {"Route15", "route-15", "Route 15"},
    {"Route16", "route-16", "Route 16"},
    {"Route17", "route-17", "Route 17"},
    {"Route18", "route-18", "Route 18"},
    {"Route19", "route-19", "Route 19"},
    {"Route20", "route-20", "Route 20"},
    {"Route21", "route-21", "Route 21"},
    {"Route22", "route-22", "Route 22"},
    {"Route23", "route-23", "Route 23"},
    {"Route24", "route-24", "Route 24"},
    {"Route25", "route-25", "Route 25"},
    {"ViridianForest", "viridian-forest", "Viridian Forest"},
    {"MtMoon", "mt-moon", "Mt. Moon"},
    {"SSAnne", "ss-anne", "S.S. Anne"},
    {"UndergroundPath", "underground-path", "Underground Path"},
    {"DiglettsCave", "digletts-cave", "Diglett's Cave"},
    {"VictoryRoad", "victory-road", "Victory Road"},
    {"RocketHideout", "rocket-hideout", "Rocket Hideout"},
    {"SilphCo", "silph-co", "Silph Co."},
    {"PokemonMansion", "pokemon-mansion", "Pokemon Mansion"},
    {"SafariZone", "safari-zone", "Safari Zone"},
    {"PokemonLeague", "pokemon-league", "Pokemon League"},
    {"RockTunnel", "rock-tunnel", "Rock Tunnel"},
    {"SeafoamIslands", "seafoam-islands", "Seafoam Islands"},
    {"PokemonTower", "pokemon-tower", "Pokemon Tower"},
    {"CeruleanCave", "cerulean-cave", "Cerulean Cave"},
    {"PowerPlant", "power-plant", "Power Plant"},
};

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
        for (const SaveCardPlace& place : SAVE_CARD_PLACES){
            JsonArray names;
            names.push_back(std::string(place.card_name));
            english[std::string(place.slug)] = std::move(names);
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

std::optional<std::string> save_card_place(std::string_view map){
    for (const SaveCardPlace& place : SAVE_CARD_PLACES){
        if (map.starts_with(place.map_prefix)
            && (map.size() == place.map_prefix.size() || map[place.map_prefix.size()] == '_')
        ){
            return std::string(place.slug);
        }
    }
    return std::nullopt;
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

//  OCR text off the card, matched against the places' names.
struct SaveCardTextCase{
    std::string_view name;
    struct Input{
        std::string_view text;
    } input;
    struct Expectation{
        std::string_view slug;
    } expectation;
};

const SaveCardTextCase SAVE_CARD_TEXT_CASES[] = {
    //  19.04's card in Victory Road (console B, 2026-11-15).
    {.name = "a misread D", .input = {.text = "VICTORY ROAO"}, .expectation = {.slug = "victory-road"}},
    {.name = "a one-digit route among two-digit ones", .input = {.text = "ROUTE 1"}, .expectation = {.slug = "route-1"}},
    {.name = "a two-digit route", .input = {.text = "ROUTE 22"}, .expectation = {.slug = "route-22"}},
};

class Test_SaveCardText : public UnitTest{
public:
    Test_SaveCardText()
        : UnitTest("PokemonFRLG::SaveCardReader - text")
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        auto best = [](std::string_view text) -> std::string{
            const OCR::StringMatchResult result = PlaceNameMatcher::instance().match_substring(Language::English, std::string(text));
            return result.results.empty() ? "" : result.results.begin()->second.token;
        };
        for (const SaveCardTextCase& c : SAVE_CARD_TEXT_CASES){
            const std::string read = best(c.input.text);
            if (read != c.expectation.slug){
                return std::format("{}: \"{}\" read as \"{}\", expected {}.", c.name, c.input.text, read, c.expectation.slug);
            }
        }
        //  Every place a save can be in is one the card's text is read as.
        for (const SaveCardPlace& place : SAVE_CARD_PLACES){
            std::string upper(place.card_name);
            for (char& ch : upper){
                ch = (char)std::toupper((unsigned char)ch);
            }
            const std::string read = best(upper);
            if (read != place.slug){
                return std::format("\"{}\" read as \"{}\", expected {}.", upper, read, place.slug);
            }
        }
        return true;
    }
};

} // namespace

void add_tests_SaveCardReader(UnitTestDatabase& database){
    database.add<Test_SaveCardText>();
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
