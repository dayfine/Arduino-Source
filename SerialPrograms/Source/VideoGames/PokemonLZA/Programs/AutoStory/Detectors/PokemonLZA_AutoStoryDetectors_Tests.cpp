/*  PokemonLZA AutoStory - Detectors Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Synthetic frames, and console frames (1920x1080 JPEG) from
 *  CommandLineTests/PokemonLZA/AutoStoryDetectors/, one folder per scene
 *  sequence.
 *
 */


#include <charconv>
#include <cmath>
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
#include "PokemonLZA_AutoStoryAdvanceArrowDetector.h"
#include "PokemonLZA_AutoStoryAutosaveDetector.h"
#include "PokemonLZA_AutoStoryControlsHintDetector.h"
#include "PokemonLZA_AutoStoryDetectors_Tests.h"
#include "PokemonLZA_AutoStoryMainMissionBannerDetector.h"
#include "PokemonLZA_AutoStoryMissionCompleteDetector.h"
#include "PokemonLZA_AutoStoryPlatformArrivalDetector.h"
#include "PokemonLZA_AutoStorySkipPromptDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

constexpr size_t WIDTH = 1920;
constexpr size_t HEIGHT = 1080;


//  What each detector reads on a frame: true or false, or none when a test
//  does not check that detector.
struct DetectorReads{
    std::optional<bool> platform = {};
    std::optional<bool> skip_prompt = {};
    std::optional<bool> autosave = {};
    std::optional<bool> controls_hint = {};
    std::optional<bool> advance_arrow = {};
    std::optional<bool> mission_banner = {};
    std::optional<bool> mission_complete = {};
};

struct DetectorField{
    std::string_view name;
    std::optional<bool> DetectorReads::* read;
};
const DetectorField DETECTOR_FIELDS[] = {
    {"platform", &DetectorReads::platform},
    {"skip prompt", &DetectorReads::skip_prompt},
    {"autosave", &DetectorReads::autosave},
    {"controls hint", &DetectorReads::controls_hint},
    {"advance arrow", &DetectorReads::advance_arrow},
    {"mission banner", &DetectorReads::mission_banner},
    {"mission complete", &DetectorReads::mission_complete},
};

//  `reads` with every detector checked: one it leaves unset must not fire.
DetectorReads every_detector(DetectorReads reads){
    for (const DetectorField& field : DETECTOR_FIELDS){
        reads.*field.read = (reads.*field.read).value_or(false);
    }
    return reads;
}

//  The detectors that fire in `got`, for the log: ", platform, autosave".
std::string firing(const DetectorReads& got){
    std::string text;
    for (const DetectorField& field : DETECTOR_FIELDS){
        if ((got.*field.read).value_or(false)){
            text += std::format(", {}", field.name);
        }
    }
    return text;
}

//  The detectors `want` checks that `got` reads otherwise: " platform
//  (expected yes)"; empty when all agree.
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

//  Every detector of this module, one of each.
struct StoryDetectors{
    PlatformArrivalDetector platform;
    SkipPromptDetector skip;
    AutosaveDetector autosave;
    ControlsHintDetector hint;
    AdvanceArrowDetector arrow;
    MainMissionBannerDetector banner;
    MissionCompleteDetector complete;

    DetectorReads read(const ImageViewRGB32& frame){
        return DetectorReads{
            .platform = platform.detect(frame), .skip_prompt = skip.detect(frame),
            .autosave = autosave.detect(frame), .controls_hint = hint.detect(frame),
            .advance_arrow = arrow.detect(frame), .mission_banner = banner.detect(frame),
            .mission_complete = complete.detect(frame),
        };
    }
};


//  Synthetic frames.

void fill_box(size_t min_x, size_t min_y, size_t max_x, size_t max_y, uint32_t color, ImageRGB32& image){
    for (size_t y = min_y; y < max_y; y++){
        for (size_t x = min_x; x < max_x; x++){
            image.pixel(x, y) = color;
        }
    }
}

