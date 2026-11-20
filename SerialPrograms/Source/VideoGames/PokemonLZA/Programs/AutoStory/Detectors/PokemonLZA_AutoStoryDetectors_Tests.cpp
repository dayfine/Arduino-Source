/*  PokemonLZA AutoStory - Detectors Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Synthetic frames, and console frames (1920x1080 JPEG or PNG) from
 *  CommandLineTests/PokemonLZA/AutoStoryDetectors/, one folder per scene
 *  sequence.
 *
 */


#include <charconv>
#include <format>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_PartyBarDetector.h"
#include "PokemonLZA_AutoStoryDialogDetectors.h"
#include "PokemonLZA_AutoStoryAutosaveDetector.h"
#include "PokemonLZA_AutoStoryControlsHintDetector.h"
#include "PokemonLZA_AutoStoryDetectors_Tests.h"
#include "PokemonLZA_AutoStoryMissionBannerDetectors.h"
#include "PokemonLZA_AutoStoryTutorialCardDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

constexpr size_t WIDTH = 1920;
constexpr size_t HEIGHT = 1080;


//  Per detector: the expected read, or none when not checked.
struct DetectorReads{
    std::optional<bool> autosave = {};
    std::optional<bool> controls_hint = {};
    std::optional<bool> advance_arrow = {};
    std::optional<bool> mission_banner = {};
    std::optional<bool> mission_complete = {};
    std::optional<bool> party_bar = {};
};

struct DetectorField{
    std::string_view name;
    std::optional<bool> DetectorReads::* read;
};
const DetectorField DETECTOR_FIELDS[] = {
    {"autosave", &DetectorReads::autosave},
    {"controls hint", &DetectorReads::controls_hint},
    {"advance arrow", &DetectorReads::advance_arrow},
    {"mission banner", &DetectorReads::mission_banner},
    {"mission complete", &DetectorReads::mission_complete},
    {"party bar", &DetectorReads::party_bar},
};

//  `reads` with every detector checked: one it leaves unset must not fire.
DetectorReads every_detector(DetectorReads reads){
    for (const DetectorField& field : DETECTOR_FIELDS){
        reads.*field.read = (reads.*field.read).value_or(false);
    }
    return reads;
}

std::string firing(const DetectorReads& got){
    std::string text;
    for (const DetectorField& field : DETECTOR_FIELDS){
        if ((got.*field.read).value_or(false)){
            text += std::format(", {}", field.name);
        }
    }
    return text;
}

std::string mismatches(const DetectorReads& got, const DetectorReads& want){
    std::string text;
    for (const DetectorField& field : DETECTOR_FIELDS){
        const std::optional<bool>& expected = want.*field.read;
        if (expected && got.*field.read != expected){
            text += std::format(" {} (expected {})", field.name, *expected ? "yes" : "no");
        }
    }
    return text;
}

struct StoryDetectors{
    AutosaveDetector autosave;
    ControlsHintDetector hint;
    AdvanceArrowDetector arrow;
    MainMissionBannerDetector banner;
    MissionCompleteDetector complete;
    PartyBarDetector party_bar;

    DetectorReads read(const ImageViewRGB32& frame){
        return DetectorReads{
            .autosave = autosave.detect(frame),
            .controls_hint = hint.detect(frame), .advance_arrow = arrow.detect(frame),
            .mission_banner = banner.detect(frame), .mission_complete = complete.detect(frame),
            .party_bar = party_bar.detect(frame),
        };
    }
};


void fill_box(size_t min_x, size_t min_y, size_t max_x, size_t max_y, uint32_t color, ImageRGB32& image){
    for (size_t y = min_y; y < max_y; y++){
        for (size_t x = min_x; x < max_x; x++){
            image.pixel(x, y) = color;
        }
    }
}

//  The pencil 48 px right of the Chinese badge's (the shorter Japanese text):
//  a white 4 px stroke in a `glow` band 12 px wide.
ImageRGB32 autosave_badge_frame(uint32_t background, uint32_t glow){
    ImageRGB32 image(WIDTH, HEIGHT);
    image.fill(background);
    fill_box(/*min_x=*/1655, /*min_y=*/73, /*max_x=*/1667, /*max_y=*/104, glow, image);
    fill_box(/*min_x=*/1659, /*min_y=*/73, /*max_x=*/1663, /*max_y=*/104, /*color=*/0xffffffff, image);
    return image;
}

