/*  AutoStory Fly Landing Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The 23:59 case (console B, 2026-11-12, 12.03) as scripted looks, and on
 *  the run's frames: the flight's black band, and Fuchsia's overworld
 *  before the fade, are quiet with nothing showing and are no landing; the
 *  Pallet Town frame after a fade is.
 *
 */

#include <format>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "Pokemon/AutoStory/Pokemon_ScreenEventLog.h"
#include "Tests/ImageUnitTest.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_PlayerFacingReader.h"
#include "PokemonFRLG_AutoStoryFlyLanding.h"
#include "PokemonFRLG_AutoStoryScreenEvents.h"
#include "PokemonFRLG_AutoStoryQuietOverworld.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using Pokemon::Direction;
using Pokemon::ScreenEvent;
using Pokemon::ScreenEvents;

namespace{

constexpr Milliseconds QUIET(1000);

//  A scripted look: the events each frame shows, 100 ms apart.
struct ScriptedFrame{
    ScreenEvents events;
};

Pokemon::ScreenLook scripted_look(const std::vector<ScriptedFrame>& frames){
    Pokemon::ScreenLook look;
    const WallClock start = current_time();
    look.window.start = start;
    WallClock now = start;
    for (const ScriptedFrame& frame : frames){
        look.log.record(frame.events, now);
        now += Milliseconds(100);
    }
    look.window.end = now;
    return look;
}

std::vector<ScriptedFrame> repeated(const ScreenEvents& events, size_t count){
    return std::vector<ScriptedFrame>(count, ScriptedFrame{events});
}

std::vector<ScriptedFrame> joined(std::vector<ScriptedFrame> a, const std::vector<ScriptedFrame>& b){
    a.insert(a.end(), b.begin(), b.end());
    return a;
}

struct FlyLandingInput{
    std::string name;
    std::vector<ScriptedFrame> frames;
    //  What the facing read returns, once asked.
    std::optional<Direction> facing;
};

struct FlyLandingExpectation{
    bool landed;
    //  Whether the facing was read at all.
    bool read_facing;
};

struct FlyLandingCase{
    FlyLandingInput input;
    FlyLandingExpectation expectation;
};

class Test_AutoStoryFlyLanding : public UnitTest{
public:
    explicit Test_AutoStoryFlyLanding(FlyLandingCase test_case)
        : UnitTest(std::format("PokemonFRLG::AutoStoryFlyLanding - {}", test_case.input.name))
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        bool read = false;
        const FlyLanding got = judge_fly_landing(
            scripted_look(m_case.input.frames), QUIET,
            [&]{
                read = true;
                return m_case.input.facing;
            }
        );
        if (got.landed() != m_case.expectation.landed || read != m_case.expectation.read_facing){
            return std::format(
                "expected landed {} (facing read {}), got landed {} (facing read {}): {}",
                m_case.expectation.landed, m_case.expectation.read_facing, got.landed(), read, describe(got)
            );
        }
        return true;
    }

private:
    FlyLandingCase m_case;
};

const ScreenEvents NOTHING = {};
const ScreenEvents BLACK = {ScreenEvent::FADE_BLACK};
const ScreenEvents WHITE = {ScreenEvent::FADE_WHITE};

const FlyLandingCase CASES[] = {
    {
        .input = {"the black band and a quiet Fuchsia, no fade: not landed", repeated(NOTHING, 15), Direction::SOUTH},
        .expectation = {false, false},
    },
    {
        .input = {"a fade then quiet, the player on screen: landed", joined(repeated(BLACK, 5), repeated(NOTHING, 12)), Direction::SOUTH},
        .expectation = {true, true},
    },
    {
        .input = {"a white fade then quiet, the player on screen: landed", joined(repeated(WHITE, 5), repeated(NOTHING, 12)), Direction::SOUTH},
        .expectation = {true, true},
    },
    {
        .input = {"a fade then quiet, no readable facing: not landed", joined(repeated(BLACK, 5), repeated(NOTHING, 12)), std::nullopt},
        .expectation = {false, true},
    },
    {
        .input = {"a fade, quiet for less than AFTER_BATTLE_QUIET: not landed", joined(repeated(BLACK, 5), repeated(NOTHING, 4)), Direction::SOUTH},
        .expectation = {false, false},
    },
    {
        .input = {"still faded: not landed", repeated(BLACK, 20), Direction::SOUTH},
        .expectation = {false, false},
    },
};


//  The run's frames, through the observer and the facing reader.
//  Fixtures: PokemonFRLG/AutoStoryFlyLanding/ in CommandLineTests.
constexpr std::string_view BAND_FRAME = "PokemonFRLG/AutoStoryFlyLanding/Fuchsia_FlightBand.png";
constexpr std::string_view FUCHSIA_FRAME = "PokemonFRLG/AutoStoryFlyLanding/Fuchsia_BeforeFade.png";
constexpr std::string_view PALLET_FRAME = "PokemonFRLG/AutoStoryFlyLanding/PalletTown_Standing.png";

