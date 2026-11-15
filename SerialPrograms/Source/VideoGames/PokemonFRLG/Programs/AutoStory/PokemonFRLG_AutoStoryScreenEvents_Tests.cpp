/*  FRLG AutoStory - Screen Events Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The adapter on synthetic frames: a uniform screen is a fade and nothing
 *  else, and no frame shows nothing. Frames of real boxes and battles are
 *  the detectors' own image tests (CommandLineTests); a flat grey frame is
 *  not one of those (the battle text box reads it as a box). Console frames
 *  the adapter once misread are replayed here too, from CommandLineTests;
 *  such a case skips when its image is missing.
 *
 */

#include <format>
#include <optional>
#include <string>
#include <utility>
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "PokemonFRLG_AutoStoryFrameReplay_Tests.h"
#include "PokemonFRLG_AutoStoryScreenEvents.h"
#include "PokemonFRLG_AutoStoryScreenEvents_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

struct ScreenEventsCase{
    struct Input{
        std::string name;
        //  The frame's colour; none for no frame.
        std::optional<uint32_t> color;
        //  The colour of a square in the middle of the frame, around the
        //  player; 0 for none.
        uint32_t middle = 0;
    };

    struct Expectation{
        ScreenEvents events;
    };

    Input input;
    Expectation expectation;
};

class Test_AutoStoryScreenEvents : public UnitTest{
public:
    explicit Test_AutoStoryScreenEvents(ScreenEventsCase test_case)
        : UnitTest(std::format("PokemonFRLG::AutoStoryScreenEvents - {}", test_case.input.name))
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        VideoSnapshot frame;
        if (m_case.input.color.has_value()){
            ImageRGB32 image(1920, 1080);
            image.fill(*m_case.input.color);
            if (m_case.input.middle != 0){
                for (size_t y = 380; y < 700; y++){
                    for (size_t x = 800; x < 1120; x++){
                        image.pixel(x, y) = m_case.input.middle;
                    }
                }
            }
            frame = VideoSnapshot(std::move(image), WallClock());
        }
        FrlgScreenObserver observer;
        const ScreenEvents got = observer.observe(frame, WallClock());
        if (got != m_case.expectation.events){
            return std::format("expected {}, got {}", describe(m_case.expectation.events), describe(got));
        }
        return true;
    }

private:
    ScreenEventsCase m_case;
};

struct BattleFrameCase{
    struct Input{
        //  The console frame, under CommandLineTests.
        std::string image;
    };

    struct Expectation{
        bool battle;
    };

    std::string name;
    Input input;
    Expectation expectation;
};

//  Whether the adapter reports BATTLE_STARTED on a console frame.
class Test_AutoStoryBattleFrame : public UnitTest{
public:
    explicit Test_AutoStoryBattleFrame(BattleFrameCase test_case)
        : UnitTest(std::format("PokemonFRLG::AutoStoryScreenEvents - {}", test_case.name))
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::string path = UNIT_TEST_RESOURCE_PATH() + m_case.input.image;
        if (!Filesystem::exists(Filesystem::Path(path))){
            return UnitTestResult(UnitTestResult::SKIPPED, std::format("Missing image: {}", path));
        }
        FrlgScreenObserver observer;
        const ScreenEvents got = observer.observe(VideoSnapshot(ImageRGB32(path), WallClock()), WallClock());
        if (got.has(ScreenEvent::BATTLE_STARTED) != m_case.expectation.battle){
            return std::format("expected battle {}, got {}", m_case.expectation.battle, describe(got));
        }
        return true;
    }

private:
    BattleFrameCase m_case;
};

} // namespace


const ScreenEventsCase SCREEN_EVENTS_CASES[] = {
    {
        .input = {"no frame shows nothing", std::nullopt},
        .expectation = {{}},
    },
    {
        .input = {"a black screen is a black fade only", 0xff000000},
        .expectation = {{ScreenEvent::FADE_BLACK}},
    },
    {
        .input = {"a white screen is a white fade only", 0xffffffff},
        .expectation = {{ScreenEvent::FADE_WHITE}},
    },
    {
        //  Rock Tunnel without Flash (console B, 2026-11-09, 08.01): its
        //  black read as a battle's text box before each step.
        .input = {"a dark cave, black but for a lit middle, shows nothing", 0xff000000, 0xff8c7c64},
        .expectation = {{}},
    },
};

//  Console B's captures. Silph Co. 5F (16.03, 2026-11-14): the floor over
//  near-black below the map, which the box's blue alone took for a battle's
//  text box before each step.
const BattleFrameCase BATTLE_FRAME_CASES[] = {
    {
        .name = "Silph Co. 5F, near-black below the map, is no battle",
        .input = {"PokemonFRLG/AutoStoryScreenEvents/SilphCo5F_Overworld_False.png"},
        .expectation = {false},
    },
    {
        .name = "Pewter's Pokemon Center, black below the room, is no battle",
        .input = {"PokemonFRLG/BattleTextBoxDetector/PewterPokemonCenter_BlackBelow_False.png"},
        .expectation = {false},
    },
    {
        .name = "a battle's opening on Route 1 is a battle",
        .input = {"PokemonFRLG/BattleTextBoxDetector/Route1_Opening_True.png"},
        .expectation = {true},
    },
    {
        .name = "a wild Caterpie in Viridian Forest is a battle",
        .input = {"PokemonFRLG/BattleTextBoxDetector/ViridianForest_WildCaterpie_True.png"},
        .expectation = {true},
    },
};

void add_tests_AutoStoryScreenEvents(UnitTestDatabase& database){
    for (const auto& item : SCREEN_EVENTS_CASES){
        database.add<Test_AutoStoryScreenEvents>(item);
    }
    for (const auto& item : BATTLE_FRAME_CASES){
        database.add<Test_AutoStoryBattleFrame>(item);
    }
    //  The same events on recorded console frames.
    add_tests_AutoStoryFrameReplay(database);
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