ImageRGB32 solid_frame(uint32_t color){
    ImageRGB32 image(WIDTH, HEIGHT);
    image.fill(color);
    return image;
}

//  The skip badge drawn on black: the white icon, white text strokes (one
//  column in four), the dark strip under them.
ImageRGB32 skip_badge_frame(){
    ImageRGB32 image = solid_frame(0xff000000);
    fill_box(1500, 100, 1920, 175, 0xff2c2c30, image);
    fill_box(1564, 128, 1586, 150, 0xffffffff, image);
    for (size_t x = 1630; x < 1875; x += 4){
        fill_box(x, 124, x + 1, 156, 0xffffffff, image);
    }
    return image;
}

//  The controls hint's card drawn on black: sky blue with a dark block for
//  the top of its "?".
ImageRGB32 controls_hint_frame(){
    ImageRGB32 image = solid_frame(0xff000000);
    fill_box(946, 868, 972, 902, 0xff7dd2fa, image);
    fill_box(956, 879, 962, 886, 0xff1c7db0, image);
    return image;
}

//  The platform reference with the player's area (x 780-1200, y 330-1080),
//  which is in no region, painted white.
ImageRGB32 platform_with_player_frame(){
    ImageRGB32 image = platform_reference_image(WIDTH, HEIGHT);
    fill_box(780, 330, 1200, HEIGHT, 0xffffffff, image);
    return image;
}

//  The advance chevron drawn in `ink` on `background`: a V 32x19 at
//  x 1400, y 1004, each arm 18% of the width across.
ImageRGB32 chevron_frame(uint32_t background, uint32_t ink){
    ImageRGB32 image = solid_frame(background);
    for (size_t y = 0; y < 19; y++){
        const double v = (double(y) + 0.5) / 19;
        for (size_t x = 0; x < 32; x++){
            const double u = (double(x) + 0.5) / 32;
            if (std::abs(std::abs(u - 0.5) - (0.40 * (1 - v) + 0.03)) <= 0.09){
                image.pixel(1400 + x, 1004 + y) = ink;
            }
        }
    }
    return image;
}

//  A solid white block the chevron's size on the ad's light blue.
ImageRGB32 chevron_sized_block_frame(){
    ImageRGB32 image = solid_frame(0xffc1ecf0);
    fill_box(1400, 1004, 1432, 1023, 0xffffffff, image);
    return image;
}

//  How mission_banner_frame() draws the icon.
enum class BannerIcon{
    NONE,
    DRAWN,
};

//  The mission banner drawn on a grey scene: the yellow bar, and with
//  BannerIcon::DRAWN the icon's white hexagon (as a block) with the orange
//  stroke, `dx` right of where the detector expects it.
ImageRGB32 mission_banner_frame(size_t dx, BannerIcon icon){
    ImageRGB32 image = solid_frame(0xff606060);
    fill_box(345, 410, 1590, 600, 0xfffad228, image);
    if (icon == BannerIcon::DRAWN){
        fill_box(700 + dx, 327, 738 + dx, 370, 0xffffffff, image);
        fill_box(716 + dx, 336, 720 + dx, 357, 0xfff07020, image);
    }
    return image;
}

//  The mission complete card drawn on a grey scene: the yellow bar, and
//  "COMPLETE" as green strokes (three columns in four) above it.
ImageRGB32 mission_complete_frame(){
    ImageRGB32 image = solid_frame(0xff606060);
    fill_box(345, 390, 1590, 585, 0xfffad228, image);
    for (size_t x = 615; x < 1300; x += 4){
        fill_box(x, 275, x + 3, 345, 0xffa5f07d, image);
    }
    return image;
}