//  The look FlyLanding judges for `screen` shown for `frames` frames after
//  `before` frames of black, as the observer reads it.
class Test_AutoStoryFlyLandingFrames : public ImageUnitTest{
public:
    Test_AutoStoryFlyLandingFrames()
        : ImageUnitTest(
            "PokemonFRLG::AutoStoryFlyLanding - the 23:59 frames",
            {std::string(BAND_FRAME), std::string(FUCHSIA_FRAME), std::string(PALLET_FRAME)}
        )
    {}

protected:
    virtual UnitTestResult run_on_images(Logger&, CancellableScope&, const std::vector<ImageRGB32>& images) const override{
        FrlgScreenObserver observer;
        const auto events_of = [&](const ImageRGB32& image){
            return observer.observe(VideoSnapshot(image.copy(), WallClock()), WallClock());
        };
        const auto facing_of = [&](const ImageRGB32& image){
            //  The run's player: whichever sheet reads the frame.
            const std::optional<Direction> girl = read_player_facing(image, PlayerGender::GIRL);
            return girl ? girl : read_player_facing(image, PlayerGender::BOY);
        };

        //  Each frame is quiet; only the Pallet frame, after a fade, has a player to read.
        for (size_t c = 0; c < 3; c++){
            if (events_of(images[c]) != NOTHING){
                return std::format("frame {} shows {}, not nothing", c, Pokemon::describe(events_of(images[c])));
            }
        }
        if (facing_of(images[0])){
            return "the flight band's frame reads a facing";
        }
        if (!facing_of(images[2])){
            return "the Pallet frame's facing is not readable";
        }

        //  The run's order: the band and Fuchsia are quiet for over a
        //  second with no fade (the logged "landed in PalletTown (quiet
        //  1.1 s)"): not landed, whatever the facing reads.
        const std::vector<ScriptedFrame> before_fade = repeated(NOTHING, 12);
        if (judge_fly_landing(scripted_look(before_fade), QUIET, [&]{ return facing_of(images[1]); }).landed()){
            return "Fuchsia before the fade counts as landed";
        }
        //  The fade, then the Pallet frame.
        const std::vector<ScriptedFrame> arrival = joined(joined(before_fade, repeated(BLACK, 6)), repeated(NOTHING, 12));
        if (!judge_fly_landing(scripted_look(arrival), QUIET, [&]{ return facing_of(images[2]); }).landed()){
            return "Pallet Town after the fade is not landed";
        }
        return true;
    }
};

//  The 02:42 and 02:46 runs (console B, 2026-11-13, 12.03): the fly map's
//  close is a fade too, and Fuchsia after it is quiet for 2.6 s with the
//  player on screen. Both runs logged "landed in PalletTown" on that frame,
//  then walked during Pallet's fade-in (the player not shown yet).
constexpr std::string_view FLY_OFF_FRAME = "PokemonFRLG/AutoStoryFlyLanding/Fuchsia_FlyOff.png";
constexpr std::string_view PALLET_FADING_FRAME = "PokemonFRLG/AutoStoryFlyLanding/PalletTown_FadingIn.png";

class Test_AutoStoryFlyLandingFlyOff : public ImageUnitTest{
public:
    Test_AutoStoryFlyLandingFlyOff()
        : ImageUnitTest(
            "PokemonFRLG::AutoStoryFlyLanding - the 02:46 frames",
            {std::string(FLY_OFF_FRAME), std::string(PALLET_FADING_FRAME), std::string(PALLET_FRAME)}
        )
    {}

protected:
    virtual UnitTestResult run_on_images(Logger&, CancellableScope&, const std::vector<ImageRGB32>& images) const override{
        const auto facing_of = [&](const ImageRGB32& image){
            const std::optional<Direction> girl = read_player_facing(image, PlayerGender::GIRL);
            return girl ? girl : read_player_facing(image, PlayerGender::BOY);
        };
        //  The fly map's fade, then Fuchsia quiet for 2.6 s.
        const std::vector<ScriptedFrame> fly_off = joined(repeated(BLACK, 5), repeated(NOTHING, 26));
        if (judge_fly_landing(scripted_look(fly_off), QUIET, [&]{ return facing_of(images[0]); }).landed()){
            return "Fuchsia after the fly map's fade counts as landed";
        }
        return true;
    }
};

} // namespace


void add_tests_AutoStoryFlyLanding(UnitTestDatabase& database){
    for (const FlyLandingCase& test_case : CASES){
        database.add<Test_AutoStoryFlyLanding>(test_case);
    }
    database.add<Test_AutoStoryFlyLandingFrames>();
    database.add<Test_AutoStoryFlyLandingFlyOff>();
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
