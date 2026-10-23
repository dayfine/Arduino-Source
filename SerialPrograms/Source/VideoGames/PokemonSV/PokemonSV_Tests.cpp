/*  Pokemon SV Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Inference/PokemonSV_WhiteButtonDetector.h"
#include "Inference/Boxes/PokemonSV_BoxDetection.h"
#include "Inference/Boxes/PokemonSV_BoxEggDetector.h"
#include "Inference/Map/PokemonSV_MapDetector.h"
#include "Inference/Overworld/PokemonSV_OverworldDetector.h"
#include "Inference/Picnics/PokemonSV_PicnicDetector.h"
#include "Inference/Battles/PokemonSV_NormalBattleMenus.h"
#include "Inference/Tera/PokemonSV_TeraCardDetector.h"
#include "Inference/Tera/PokemonSV_TeraSilhouetteReader.h"
#include "Inference/Tera/PokemonSV_TeraTypeReader.h"
#include "Inference/Dialogs/PokemonSV_DialogDetector.h"
#include "Inference/Map/PokemonSV_FastTravelDetector.h"
#include "Inference/Map/PokemonSV_MapMenuDetector.h"
#include "Inference/Map/PokemonSV_MapPokeCenterIconDetector.h"
#include "Inference/PokemonSV_ESPEmotionDetector.h"
#include "Inference/Picnics/PokemonSV_SandwichIngredientDetector.h"
#include "Inference/Picnics/PokemonSV_SandwichPlateDetector.h"
#include "Inference/Overworld/PokemonSV_LetsGoKillDetector.h"
#include "Inference/Picnics/PokemonSV_SandwichRecipeDetector.h"
#include "Inference/Picnics/PokemonSV_SandwichHandDetector.h"
#include "Programs/PokemonSV_TerariumPlazaReturn.h"
#include "PokemonSV_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{



namespace{

std::string join_bools(const std::vector<bool>& items){
    std::string ret;
    for (bool item : items){
        if (!ret.empty()){
            ret += ", ";
        }
        ret += item ? "true" : "false";
    }
    return "{" + ret + "}";
}


struct PlazaBannerToCheckCase{
    std::string name;
    Language language;
    bool ocr_available;
    std::string expected;
};

class Test_BBQPlazaBannerToCheck : public UnitTest{
public:
    Test_BBQPlazaBannerToCheck(PlazaBannerToCheckCase test_case)
        : UnitTest("PokemonSV::BBQPlazaBannerToCheck - " + test_case.name)
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::string banner = central_plaza_banner_to_check(m_case.language, m_case.ocr_available);
        if (banner != m_case.expected){
            return "Expected \"" + m_case.expected + "\", got \"" + banner + "\"";
        }
        return true;
    }

private:
    const PlazaBannerToCheckCase m_case;
};

void add_tests_BBQPlazaBannerToCheck(UnitTestDatabase& database){
    const PlazaBannerToCheckCase cases[] = {
        {.name = "english",                     .language = Language::English,              .ocr_available = true,  .expected = "Central Plaza"},
        {.name = "chinese traditional",         .language = Language::ChineseTraditional,   .ocr_available = true,  .expected = "中心廣場"},
        {.name = "no OCR data",                 .language = Language::ChineseTraditional,   .ocr_available = false, .expected = ""},
        {.name = "name not known yet",          .language = Language::Japanese,             .ocr_available = true,  .expected = ""},
        {.name = "no language",                 .language = Language::None,                 .ocr_available = true,  .expected = ""},
    };
    for (const PlazaBannerToCheckCase& test_case : cases){
        database.add<Test_BBQPlazaBannerToCheck>(test_case);
    }
}


struct PlazaBannerMatchCase{
    std::string name;
    std::string expected;
    std::string ocr_text;
    bool matches;
};

class Test_BBQPlazaBannerMatch : public UnitTest{
public:
    Test_BBQPlazaBannerMatch(PlazaBannerMatchCase test_case)
        : UnitTest("PokemonSV::BBQPlazaBannerMatch - " + test_case.name)
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const bool matches = place_name_banner_matches(m_case.expected, m_case.ocr_text);
        if (matches != m_case.matches){
            return "Banner \"" + m_case.ocr_text + "\" against \"" + m_case.expected + "\": expected "
                + (m_case.matches ? "a match" : "no match") + ", got "
                + (matches ? "a match" : "no match");
        }
        return true;
    }

private:
    const PlazaBannerMatchCase m_case;
};

void add_tests_BBQPlazaBannerMatch(UnitTestDatabase& database){
    const PlazaBannerMatchCase cases[] = {
        {.name = "chinese as seen in the lab",          .expected = "中心廣場",        .ocr_text = "中心廣場",              .matches = true},
        {.name = "chinese with spaces",                 .expected = "中心廣場",        .ocr_text = "中 心\xE3\x80\x80廣 場",  .matches = true},
        {.name = "chinese with noise around it",        .expected = "中心廣場",        .ocr_text = "|一中心廣場。",          .matches = true},
        {.name = "chinese, one character misread",      .expected = "中心廣場",        .ocr_text = "中心廣塲",              .matches = true},
        {.name = "chinese, one character missing",      .expected = "中心廣場",        .ocr_text = "中心場",                .matches = true},
        {.name = "chinese, coastal plaza",              .expected = "中心廣場",        .ocr_text = "海岸廣場",              .matches = false},
        {.name = "chinese, savanna plaza",              .expected = "中心廣場",        .ocr_text = "草原廣場",              .matches = false},
        {.name = "chinese, canyon plaza",               .expected = "中心廣場",        .ocr_text = "峽谷廣場",              .matches = false},
        {.name = "chinese, polar plaza",                .expected = "中心廣場",        .ocr_text = "極地廣場",              .matches = false},
        {.name = "chinese, only half the name",         .expected = "中心廣場",        .ocr_text = "廣場",                  .matches = false},
        {.name = "english",                             .expected = "Central Plaza",   .ocr_text = "Central Plaza",         .matches = true},
        {.name = "english, case and punctuation",       .expected = "Central Plaza",   .ocr_text = " CENTRAL  plaza.",      .matches = true},
        {.name = "english, two characters misread",     .expected = "Central Plaza",   .ocr_text = "Centra1 P1aza",         .matches = true},
        {.name = "english, coastal plaza",              .expected = "Central Plaza",   .ocr_text = "Coastal Plaza",         .matches = false},
        {.name = "english, canyon plaza",               .expected = "Central Plaza",   .ocr_text = "Canyon Plaza",          .matches = false},
        {.name = "english, savanna plaza",              .expected = "Central Plaza",   .ocr_text = "Savanna Plaza",         .matches = false},
        {.name = "english, polar plaza",                .expected = "Central Plaza",   .ocr_text = "Polar Plaza",           .matches = false},
        {.name = "english, chargestone cavern",         .expected = "Central Plaza",   .ocr_text = "Chargestone Cavern",    .matches = false},
        {.name = "nothing read",                        .expected = "中心廣場",        .ocr_text = "",                      .matches = false},
        {.name = "only punctuation read",               .expected = "Central Plaza",   .ocr_text = " .|- ",                 .matches = false},
        {.name = "nothing expected",                    .expected = "",                .ocr_text = "Central Plaza",         .matches = false},
    };
    for (const PlazaBannerMatchCase& test_case : cases){
        database.add<Test_BBQPlazaBannerMatch>(test_case);
    }
}


// Plays back the banner texts read after each landing, in order. Each
// landing is a list of reads (frames x thresholds), handed to `matches` the
// way the console reads them.
class ScriptedPlazaReturnActions : public PlazaReturnActions{
public:
    ScriptedPlazaReturnActions(std::vector<std::vector<std::string>> landings)
        : m_landings(std::move(landings))
    {}

    virtual bool fly_to_plaza(bool overshoot) override{
        const size_t flight = overshoots.size();
        overshoots.push_back(overshoot);
        return flight >= fly_results.size() || fly_results[flight];
    }
    virtual std::string read_landing_banner(const std::function<bool(const std::string& text)>& matches) override{
        const size_t landing = banner_reads++;
        if (landing >= m_landings.size()){
            return "";
        }
        std::string last;
        for (const std::string& text : m_landings[landing]){
            if (matches(text)){
                return text;
            }
            if (!text.empty()){
                last = text;
            }
        }
        return last;
    }

public:
    // Whether each fly happened; flies past the end do.
    std::vector<bool> fly_results;
    std::vector<bool> overshoots;
    size_t banner_reads = 0;

private:
    const std::vector<std::vector<std::string>> m_landings;
};

class NullLogger : public Logger{
public:
    virtual void log(const std::string&, Color) override{}
};


struct PlazaReturnCase{
    std::string name;
    std::string expected_banner;
    std::vector<std::vector<std::string>> landings = {};
    std::vector<bool> fly_results = {};
    PlazaReturnResult result;
    std::vector<bool> overshoots;
    size_t banner_reads;
};

class Test_BBQPlazaReturn : public UnitTest{
public:
    Test_BBQPlazaReturn(PlazaReturnCase test_case)
        : UnitTest("PokemonSV::BBQPlazaReturn - " + test_case.name)
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        NullLogger logger;
        ScriptedPlazaReturnActions actions(m_case.landings);
        actions.fly_results = m_case.fly_results;
        const PlazaReturnResult result = return_to_plaza_checked(logger, actions, m_case.expected_banner, 3);
        if (result != m_case.result){
            return "Expected result " + plaza_return_result_to_string(m_case.result)
                + ", got " + plaza_return_result_to_string(result);
        }
        if (actions.overshoots != m_case.overshoots){
            return "Expected flights with overshoot " + join_bools(m_case.overshoots)
                + ", got " + join_bools(actions.overshoots);
        }
        if (actions.banner_reads != m_case.banner_reads){
            return "Expected " + std::to_string(m_case.banner_reads) + " banner reads, got "
                + std::to_string(actions.banner_reads);
        }
        return true;
    }

private:
    const PlazaReturnCase m_case;
};

void add_tests_BBQPlazaReturn(UnitTestDatabase& database){
    const PlazaReturnCase cases[] = {
        {
            .name = "arrives first time",
            .expected_banner = "中心廣場",
            .landings = {{"", "中心廣場"}},
            .result = PlazaReturnResult::ARRIVED,
            .overshoots = {false},
            .banner_reads = 1,
        },
        {
            // As in the lab on 2026-10-22, when the closest-Pokecenter
            // fallback flew to another fly point.
            .name = "lands elsewhere, then arrives with overshoot",
            .expected_banner = "中心廣場",
            .landings = {{"海岸廣場"}, {"中心廣場"}},
            .result = PlazaReturnResult::ARRIVED,
            .overshoots = {false, true},
            .banner_reads = 2,
        },
        {
            .name = "banner not seen, then arrives",
            .expected_banner = "Central Plaza",
            .landings = {{"", ""}, {"", ""}, {"Central Plaza"}},
            .result = PlazaReturnResult::ARRIVED,
            .overshoots = {false, true, true},
            .banner_reads = 3,
        },
        {
            .name = "never arrives",
            .expected_banner = "Central Plaza",
            .landings = {{"Coastal Plaza"}, {"Canyon Plaza"}, {""}},
            .result = PlazaReturnResult::FAILED,
            .overshoots = {false, true, true},
            .banner_reads = 3,
        },
        {
            .name = "nothing to check",
            .expected_banner = "",
            .landings = {{"Coastal Plaza"}},
            .result = PlazaReturnResult::NOT_CHECKED,
            .overshoots = {false},
            .banner_reads = 0,
        },
        {
            // No Pokecenter to fly to, or a battle on the way: no landing to
            // read, and the next attempt overshoots.
            .name = "a fly that didn't happen uses an attempt, then arrives",
            .expected_banner = "中心廣場",
            .landings = {{"中心廣場"}},
            .fly_results = {false},
            .result = PlazaReturnResult::ARRIVED,
            .overshoots = {false, true},
            .banner_reads = 1,
        },
        {
            // bbq-m5 11:20: the fly kept failing and the return looped for
            // minutes. Now it fails after 3 flies.
            .name = "flies that never happen fail the return",
            .expected_banner = "中心廣場",
            .fly_results = {false, false, false},
            .result = PlazaReturnResult::FAILED,
            .overshoots = {false, true, true},
            .banner_reads = 0,
        },
        {
            .name = "a failed fly and a wrong landing both count",
            .expected_banner = "Central Plaza",
            .landings = {{"Coastal Plaza"}},
            .fly_results = {false, true, false},
            .result = PlazaReturnResult::FAILED,
            .overshoots = {false, true, true},
            .banner_reads = 1,
        },
        {
            .name = "nothing to check retries a fly that didn't happen",
            .expected_banner = "",
            .fly_results = {false},
            .result = PlazaReturnResult::NOT_CHECKED,
            .overshoots = {false, true},
            .banner_reads = 0,
        },
    };
    for (const PlazaReturnCase& test_case : cases){
        database.add<Test_BBQPlazaReturn>(test_case);
    }
}

}



void add_tests(UnitTestDatabase& database){
    add_tests_WhiteButtonDetector(database);
    add_tests_BoxInfoDetector(database);
    add_tests_BoxEggDetector(database);
    add_tests_DialogDetector(database);
    add_tests_ESPEmotionDetector(database);
    add_tests_FastTravelDetector(database);
    add_tests_MapDetector(database);
    add_tests_MapMenuDetector(database);
    add_tests_MapPokeCenterIconDetector(database);
    add_tests_OverworldDetector(database);
    add_tests_PicnicDetector(database);
    add_tests_SandwichIngredientDetector(database);
    add_tests_SandwichPlateDetector(database);
    add_tests_SandwichRecipeDetector(database);
    add_tests_SandwichHandDetector(database);
    add_tests_TeraCardDetector(database);
    add_tests_TeraSilhouetteReader(database);
    add_tests_TeraTypeReader(database);
    add_tests_NormalBattleMenus(database);
    add_tests_LetsGoKillDetector(database);
    add_tests_BBQPlazaBannerToCheck(database);
    add_tests_BBQPlazaBannerMatch(database);
    add_tests_BBQPlazaReturn(database);
}



}
}
}
