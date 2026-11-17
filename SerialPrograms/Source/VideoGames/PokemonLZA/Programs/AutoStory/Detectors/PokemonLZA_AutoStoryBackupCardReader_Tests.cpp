/*  PokemonLZA AutoStory - Backup card reader Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cstdint>
#include <format>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "PokemonLZA_AutoStoryBackupCardReader.h"
#include "PokemonLZA_AutoStoryBackupCardReader_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

std::string describe(std::optional<size_t> registered){
    return registered ? std::format("{} registered", *registered) : std::string("no backup card");
}

ImageRGB32 flat_frame(uint32_t color){
    ImageRGB32 image(1920, 1080);
    image.fill(color);
    return image;
}


struct SyntheticCase{
    std::string name;
    struct Input{
        std::function<ImageRGB32()> frame;
    };
    Input input;
    struct Expectation{
        std::optional<size_t> registered;   //  None: no card, or no number on it.
    };
    Expectation expectation;
};

const SyntheticCase SYNTHETIC_CASES[] = {
    {.name = "an empty frame", .input = {.frame = []{ return ImageRGB32(); }}, .expectation = {.registered = std::nullopt}},
    {.name = "a black frame", .input = {.frame = []{ return flat_frame(0xff000000); }}, .expectation = {.registered = std::nullopt}},
    {.name = "a white card with no number", .input = {.frame = []{ return flat_frame(0xffffffff); }}, .expectation = {.registered = std::nullopt}},
};

class SyntheticTest : public UnitTest{
public:
    explicit SyntheticTest(SyntheticCase test_case)
        : UnitTest(std::format("PokemonLZA::AutoStory - Backup card reader: {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        const std::optional<size_t> registered = read_backup_card_registered(logger, m_case.input.frame());
        if (registered != m_case.expectation.registered){
            return std::format("expected {}, read {}.", describe(m_case.expectation.registered), describe(registered));
        }
        return true;
    }
private:
    SyntheticCase m_case;
};


//  The console's frames, PokemonLZA/AutoStoryBackupCard/<file> (console B,
//  2026-11-16, Italian; Japanese: the user's capture, 3 registered).
struct CaptureCase{
    std::string name;
    struct Input{
        std::string file;
    };
    Input input;
    struct Expectation{
        std::optional<size_t> registered;
    };
    Expectation expectation;
};

const CaptureCase CAPTURE_CASES[] = {
    {.name = "the backup card, 1 registered", .input = {.file = "Italian_card.png"}, .expectation = {.registered = 1}},
    {
        .name = "the backup card under \"Backup data loaded.\"",
        .input = {.file = "Italian_card_loaded.png"}, .expectation = {.registered = 1},
    },
    {.name = "the title screen", .input = {.file = "Italian_title.png"}, .expectation = {.registered = std::nullopt}},
    //  The count reads "3" and the counter kanji hiki: English OCR reads "3a", which the number
    //  reader's misread table turned into 34.
    {.name = "the Japanese backup card, 3 registered and a counter word", .input = {.file = "Japanese_card.png"}, .expectation = {.registered = 3}},
    {.name = "in the game after the load", .input = {.file = "Italian_in_game.png"}, .expectation = {.registered = std::nullopt}},
};

class CaptureTest : public UnitTest{
public:
    explicit CaptureTest(CaptureCase test_case)
        : UnitTest(std::format("PokemonLZA::AutoStory - Backup card reader: {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        const std::string path = std::format("{}PokemonLZA/AutoStoryBackupCard/{}", UNIT_TEST_RESOURCE_PATH(), m_case.input.file);
        if (!Filesystem::exists(Filesystem::Path(path))){
            return UnitTestResult(UnitTestResult::SKIPPED, std::format("{} is not in the test images.", m_case.input.file));
        }
        const std::optional<size_t> registered = read_backup_card_registered(logger, ImageRGB32(path));
        if (registered != m_case.expectation.registered){
            return std::format("{}: expected {}, read {}.", m_case.input.file, describe(m_case.expectation.registered), describe(registered));
        }
        return true;
    }
private:
    CaptureCase m_case;
};

} // namespace


void add_tests_AutoStoryBackupCardReader(UnitTestDatabase& database){
    for (const SyntheticCase& test : SYNTHETIC_CASES){
        database.add<SyntheticTest>(test);
    }
    for (const CaptureCase& test : CAPTURE_CASES){
        database.add<CaptureTest>(test);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