constexpr uint32_t WHITE = 0xffffffff;
constexpr uint32_t BLACK = 0xff000000;
constexpr uint32_t GRAY = 0xff808080;
constexpr uint32_t AD_LIGHT_BLUE = 0xffc1ecf0;
constexpr uint32_t BOX_WHITE = 0xfffcfcf7;
constexpr uint32_t BOX_TEXT_DARK = 0xff323648;
constexpr uint32_t HINT_SKY_BLUE = 0xff7dd2fa;
constexpr uint32_t BANNER_YELLOW = 0xfffad228;
constexpr uint32_t COMPLETE_GREEN = 0xffa5f07d;

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
    {
        .name = "a black frame",
        .input = {.frame = []{ return solid_frame(BLACK); }},
        .expectation = {.reads = {.skip_prompt = false, .autosave = false, .controls_hint = false, .advance_arrow = false}},
    },
    {
        .name = "a white frame",
        .input = {.frame = []{ return solid_frame(WHITE); }},
        .expectation = {.reads = {
            .skip_prompt = false, .autosave = false, .controls_hint = false, .advance_arrow = false, .mission_banner = false,
        }},
    },
    {
        .name = "a gray frame",
        .input = {.frame = []{ return solid_frame(GRAY); }},
        .expectation = {.reads = {.platform = false}},
    },
    {
        .name = "the platform reference",
        .input = {.frame = []{ return platform_reference_image(WIDTH, HEIGHT); }},
        .expectation = {.reads = {.platform = true}},
    },
    {
        .name = "the platform reference with the player's area painted",
        .input = {.frame = platform_with_player_frame},
        .expectation = {.reads = {.platform = true}},
    },
    {
        .name = "the skip badge",
        .input = {.frame = skip_badge_frame},
        .expectation = {.reads = {.skip_prompt = true, .autosave = false}},
    },
    {
        .name = "the controls hint card",
        .input = {.frame = controls_hint_frame},
        .expectation = {.reads = {.controls_hint = true}},
    },
    {
        .name = "a frame all in the hint card's blue (a sky)",
        .input = {.frame = []{ return solid_frame(HINT_SKY_BLUE); }},
        .expectation = {.reads = {.controls_hint = false}},
    },
    {
        .name = "a white chevron on the ad's light blue",
        .input = {.frame = []{ return chevron_frame(AD_LIGHT_BLUE, WHITE); }},
        .expectation = {.reads = {.advance_arrow = true}},
    },
    {
        .name = "a dark chevron on a white box",
        .input = {.frame = []{ return chevron_frame(BOX_WHITE, BOX_TEXT_DARK); }},
        .expectation = {.reads = {.advance_arrow = true}},
    },
    {
        .name = "a solid block the chevron's size",
        .input = {.frame = chevron_sized_block_frame},
        .expectation = {.reads = {.advance_arrow = false}},
    },
    {
        .name = "the mission banner",
        .input = {.frame = []{ return mission_banner_frame(0, BannerIcon::DRAWN); }},
        .expectation = {.reads = {.mission_banner = true, .mission_complete = false}},
    },
    {
        .name = "the mission banner with its icon 40 px right",
        .input = {.frame = []{ return mission_banner_frame(40, BannerIcon::DRAWN); }},
        .expectation = {.reads = {.mission_banner = true}},
    },
    {
        .name = "the mission banner's bar without the icon",
        .input = {.frame = []{ return mission_banner_frame(0, BannerIcon::NONE); }},
        .expectation = {.reads = {.mission_banner = false}},
    },
    {
        .name = "a frame all in the bar's yellow",
        .input = {.frame = []{ return solid_frame(BANNER_YELLOW); }},
        .expectation = {.reads = {.mission_banner = false, .mission_complete = false}},
    },
    {
        .name = "the mission complete card",
        .input = {.frame = mission_complete_frame},
        .expectation = {.reads = {.mission_banner = false, .mission_complete = true}},
    },
    {
        .name = "a frame all in the card's green",
        .input = {.frame = []{ return solid_frame(COMPLETE_GREEN); }},
        .expectation = {.reads = {.mission_complete = false}},
    },
};

UnitTestResult check_synthetic_frame(const SyntheticFrameCase& test, Logger& logger){
    StoryDetectors detectors;
    const DetectorReads got = detectors.read(test.input.frame());
    logger.log(std::format("{}: {}{}", test.name, to_string(detectors.platform.last_distances()), firing(got)));
    const std::string wrong = mismatches(got, test.expectation.reads);
    if (!wrong.empty()){
        return std::format("Wrong on {}:{}", test.name, wrong);
    }
    return true;
}