//  A bright glow over a dark scene, with 4 px bands of banner mint green
//  (about 96, 203, 147) above and below the icon, as on 01.01 in English.
ImageRGB32 autosave_badge_by_mint_banner_frame(){
    ImageRGB32 image = autosave_badge_frame(/*background=*/0xff101418, /*glow=*/0xff28c8e6);
    fill_box(/*min_x=*/1600, /*min_y=*/60, /*max_x=*/1720, /*max_y=*/64, /*color=*/0xff60cb93, image);
    fill_box(/*min_x=*/1600, /*min_y=*/112, /*max_x=*/1720, /*max_y=*/116, /*color=*/0xff60cb93, image);
    return image;
}

struct SyntheticFrameCase{
    std::string name;
    struct Input{
        std::function<ImageRGB32()> frame;
    };
    Input input;
    struct Expectation{
        DetectorReads reads = {};
    };
    Expectation expectation;
};

const SyntheticFrameCase SYNTHETIC_FRAME_CASES[] = {
    {
        .name = "an empty image",
        .input = {.frame = []{ return ImageRGB32(); }},
        .expectation = {.reads = every_detector({})},
    },
    //  The badge's glow is dim over a lamp-lit wall (about 28, 70, 85: 02.04, frame 0075).
    {
        .name = "the autosave badge with a dim glow over a dark wall",
        .input = {.frame = []{ return autosave_badge_frame(0xff252722, 0xff1c4655); }},
        .expectation = {.reads = {.autosave = true}},
    },
    {
        .name = "the autosave badge's white stroke with no glow",
        .input = {.frame = []{ return autosave_badge_frame(0xff252722, 0xff252722); }},
        .expectation = {.reads = {.autosave = false}},
    },
    //  Mint and sign green have green over blue; the badge's glow does not.
    {
        .name = "the autosave badge with mint banner bands above and below",
        .input = {.frame = autosave_badge_by_mint_banner_frame},
        .expectation = {.reads = {.autosave = true}},
    },
    {
        .name = "a white stroke with a green sign's glow",
        .input = {.frame = []{ return autosave_badge_frame(0xff252722, 0xff3cf06e); }},
        .expectation = {.reads = {.autosave = false}},
    },
};

UnitTestResult check_synthetic_frame(const SyntheticFrameCase& test, Logger& logger){
    StoryDetectors detectors;
    const DetectorReads got = detectors.read(test.input.frame());
    logger.log(std::format("{} reads{}", test.name, firing(got)));
    const std::string wrong = mismatches(got, test.expectation.reads);
    if (!wrong.empty()){
        return std::format("Wrong on {}:{}", test.name, wrong);
    }
    return true;
}


std::optional<int> frame_number(std::string_view name, size_t offset, size_t digits){
    if (name.size() < offset + digits){
        return std::nullopt;
    }
    const std::string_view text = name.substr(offset, digits);
    int number = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), number);
    if (error != std::errc() || end != text.data() + text.size()){
        return std::nullopt;
    }
    return number;
}

//  Reads from the file name ("0001-nothing+platform+controls_hint.jpg");
//  detectors not in this module are ignored.
DetectorReads as_named(std::string_view name){
    const auto has = [name](std::string_view reading){
        return name.find(reading) != std::string_view::npos;
    };
    return DetectorReads{
        .autosave = has("+autosave"),
        .controls_hint = has("+controls_hint"), .advance_arrow = has("+advance_arrow"),
        .mission_banner = has("+mission_banner"), .mission_complete = has("+mission_complete"),
    };
}

//  The opening cutscene up to the platform: no detector fires.
std::optional<DetectorReads> opening(std::string_view){
    return DetectorReads{};
}

//  As named, but no autosave badge and the advance chevron only on 0040.
std::optional<DetectorReads> arrival_to_ad(std::string_view name){
    const std::optional<int> index = frame_number(name, /*offset=*/0, /*digits=*/4);
    if (!index){
        return std::nullopt;
    }
    return DetectorReads{
        .controls_hint = *index <= 6, .advance_arrow = *index == 40,
    };
}

