/*  PokemonLZA AutoStory - Opening detectors tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Synthetic frames always. The opening's frames are not in
 *  CommandLineTests yet; each set runs when its folder is in the
 *  environment and is skipped otherwise (console B, 1920x1080):
 *  - PLZA_OPENING_FRAMES: platform_ref.png and the lab's cutscene,
 *    0001-*.png ... 0056-*.png;
 *  - PLZA_USER_OPENING_FRAMES: the user's run, rec1_u001.png ...
 *    rec1_u146.png, one per ~1.5 s, with the autosave badge (in Chinese);
 *  - PLZA_0101_RUN1_FRAMES: 01.01's first run (2026-11-10 13:00, in
 *    Chinese), 0001-*.png ... 0089-*.png: the platform with the controls
 *    hint, the walk to the plaza and the arrival scenes;
 *  - PLZA_0101_RUN2_FRAMES: 01.01's second run (2026-11-10 13:28, in
 *    Chinese), 0001-*.png ... 0115-*.png: the same, on to the ad's
 *    subtitles (0090-0115), where the run stopped.
 *
 */

#include <cmath>
#include <cstdlib>
#include <format>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "PokemonLZA_AutoStoryBadgeDetectors.h"
#include "PokemonLZA_AutoStoryOpeningDetectors.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

