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
#include "PokemonLZA_AutoStoryDetectors_Tests.h"
#include "PokemonLZA_AutoStoryPlatformArrivalDetector.h"

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
    std::optional<bool> autosave = {};
    std::optional<bool> advance_arrow = {};
};

struct DetectorField{
    std::string_view name;
    std::optional<bool> DetectorReads::* read;
};
const DetectorField DETECTOR_FIELDS[] = {
    {"platform", &DetectorReads::platform},
    {"autosave", &DetectorReads::autosave},
    {"advance arrow", &DetectorReads::advance_arrow},
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
    AutosaveDetector autosave;
    AdvanceArrowDetector arrow;

    DetectorReads read(const ImageViewRGB32& frame){
        return DetectorReads{
            .platform = platform.detect(frame), .autosave = autosave.detect(frame),
            .advance_arrow = arrow.detect(frame),
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

//  The autosave badge's pencil, 48 px right of the Chinese badge's (where
//  the shorter Japanese text puts it), over `background`: a white stroke 4 px
//  wide in a `glow` band 12 px wide.
ImageRGB32 autosave_badge_frame(uint32_t background, uint32_t glow){
    ImageRGB32 image(WIDTH, HEIGHT);
    image.fill(background);
    fill_box(1655, 73, 1667, 104, glow, image);
    fill_box(1659, 73, 1663, 104, 0xffffffff, image);
    return image;
}

constexpr uint32_t WHITE = 0xffffffff;
constexpr uint32_t BLACK = 0xff000000;
constexpr uint32_t GRAY = 0xff808080;
constexpr uint32_t AD_LIGHT_BLUE = 0xffc1ecf0;
constexpr uint32_t BOX_WHITE = 0xfffcfcf7;
constexpr uint32_t BOX_TEXT_DARK = 0xff323648;

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
        .expectation = {.reads = {.autosave = false, .advance_arrow = false}},
    },
    {
        .name = "a white frame",
        .input = {.frame = []{ return solid_frame(WHITE); }},
        .expectation = {.reads = {.autosave = false, .advance_arrow = false}},
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
    //  The autosave badge's glow is bright over a dark sky and dim over a
    //  lamp-lit wall (about 28, 70, 85: 02.04's check-in,
    //  frame 0075, which the badge detector missed).
    {
        .name = "the autosave badge with a bright glow",
        .input = {.frame = []{ return autosave_badge_frame(0xff101418, 0xff28c8e6); }},
        .expectation = {.reads = {.autosave = true}},
    },
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
//  ("0001-nothing+platform+controls_hint.jpg"); a reading of a detector
//  not in this module is ignored.
DetectorReads as_named(std::string_view name){
    const auto has = [name](std::string_view reading){
        return name.find(reading) != std::string_view::npos;
    };
    return DetectorReads{
        .platform = has("+platform"), .autosave = has("+autosave"),
        .advance_arrow = has("+advance_arrow"),
    };
}

//  The opening cutscene up to the platform: none is the platform.
std::optional<DetectorReads> opening(std::string_view){
    return DetectorReads{};
}

//  The platform (as named), the walk, the white fade, the title card, the
//  text-free flyover, the starters, the tourist choice and the start of the
//  ad: no autosave badge, the advance chevron only on the one box with its
//  text done (0040).
std::optional<DetectorReads> arrival_to_ad(std::string_view name){
    const std::optional<int> index = frame_number(name, 0, 4);
    if (!index){
        return std::nullopt;
    }
    return DetectorReads{
        .platform = name.find("+platform") != std::string_view::npos,
        .advance_arrow = *index == 40,
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
        .advance_arrow = box_arrow || (*index >= 93 && *index <= 115),
    };
}

//  As arrival_to_ad_subtitles(), on through the ad and the bag scene. The
//  platform and the advance chevron as named.
std::optional<DetectorReads> arrival_to_mission_banner(std::string_view name){
    const std::optional<int> index = frame_number(name, 0, 4);
    if (!index){
        return std::nullopt;
    }
    return DetectorReads{
        .platform = name.find("+platform") != std::string_view::npos,
        .advance_arrow = name.find("+advance_arrow") != std::string_view::npos,
    };
}

//  Every detector as named. The autosave badge on 0197 too: it is named
//  "nothing" but shows the badge
//  (Traditional Chinese, top right), right after the black fade.
std::optional<DetectorReads> arrival_past_mission_banner(std::string_view name){
    DetectorReads want = as_named(name);
    if (name.starts_with("0197-")){
        want.autosave = true;
    }
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
