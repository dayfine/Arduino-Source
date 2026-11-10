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
 *    rec1_u146.png, one per ~1.5 s, with the autosave badge (in Chinese).
 *
 */

#include <cstdlib>
#include <format>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
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
    return true;
}

//  What a frame should show.
struct FrameExpectation{
    bool platform;
    bool skip_prompt;
    bool autosave;
};

//  The lab's opening: platform_ref.png is the platform, no cutscene frame
//  0001-0056 is; only 0050 shows the skip prompt.
FrameExpectation lab_opening(const std::string& name){
    return {.platform = name == "platform_ref.png", .skip_prompt = name.starts_with("0050-"), .autosave = false};
}

//  The user's run rec1_u001-u146: the platform settles at u131 (u124-u130
//  are the fade in and the camera moving in), the autosave badge shows on
//  u132 only, no skip prompt.
FrameExpectation user_opening(const std::string& name){
    const int index = std::stoi(name.substr(6, 3));
    return {.platform = index >= 131, .skip_prompt = false, .autosave = index == 132};
}

//  The frames `prefix`*.png in `folder` against `expect`; logs each one.
UnitTestResult check_frames(
    Logger& logger, const std::string& folder, const std::string& prefix,
    size_t count, const std::function<FrameExpectation(const std::string&)>& expect
){
    PlatformArrivalDetector platform;
    SkipPromptDetector skip;
    AutosaveDetector autosave;
    size_t frames = 0;
    std::string failures;
    for (const Filesystem::Path& path : Filesystem::list_directory(Filesystem::Path(folder))){
        const std::string name = path.filename().string();
        if (!name.starts_with(prefix) || !name.ends_with(".png")){
            continue;
        }
        const ImageRGB32 frame(path.string());
        const FrameExpectation got{.platform = platform.detect(frame), .skip_prompt = skip.detect(frame), .autosave = autosave.detect(frame)};
        logger.log(std::format("{}: {}{}{}{}", name, to_string(platform.last_distances()),
            got.platform ? ", platform" : "", got.skip_prompt ? ", skip prompt" : "", got.autosave ? ", autosave" : ""));
        const FrameExpectation want = expect(name);
        if (got.platform != want.platform || got.skip_prompt != want.skip_prompt || got.autosave != want.autosave){
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


class Test_AutoStoryOpeningDetectors : public UnitTest{
public:
    Test_AutoStoryOpeningDetectors(std::string name, std::function<UnitTestResult(Logger&)> body)
        : UnitTest("PokemonLZA::AutoStoryOpeningDetectors - " + name)
        , m_body(std::move(body))
    {}

    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        return m_body(logger);
    }

private:
    std::function<UnitTestResult(Logger&)> m_body;
};

} // namespace


void add_tests_AutoStoryOpeningDetectors(UnitTestDatabase& database){
    database.add<Test_AutoStoryOpeningDetectors>("platform: synthetic frames", test_synthetic_platform);
    database.add<Test_AutoStoryOpeningDetectors>("skip prompt and autosave: synthetic frames", test_synthetic_badges);
    database.add<Test_AutoStoryOpeningDetectors>("the lab's opening frames (PLZA_OPENING_FRAMES)", test_lab_opening_frames);
    database.add<Test_AutoStoryOpeningDetectors>("the user's opening frames (PLZA_USER_OPENING_FRAMES)", test_user_opening_frames);
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