//  As arrival_to_ad(), plus the chevron on 0044, 0049, 0053, 0078 and 0093-0115.
std::optional<DetectorReads> arrival_to_ad_subtitles(std::string_view name){
    const std::optional<int> index = frame_number(name, /*offset=*/0, /*digits=*/4);
    if (!index){
        return std::nullopt;
    }
    const bool box_arrow = *index == 44 || *index == 49 || *index == 53 || *index == 78;
    return DetectorReads{
        .controls_hint = *index <= 6,
        .advance_arrow = box_arrow || (*index >= 93 && *index <= 115),
    };
}

//  As named; the mission banner on 0181 only.
std::optional<DetectorReads> arrival_to_mission_banner(std::string_view name){
    const std::optional<int> index = frame_number(name, /*offset=*/0, /*digits=*/4);
    if (!index){
        return std::nullopt;
    }
    return DetectorReads{
        .controls_hint = name.find("+controls_hint") != std::string_view::npos,
        .advance_arrow = name.find("+advance_arrow") != std::string_view::npos,
        .mission_banner = *index == 181,
    };
}

//  As named, plus the autosave badge on 0197 (named "nothing").
std::optional<DetectorReads> arrival_past_mission_banner(std::string_view name){
    DetectorReads want = as_named(name);
    if (name.starts_with("0197-")){
        want.autosave = true;
    }
    return want;
}

//  As named, plus the complete card on 0045-0046 (named "nothing").
std::optional<DetectorReads> mission_complete_to_next_banner(std::string_view name){
    DetectorReads want = as_named(name);
    want.mission_complete = name.starts_with("0045-") || name.starts_with("0046-");
    return want;
}

struct FrameFolderCase{
    std::string name;
    struct Input{
        std::string_view folder;    //  In CommandLineTests/PokemonLZA/AutoStoryDetectors/.
    };
    Input input;
    struct Expectation{
        size_t frames = 0;
        //  Reads by file name; none for a file not in the set.
        std::function<std::optional<DetectorReads>(std::string_view name)> reads = {};
    };
    Expectation expectation;
};

const FrameFolderCase FRAME_FOLDER_CASES[] = {
    {
        .name = "the opening cutscene",
        .input = {.folder = "Opening"},
        .expectation = {.frames = 56, .reads = opening},
    },
    {
        .name = "the arrival to the ad",
        .input = {.folder = "ArrivalToAd"},
        .expectation = {.frames = 89, .reads = arrival_to_ad},
    },
    {
        .name = "the arrival to the ad's subtitles",
        .input = {.folder = "ArrivalToAdSubtitles"},
        .expectation = {.frames = 115, .reads = arrival_to_ad_subtitles},
    },
    {
        .name = "the arrival to the mission banner",
        .input = {.folder = "ArrivalToMissionBanner"},
        .expectation = {.frames = 181, .reads = arrival_to_mission_banner},
    },
    {
        .name = "the arrival past the mission banner",
        .input = {.folder = "ArrivalPastMissionBanner"},
        .expectation = {.frames = 221, .reads = arrival_past_mission_banner},
    },
    {
        .name = "the mission complete card to the next mission's banner",
        .input = {.folder = "MissionCompleteToNextBanner"},
        .expectation = {.frames = 139, .reads = mission_complete_to_next_banner},
    },
    {
        //  console B, 2026-11-15, 1280x720: Italian's longer header moves
        //  the banner's icon 96 px left; the three-row controls hint puts
        //  its card ~20 px higher.
        .name = "01.01's banner and controls hint in English and Italian",
        .input = {.folder = "Arrival0101Languages"},
        .expectation = {.frames = 7, .reads = as_named},
    },
};

