/*  AutoStory Fly Landing Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The 23:59 case (console B, 2026-11-12, 12.03) as scripted looks, and on
 *  the run's frames: the flight's black band, and Fuchsia's overworld
 *  before the fade, are quiet with nothing showing and are no landing; the
 *  Pallet Town frame after a fade is.
 *
 *  The 02:46 case (console B, 2026-11-13, 12.03): the fly map's close is a
 *  fade, and Fuchsia after it is quiet with the player on screen; it is no
 *  landing either, as the screen has not left the departure town.
 *
 */

#include <format>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Settings.h"
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
    //  Whether the screen has left the departure town, once asked.
    bool left = true;
};

struct FlyLandingExpectation{
    bool landed;
    //  Whether the facing was read at all.
    bool read_facing;
    //  Whether the departure town was compared at all.
    bool compared = true;
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
        bool compared = false;
        const FlyLanding got = judge_fly_landing(
            scripted_look(m_case.input.frames), QUIET,
            [&]{
                compared = true;
                return m_case.input.left;
            },
            [&]{
                read = true;
                return m_case.input.facing;
            }
        );
        const FlyLandingExpectation& want = m_case.expectation;
        if (got.landed() != want.landed || read != want.read_facing || compared != want.compared){
            return std::format(
                "expected landed {} (facing read {}, compared {}), got landed {} (facing read {}, compared {}): {}",
                want.landed, want.read_facing, want.compared, got.landed(), read, compared, describe(got)
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
        .expectation = {false, false, false},
    },
    {
        .input = {"a fade then quiet, still the departure town: not landed", joined(repeated(BLACK, 5), repeated(NOTHING, 26)), Direction::WEST, false},
        .expectation = {false, false, true},
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
        .expectation = {false, false, false},
    },
    {
        .input = {"still faded: not landed", repeated(BLACK, 20), Direction::SOUTH},
        .expectation = {false, false, false},
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
        const ImageFloatBox game = GameSettings::instance().GAME_BOX;
        const auto has_left = [&]{ return left_departure(images[1], images[2], game); };
        if (judge_fly_landing(scripted_look(before_fade), QUIET, has_left, [&]{ return facing_of(images[1]); }).landed()){
            return "Fuchsia before the fade counts as landed";
        }
        //  The fade, then the Pallet frame.
        const std::vector<ScriptedFrame> arrival = joined(joined(before_fade, repeated(BLACK, 6)), repeated(NOTHING, 12));
        if (!judge_fly_landing(scripted_look(arrival), QUIET, has_left, [&]{ return facing_of(images[2]); }).landed()){
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
            {std::string(FLY_OFF_FRAME), std::string(FUCHSIA_FRAME), std::string(PALLET_FADING_FRAME), std::string(PALLET_FRAME)}
        )
    {}

protected:
    virtual UnitTestResult run_on_images(Logger& logger, CancellableScope&, const std::vector<ImageRGB32>& images) const override{
        const ImageRGB32& departure = images[0];
        const ImageRGB32& bird = images[1];
        const ImageRGB32& fading = images[2];
        const ImageRGB32& pallet = images[3];
        const ImageFloatBox game = GameSettings::instance().GAME_BOX;
        const auto facing_of = [&](const ImageRGB32& image){
            const std::optional<Direction> girl = read_player_facing(image, PlayerGender::GIRL);
            return girl ? girl : read_player_facing(image, PlayerGender::BOY);
        };
        logger.log(std::format(
            "departure change: Fuchsia with the bird {:.1f}, Pallet fading in {:.1f}, Pallet standing {:.1f}",
            departure_change(departure, bird, game), departure_change(departure, fading, game), departure_change(departure, pallet, game)
        ));

        //  The departure town, with the Fly's bird over it, is not left;
        //  Pallet Town is, from its fade-in on.
        if (left_departure(departure, bird, game)){
            return "Fuchsia with the bird reads as another town";
        }
        if (!left_departure(departure, fading, game) || !left_departure(departure, pallet, game)){
            return "Pallet Town reads as the departure town";
        }
        //  Pallet fading in: no player yet, so no landing.
        if (facing_of(fading)){
            return "Pallet fading in reads a facing";
        }

        //  The fly map's fade, then Fuchsia quiet for 2.6 s: both runs'
        //  "landed in PalletTown". Not landed.
        const std::vector<ScriptedFrame> fly_off = joined(repeated(BLACK, 5), repeated(NOTHING, 26));
        const FlyLanding on_fuchsia = judge_fly_landing(
            scripted_look(fly_off), QUIET,
            [&]{ return left_departure(departure, departure, game); },
            [&]{ return facing_of(departure); }
        );
        if (on_fuchsia.landed()){
            return "Fuchsia after the fly map's fade counts as landed: " + describe(on_fuchsia);
        }
        //  Then the flight's fade and Pallet with the player standing: landed.
        const std::vector<ScriptedFrame> arrival = joined(joined(fly_off, repeated(BLACK, 6)), repeated(NOTHING, 12));
        const FlyLanding on_pallet = judge_fly_landing(
            scripted_look(arrival), QUIET,
            [&]{ return left_departure(departure, pallet, game); },
            [&]{ return facing_of(pallet); }
        );
        if (!on_pallet.landed()){
            return "Pallet Town after the flight's fade is not landed: " + describe(on_pallet);
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