namespace{

constexpr size_t WIDTH = 1920;
constexpr size_t HEIGHT = 1080;

void fill_box(ImageRGB32& image, size_t min_x, size_t min_y, size_t max_x, size_t max_y, uint32_t color){
    for (size_t y = min_y; y < max_y; y++){
        for (size_t x = min_x; x < max_x; x++){
            image.pixel(x, y) = color;
        }
    }
}

//  The skip badge drawn on black: the white icon, white text strokes (one
//  column in four), the dark strip under them.
ImageRGB32 skip_badge_frame(){
    ImageRGB32 image(WIDTH, HEIGHT);
    image.fill(0xff000000);
    fill_box(image, 1500, 100, 1920, 175, 0xff2c2c30);
    fill_box(image, 1564, 128, 1586, 150, 0xffffffff);
    for (size_t x = 1630; x < 1875; x += 4){
        fill_box(image, x, 124, x + 1, 156, 0xffffffff);
    }
    return image;
}

//  The controls hint's card drawn on black: sky blue with a dark block for
//  the top of its "?".
ImageRGB32 controls_hint_frame(){
    ImageRGB32 image(WIDTH, HEIGHT);
    image.fill(0xff000000);
    fill_box(image, 946, 868, 972, 902, 0xff7dd2fa);
    fill_box(image, 956, 879, 962, 886, 0xff1c7db0);
    return image;
}

UnitTestResult test_synthetic_platform(Logger& logger){
    const ImageRGB32 reference = platform_reference_image(WIDTH, HEIGHT);
    const PlatformDistances same = platform_distances(reference);
    logger.log("reference: " + to_string(same));
    if (!platform_matches(same) || same.top > 1 || same.left > 1 || same.right > 1){
        return "Expected the reference's own cells to match at about 0: " + to_string(same);
    }
    ImageRGB32 gray(WIDTH, HEIGHT);
    gray.fill(0xff808080);
    const PlatformDistances other = platform_distances(gray);
    if (platform_matches(other)){
        return "Expected a gray frame not to be the platform: " + to_string(other);
    }
    //  The player's area (x 780-1200, y 330-1080) is in no region.
    ImageRGB32 with_player = reference.copy();
    fill_box(with_player, 780, 330, 1200, HEIGHT, 0xffffffff);
    if (!platform_matches(platform_distances(with_player))){
        return "Expected the player's area not to count.";
    }
    return true;
}

//  The advance chevron drawn in `ink` on `background`: a V 32x19 at
//  x 1400, y 1004, each arm 18% of the width across.
ImageRGB32 chevron_frame(uint32_t background, uint32_t ink){
    ImageRGB32 image(WIDTH, HEIGHT);
    image.fill(background);
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

//  The chevron white on the ad's light blue and dark on a white box; not a
//  plain white or black frame, nor a solid block the chevron's size.
UnitTestResult test_synthetic_arrow(Logger&){
    AdvanceArrowDetector arrow;
    if (!arrow.detect(chevron_frame(0xffc1ecf0, 0xffffffff)) || !arrow.detect(chevron_frame(0xfffcfcf7, 0xff323648))){
        return "Expected the drawn chevrons to be the advance arrow.";
    }
    ImageRGB32 block(WIDTH, HEIGHT);
    block.fill(0xffc1ecf0);
    fill_box(block, 1400, 1004, 1432, 1023, 0xffffffff);
    ImageRGB32 white(WIDTH, HEIGHT);
    white.fill(0xffffffff);
    ImageRGB32 black(WIDTH, HEIGHT);
    black.fill(0xff000000);
    if (arrow.detect(block) || arrow.detect(white) || arrow.detect(black)){
        return "Expected a white block, a white or a black frame not to be the advance arrow.";
    }
    return true;
}

UnitTestResult test_synthetic_badges(Logger&){
    SkipPromptDetector detector;
    if (!detector.detect(skip_badge_frame())){
        return "Expected the drawn badge to be the skip prompt.";
    }
    ImageRGB32 white(WIDTH, HEIGHT);
    white.fill(0xffffffff);
    ImageRGB32 black(WIDTH, HEIGHT);
    black.fill(0xff000000);
    if (detector.detect(white) || detector.detect(black)){
        return "Expected a white or black frame not to be the skip prompt.";
    }
    AutosaveDetector autosave;
    if (autosave.detect(white) || autosave.detect(black) || autosave.detect(skip_badge_frame())){
        return "Expected a white, black or skip-badge frame not to be the autosave badge.";
    }
    ControlsHintDetector hint;
    if (!hint.detect(controls_hint_frame()) || hint.detect(white) || hint.detect(black)){
        return "Expected the drawn card, and no white or black frame, to be the controls hint.";
    }
    ImageRGB32 sky(WIDTH, HEIGHT);
    sky.fill(0xff7dd2fa);
    if (hint.detect(sky)){
        return "Expected a frame all in the card's blue (a sky) not to be the controls hint.";
    }
    return true;
}

//  What a frame should show.
struct FrameExpectation{
    bool platform;
    bool skip_prompt;
    bool autosave;
    bool controls_hint;
    bool advance_arrow;
};

//  The lab's opening: platform_ref.png is the platform, no cutscene frame
//  0001-0056 is; only 0050 shows the skip prompt. The platform reference
//  was taken after the controls hint had gone.
std::optional<FrameExpectation> lab_opening(const std::string& name){
    return FrameExpectation{
        .platform = name == "platform_ref.png", .skip_prompt = name.starts_with("0050-"),
        .autosave = false, .controls_hint = false, .advance_arrow = false,
    };
}

//  The user's run rec1_u001-u146: the platform settles at u131 (u124-u130
//  are the fade in and the camera moving in; u128 already passes, top 77
//  under 90, as the platform it shows), the autosave badge shows on
//  u132 only, the controls hint on u132-u138 (~10 s), no skip prompt. The
//  train's blue box has its advance chevron on u116-u122.
//  Later frames of the folder are not in the set.
std::optional<FrameExpectation> user_opening(const std::string& name){
    const int index = std::stoi(name.substr(6, 3));
    if (index > 146){
        return std::nullopt;
    }
    return FrameExpectation{
        .platform = index == 128 || index >= 131, .skip_prompt = false,
        .autosave = index == 132, .controls_hint = index >= 132 && index <= 138,
        .advance_arrow = index >= 116 && index <= 122,
    };
}

//  01.01's first run: the platform (0001-0003, as the run read it) and the
//  walk with the controls hint (0001-0006; it is gone by 0007), then the
//  white fade, the title card, the text-free flyover, the starters and the
//  girl, the tourist choice (0065) and the ad (0080-0089): no free control.
//  No autosave: the run took flyover frames 0033 and 0038 for one. The
//  advance chevron on the one box with its text done, 0040.
std::optional<FrameExpectation> run1_arrival(const std::string& name){
    const int index = std::stoi(name.substr(0, 4));
    return FrameExpectation{
        .platform = name.find("+platform") != std::string::npos, .skip_prompt = false,
        .autosave = false, .controls_hint = index <= 6, .advance_arrow = index == 40,
    };
}

//  01.01's second run: as the first, with the hint on 0001-0006, on to the
//  ad (the logo 0088-0089, the president's subtitles 0090-0115). The
//  advance chevron on the boxes with their text done (0044, 0049, 0053,
//  0078) and on the ad's subtitles once typed (0093-0115).
std::optional<FrameExpectation> run2_arrival(const std::string& name){
    const int index = std::stoi(name.substr(0, 4));
    const bool box_arrow = index == 44 || index == 49 || index == 53 || index == 78;
    return FrameExpectation{
        .platform = name.find("+platform") != std::string::npos, .skip_prompt = false,
        .autosave = false, .controls_hint = index <= 6,
        .advance_arrow = box_arrow || (index >= 93 && index <= 115),
    };
}

//  The opening's detectors, one of each.
struct OpeningDetectors{
    PlatformArrivalDetector platform;
    SkipPromptDetector skip;
    AutosaveDetector autosave;
    ControlsHintDetector hint;
    AdvanceArrowDetector arrow;

    FrameExpectation read(const ImageRGB32& frame){
        return FrameExpectation{
            .platform = platform.detect(frame), .skip_prompt = skip.detect(frame),
            .autosave = autosave.detect(frame), .controls_hint = hint.detect(frame),
            .advance_arrow = arrow.detect(frame),
        };
    }
};

bool same_reads(const FrameExpectation& got, const FrameExpectation& want){
    return got.platform == want.platform && got.skip_prompt == want.skip_prompt &&
        got.autosave == want.autosave && got.controls_hint == want.controls_hint &&
        got.advance_arrow == want.advance_arrow;
}

//  The frames `prefix`*.png in `folder` that `expect` has an expectation
//  for, against it; logs each one.
UnitTestResult check_frames(
    Logger& logger, const std::string& folder, const std::string& prefix,
    size_t count, const std::function<std::optional<FrameExpectation>(const std::string&)>& expect
){
    OpeningDetectors detectors;
    size_t frames = 0;
    std::string failures;
    for (const Filesystem::Path& path : Filesystem::list_directory(Filesystem::Path(folder))){
        const std::string name = path.filename().string();
        const std::optional<FrameExpectation> want =
            name.starts_with(prefix) && name.ends_with(".png") ? expect(name) : std::nullopt;
        if (!want){
            continue;
        }
        const ImageRGB32 frame(path.string());
        const FrameExpectation got = detectors.read(frame);
        logger.log(std::format("{}: {}{}{}{}{}{}", name, to_string(detectors.platform.last_distances()),
            got.platform ? ", platform" : "", got.skip_prompt ? ", skip prompt" : "",
            got.autosave ? ", autosave" : "", got.controls_hint ? ", controls hint" : "",
            got.advance_arrow ? ", advance arrow" : ""));
        if (!same_reads(got, *want)){
            failures += " " + name;
        }
        frames++;
    }
    if (frames != count){
        return std::format("Expected {} frames in {}, found {}.", count, folder, frames);
    }
    if (!failures.empty()){
        return "Wrong on:" + failures;
    }
    return true;
}

//  The folder in the environment variable `variable`; none when unset.
std::string frames_folder(std::string_view variable){
    const auto* folder = std::getenv(std::string(variable).c_str());
    if (folder == nullptr || !Filesystem::is_directory(Filesystem::Path(folder))){
        return "";
    }
    return folder;
}

UnitTestResult test_lab_opening_frames(Logger& logger){
    const std::string folder = frames_folder("PLZA_OPENING_FRAMES");
    if (folder.empty()){
        return UnitTestResult(UnitTestResult::SKIPPED, "PLZA_OPENING_FRAMES is not a folder.");
    }
    //  0001-0056 and platform_ref.png.
    return check_frames(logger, folder, "", 57, lab_opening);
}

UnitTestResult test_user_opening_frames(Logger& logger){
    const std::string folder = frames_folder("PLZA_USER_OPENING_FRAMES");
    if (folder.empty()){
        return UnitTestResult(UnitTestResult::SKIPPED, "PLZA_USER_OPENING_FRAMES is not a folder.");
    }
    return check_frames(logger, folder, "rec1_u", 146, user_opening);
}

UnitTestResult test_run1_arrival_frames(Logger& logger){
    const std::string folder = frames_folder("PLZA_0101_RUN1_FRAMES");
    if (folder.empty()){
        return UnitTestResult(UnitTestResult::SKIPPED, "PLZA_0101_RUN1_FRAMES is not a folder.");
    }
    return check_frames(logger, folder, "00", 89, run1_arrival);
}

UnitTestResult test_run2_arrival_frames(Logger& logger){
    const std::string folder = frames_folder("PLZA_0101_RUN2_FRAMES");
    if (folder.empty()){
        return UnitTestResult(UnitTestResult::SKIPPED, "PLZA_0101_RUN2_FRAMES is not a folder.");
    }
    return check_frames(logger, folder, "0", 115, run2_arrival);
}


struct OpeningTestInput{
    std::string name;
    std::function<UnitTestResult(Logger&)> body;
};

struct OpeningTestExpectation{
};

struct OpeningTestCase{
    OpeningTestInput input;
    OpeningTestExpectation expectation;
};

class Test_AutoStoryOpeningDetectors : public UnitTest{
public:
    explicit Test_AutoStoryOpeningDetectors(OpeningTestCase test_case)
        : UnitTest("PokemonLZA::AutoStoryOpeningDetectors - " + test_case.input.name)
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        return m_case.input.body(logger);
    }

private:
    OpeningTestCase m_case;
};

} // namespace


const OpeningTestCase OPENING_TEST_CASES[] = {
    {.input = {"platform: synthetic frames", test_synthetic_platform}, .expectation = {}},
    {.input = {"skip prompt, autosave and controls hint: synthetic frames", test_synthetic_badges}, .expectation = {}},
    {.input = {"advance arrow: synthetic frames", test_synthetic_arrow}, .expectation = {}},
    {.input = {"the lab's opening frames (PLZA_OPENING_FRAMES)", test_lab_opening_frames}, .expectation = {}},
    {.input = {"the user's opening frames (PLZA_USER_OPENING_FRAMES)", test_user_opening_frames}, .expectation = {}},
    {.input = {"01.01 run 1's arrival frames (PLZA_0101_RUN1_FRAMES)", test_run1_arrival_frames}, .expectation = {}},
    {.input = {"01.01 run 2's arrival frames to the ad (PLZA_0101_RUN2_FRAMES)", test_run2_arrival_frames}, .expectation = {}},
};

void add_tests_AutoStoryOpeningDetectors(UnitTestDatabase& database){
    for (const OpeningTestCase& item : OPENING_TEST_CASES){
        database.add<Test_AutoStoryOpeningDetectors>(item);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