UnitTestResult check_frame_folder(const FrameFolderCase& test, Logger& logger){
    const Filesystem::Path folder(std::format("{}PokemonLZA/AutoStoryDetectors/{}/", UNIT_TEST_RESOURCE_PATH(), test.input.folder));
    if (!Filesystem::is_directory(folder)){
        return std::format("{} is not a folder.", folder.string());
    }
    StoryDetectors detectors;
    size_t frames = 0;
    std::string failures;
    for (const Filesystem::Path& path : Filesystem::list_directory(folder)){
        const std::string name = path.filename().string();
        if (!name.ends_with(".jpg") && !name.ends_with(".png")){
            continue;
        }
        const std::optional<DetectorReads> want = test.expectation.reads(name);
        if (!want){
            continue;
        }
        const DetectorReads got = detectors.read(ImageRGB32(path.string()));
        logger.log(std::format("{} reads{}", name, firing(got)));
        const std::string wrong = mismatches(got, every_detector(*want));
        if (!wrong.empty()){
            failures += std::format(" {}:{};", name, wrong);
        }
        frames++;
    }
    if (frames != test.expectation.frames){
        return std::format("Expected {} frames in {}, found {}.", test.expectation.frames, folder.string(), frames);
    }
    if (!failures.empty()){
        return std::format("Wrong on:{}", failures);
    }
    return true;
}


struct TutorialCardFrameCase{
    std::string name;
    struct Input{
        std::string_view path;  //  In CommandLineTests/PokemonLZA/.
    };
    Input input;
    struct Expectation{
        bool card = false;
    };
    Expectation expectation;
};

const TutorialCardFrameCase TUTORIAL_CARD_FRAME_CASES[] = {
    {
        //  console B, 2026-11-16 (03.02b run 5): the "Map & Missions" card,
        //  page 1 of 2, over the map app's blue screen, not a dark one.
        .name = "the Map & Missions card over the map app (Italian)",
        .input = {.path = "AutoStoryScreenEvents/it_0302b_map_missions_card.png"},
        .expectation = {.card = true},
    },
    {
        //  console B, 2026-11-16: a save file's first X opens the menu under
        //  the "Salvare il gioco" card (save by R in the main menu); A
        //  closes it.
        .name = "the Save card over the main menu (Italian)",
        .input = {.path = "AutoStoryScreenEvents/it_menu_save_tutorial_card.png"},
        .expectation = {.card = true},
    },
    {
        .name = "no card on the main menu",
        .input = {.path = "MainMenuDetector/it_menu_after_save_tutorial_True.png"},
        .expectation = {.card = false},
    },
};

UnitTestResult check_tutorial_card_frame(const TutorialCardFrameCase& test){
    const std::string path = std::format("{}PokemonLZA/{}", UNIT_TEST_RESOURCE_PATH(), test.input.path);
    const bool card = tutorial_card_shows(ImageRGB32(path));
    if (card != test.expectation.card){
        return std::format("{}: expected card {}, got {}", test.input.path, test.expectation.card ? "yes" : "no", card ? "yes" : "no");
    }
    return true;
}


class Test_SyntheticFrame : public UnitTest{
public:
    explicit Test_SyntheticFrame(const SyntheticFrameCase& test)
        : UnitTest(std::format("PokemonLZA::AutoStoryDetectors - synthetic frame: {}", test.name))
        , m_test(test)
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        return check_synthetic_frame(m_test, logger);
    }

private:
    const SyntheticFrameCase& m_test;
};

class Test_TutorialCardFrame : public UnitTest{
public:
    explicit Test_TutorialCardFrame(const TutorialCardFrameCase& test)
        : UnitTest(std::format("PokemonLZA::AutoStoryDetectors - tutorial card frame: {}", test.name))
        , m_test(test)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return check_tutorial_card_frame(m_test);
    }

private:
    const TutorialCardFrameCase& m_test;
};

class Test_FrameFolder : public UnitTest{
public:
    explicit Test_FrameFolder(const FrameFolderCase& test)
        : UnitTest(std::format("PokemonLZA::AutoStoryDetectors - console frames: {}", test.name))
        , m_test(test)
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        return check_frame_folder(m_test, logger);
    }

private:
    const FrameFolderCase& m_test;
};

} // namespace


void add_tests_AutoStoryDetectors(UnitTestDatabase& database){
    for (const SyntheticFrameCase& test : SYNTHETIC_FRAME_CASES){
        database.add<Test_SyntheticFrame>(test);
    }
    for (const FrameFolderCase& test : FRAME_FOLDER_CASES){
        database.add<Test_FrameFolder>(test);
    }
    for (const TutorialCardFrameCase& test : TUTORIAL_CARD_FRAME_CASES){
        database.add<Test_TutorialCardFrame>(test);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
