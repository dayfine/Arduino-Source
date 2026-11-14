/*  Blueberry Quest Detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"
#include "CommonTools/Images/ImageFilter.h"
#include "VideoGames/PokemonSV/Inference/PokemonSV_BlueberryQuestReader.h"
#include "Tests/ImageUnitTest.h"
#include "PokemonSV_BlueberryQuestDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{


//BlueberryQuestDetector::~BlueberryQuestDetector() = default;

BlueberryQuestDetector::BlueberryQuestDetector(Logger& logger, Color color, Language language, QuestPosition position)
    : m_logger(logger), m_color(color), m_language(language), m_position(position)
{
    switch(position){
    case QuestPosition::FIRST:
        m_box = ImageFloatBox(0.604, 0.218, 0.282, 0.095);
        break;
    case QuestPosition::SECOND:
        m_box = ImageFloatBox(0.604, 0.395, 0.282, 0.095);
        break;
    case QuestPosition::THIRD:
        m_box = ImageFloatBox(0.604, 0.572, 0.282, 0.095);
        break;
    case QuestPosition::FOURTH:
        m_box = ImageFloatBox(0.604, 0.749, 0.282, 0.095);
        break;
    }
}

void BlueberryQuestDetector::make_overlays(VideoOverlaySet& items) const{
    items.add(m_color, m_box);
}

bool BlueberryQuestDetector::detect(const ImageViewRGB32& screen){
    return !detect_quest(screen).empty();
}

std::string BlueberryQuestDetector::detect_quest(const ImageViewRGB32& screen) const{
    std::multimap<double, OCR::StringMatchData> results;

    ImageRGB32 quest_label = to_blackwhite_rgb32_range(
        extract_box_reference(screen, m_box),
        true,
        combine_rgb(198, 198, 198), combine_rgb(255, 255, 255)
    );

    //quest_label.save("quest_label.png");

    OCR::StringMatchResult ocr_result = PokemonSV::BlueberryQuestReader::instance().read_substring(
        m_logger, m_language, quest_label,
        OCR::BLACK_TEXT_FILTERS()
    );
    ocr_result.clear_beyond_log10p(BlueberryQuestReader::MAX_LOG10P);
    ocr_result.clear_beyond_spread(BlueberryQuestReader::MAX_LOG10P_SPREAD);
    if (!ocr_result.results.empty()){
        for (const auto& result : ocr_result.results){
            results.emplace(result.first, result.second);
        }
            
    }

    if (results.empty()){
        return "";
    }

    if (results.size() > 1){
        throw_and_log<OperationFailedException>(
            m_logger, ErrorReportMode::SEND_ERROR_REPORT,
            "BlueberryQuestDetector::detect_quest(): Unable to read selected item. Ambiguous or multiple results.\n" + language_warning(m_language)
        );
    }

    return results.begin()->second.token;
}



BlueberryQuestWatcher::~BlueberryQuestWatcher() = default;

BlueberryQuestWatcher::BlueberryQuestWatcher(Logger& logger, Color color, VideoOverlay& overlay, Language language, BlueberryQuestDetector::QuestPosition position)
    : VisualInferenceCallback("BlueberryQuestWatcher")
    , m_overlay(overlay)
    , m_detector(logger, color, language, position)
{}

void BlueberryQuestWatcher::make_overlays(VideoOverlaySet& items) const{
    m_detector.make_overlays(items);
}

bool BlueberryQuestWatcher::process_frame(const ImageViewRGB32& screen, WallClock timestamp){
    m_quest_name = m_detector.detect_quest(screen);
    if (m_quest_name.empty()){
        return false;
    }

    return true;
}


//  One quest slot on a full frame of the quest panel.
struct BlueberryQuestScenario{
    std::string image;
    BlueberryQuestDetector::QuestPosition position;
    std::string expected_slug;
    size_t width;
    size_t height;
};

class Test_BlueberryQuestDetector : public ImageUnitTest{
public:
    Test_BlueberryQuestDetector(BlueberryQuestScenario scenario)
        : ImageUnitTest(
            "PokemonSV::BlueberryQuestDetector - " + scenario.image
            + " - slot " + std::to_string((int)scenario.position + 1)
            + " - " + std::to_string(scenario.width),
            {scenario.image}
        )
        , m_scenario(std::move(scenario))
    {}

    virtual UnitTestResult run_on_images(
        Logger& logger, CancellableScope&,
        const std::vector<ImageRGB32>& images
    ) const override{
        ImageRGB32 image = images[0].scale_to(m_scenario.width, m_scenario.height);
        BlueberryQuestDetector detector(logger, COLOR_RED, Language::ChineseTraditional, m_scenario.position);
        const std::string actual = detector.detect_quest(image);
        if (actual != m_scenario.expected_slug){
            return "Expected quest \"" + m_scenario.expected_slug + "\", read \"" + actual + "\"";
        }
        return true;
    }

private:
    const BlueberryQuestScenario m_scenario;
};

//  Every slot of one Traditional Chinese quest panel frame, at 1080p and 720p.
static void add_tests_BlueberryQuestPanel(
    UnitTestDatabase& database,
    const std::string& image, const std::string (&slugs)[4]
){
    using enum BlueberryQuestDetector::QuestPosition;
    const BlueberryQuestDetector::QuestPosition positions[] = {FIRST, SECOND, THIRD, FOURTH};
    const std::pair<size_t, size_t> sizes[] = {{1920, 1080}, {1280, 720}};
    for (size_t slot = 0; slot < 4; slot++){
        for (const auto& [width, height] : sizes){
            database.add<Test_BlueberryQuestDetector>(BlueberryQuestScenario{
                .image = image,
                .position = positions[slot],
                .expected_slug = slugs[slot],
                .width = width,
                .height = height,
            });
        }
    }
}

void add_tests_BlueberryQuestDetector(UnitTestDatabase& database){
    //  Singleplayer without a red quest shows 3 quests, so the fourth slot reads
    //  as nothing. "catch-any" reads "捕捉1隻..." on screen where the dictionary
    //  has "捕捉一隻...".
    add_tests_BlueberryQuestPanel(database, "PokemonSV/BlueberryPoints/6750.png", {"catch-any", "sneak-up", "auto-10", ""});
    //  The first frame after rerolling the first quest.
    add_tests_BlueberryQuestPanel(database, "PokemonSV/BlueberryPoints/6740.png", {"photo-swim", "sneak-up", "auto-10", ""});
}




}
}
}