//  Console frames, from folders in CommandLineTests.

//  The number of `digits` digits at `offset` in a frame's file name; none
//  if they are not all digits.
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

//  What a frame shows, from the readings in its name
//  ("0133-nothing+mission_banner.jpg").
DetectorReads as_named(std::string_view name){
    const auto has = [name](std::string_view reading){
        return name.find(reading) != std::string_view::npos;
    };
    return DetectorReads{
        .platform = has("+platform"), .skip_prompt = has("+skip_prompt"),
        .autosave = has("+autosave"), .controls_hint = has("+controls_hint"),
        .advance_arrow = has("+advance_arrow"), .mission_banner = has("+mission_banner"),
    };
}

//  The opening cutscene up to the platform: only 0050 shows the skip
//  prompt, and none is the platform.
std::optional<DetectorReads> opening(std::string_view name){
    return DetectorReads{.skip_prompt = name.starts_with("0050-")};
}

//  The platform (as named) with the controls hint (0001-0006), the walk,
//  the white fade, the title card, the text-free flyover, the starters, the
//  tourist choice and the start of the ad: no autosave badge, the advance
//  chevron only on the one box with its text done (0040).
std::optional<DetectorReads> arrival_to_ad(std::string_view name){
    const std::optional<int> index = frame_number(name, 0, 4);
    if (!index){
        return std::nullopt;
    }
    return DetectorReads{
        .platform = name.find("+platform") != std::string_view::npos,
        .controls_hint = *index <= 6, .advance_arrow = *index == 40,
    };
}

//  As arrival_to_ad(), on to the ad's subtitles (0090-0115). The advance
//  chevron on the boxes with their text done (0044, 0049, 0053, 0078) and
//  on the subtitles once typed (0093-0115).
std::optional<DetectorReads> arrival_to_ad_subtitles(std::string_view name){
    const std::optional<int> index = frame_number(name, 0, 4);
    if (!index){
        return std::nullopt;
    }
    const bool box_arrow = *index == 44 || *index == 49 || *index == 53 || *index == 78;
    return DetectorReads{
        .platform = name.find("+platform") != std::string_view::npos,
        .controls_hint = *index <= 6,
        .advance_arrow = box_arrow || (*index >= 93 && *index <= 115),
    };
}

//  As arrival_to_ad_subtitles(), on through the ad and the bag scene. The
//  platform, the controls hint and the advance chevron as named; the
//  mission banner on 0181 only, the last frame.
std::optional<DetectorReads> arrival_to_mission_banner(std::string_view name){
    const std::optional<int> index = frame_number(name, 0, 4);
    if (!index){
        return std::nullopt;
    }
    return DetectorReads{
        .platform = name.find("+platform") != std::string_view::npos,
        .controls_hint = name.find("+controls_hint") != std::string_view::npos,
        .advance_arrow = name.find("+advance_arrow") != std::string_view::npos,
        .mission_banner = *index == 181,
    };
}

//  Every detector as named; the banner on 0181-0189, no complete card.
std::optional<DetectorReads> arrival_past_mission_banner(std::string_view name){
    return as_named(name);
}

//  As named, and the complete card on 0045 and 0046 (named "nothing"); the
//  next mission's banner (0133-0139) is not the card.
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
        size_t frames = 0;  //  How many frames of the folder `reads` has an expectation for.
        //  Each frame's reads, by file name; none for a file not in the set.
        //  A detector it leaves unset must not fire.
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
        if (!name.ends_with(".jpg")){
            continue;
        }
        const std::optional<DetectorReads> want = test.expectation.reads(name);
        if (!want){
            continue;
        }
        const DetectorReads got = detectors.read(ImageRGB32(path.string()));
        logger.log(std::format("{}: {}{}", name, to_string(detectors.platform.last_distances()), firing(got)));
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
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
