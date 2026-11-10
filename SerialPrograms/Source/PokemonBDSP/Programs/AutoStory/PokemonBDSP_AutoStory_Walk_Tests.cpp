/*  BDSP AutoStory Walk Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "Utils/PokemonBDSP_AutoStory_Indoors.h"
#include "Utils/PokemonBDSP_AutoStory_LegBattle.h"
#include "Utils/PokemonBDSP_AutoStory_Location.h"
#include "Utils/PokemonBDSP_AutoStory_RouteWalk.h"
#include "Utils/PokemonBDSP_AutoStory_Walk.h"
#include "Actions/PokemonBDSP_AutoStory_Talks.h"
#include "PokemonBDSP_AutoStory_StoryPlaces.h"
#include "PokemonBDSP_AutoStory_StoryStepIO.h"
#include "PokemonBDSP_AutoStory_StoryWalkSteps.h"
#include "PokemonBDSP_AutoStory_Walk_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


namespace{

//  A turn must not step: on the console D-pad taps of 40 to 80 ms only
//  turned, a 160 ms D-pad tap stepped a whole tile, and a 100 ms stick tap
//  stepped about 0.6 tile, off the tile grid (r49, r248a). So route_turn()
//  taps the D-pad, once, for at most 80 ms.
UnitTestResult test_route_turn_tap(Logger&){
        constexpr Milliseconds LONGEST_TURN(80);
        for (Pokemon::Direction direction : {Pokemon::Direction::NORTH, Pokemon::Direction::SOUTH, Pokemon::Direction::EAST, Pokemon::Direction::WEST}){
            const RouteInput tap = route_turn_tap(direction);
            const std::string name = std::string("turning ") + Pokemon::direction_name(direction);
            if (tap.kind != RouteInputKind::DPAD){
                return name + ": not a D-pad tap; a stick tap moves part of a tile.";
            }
            if (tap.direction != direction || tap.count != 1){
                return name + ": expected one tap that way.";
            }
            if (tap.hold > LONGEST_TURN){
                return name + ": a " + std::to_string(tap.hold.count()) + " ms tap; over 80 ms may step.";
            }
        }
        if (ROUTE_TURN_TAP > LONGEST_TURN){
            return "ROUTE_TURN_TAP is " + std::to_string(ROUTE_TURN_TAP.count()) + " ms; over 80 ms may step.";
        }
        return true;
}

//  WarpFadeWatch on synthetic frames 33 ms apart, each a brightness and a
//  texture (stddev a channel). Two scenes differ here when their
//  brightness does.
struct FadeFrame{
    double brightness;
    double stddev;
};
struct WarpFadeCase{
    std::string name;
    WarpFadeRule rule;
    std::vector<FadeFrame> frames;
    //  The frame (1-based) the fade shows on, 0 for none, and how.
    size_t seen_at;
    WarpFade fade;
};

UnitTestResult test_warp_fade_watch(Logger&){
    constexpr WarpFadeRule DOOR{.partial_dark = true, .dark_for = Milliseconds(50), .white_for = Milliseconds(60)};
    constexpr WarpFadeRule TAP{};
    constexpr FadeFrame STREET{104, 20};
    constexpr FadeFrame ROOM{130, 20};
    constexpr FadeFrame FALL{55, 10};
    constexpr FadeFrame BLACK{7, 1};
    const std::vector<WarpFadeCase> cases{
        {.name = "a door's fade, its hold seen", .rule = DOOR, .frames = {STREET, FALL, BLACK, BLACK, BLACK, ROOM}, .seen_at = 4, .fade = WarpFade::BLACK},
        {.name = "a tap's fade, its hold seen", .rule = TAP, .frames = {STREET, FALL, BLACK, ROOM}, .seen_at = 3, .fade = WarpFade::BLACK},
        {.name = "a short fade, its hold between two frames", .rule = TAP, .frames = {STREET, FALL, ROOM}, .seen_at = 3, .fade = WarpFade::BLACK},
        {.name = "a door's short fade, its hold between two frames", .rule = DOOR, .frames = {STREET, FALL, ROOM}, .seen_at = 3, .fade = WarpFade::BLACK},
        {.name = "a dip back to the same street", .rule = TAP, .frames = {STREET, FALL, STREET}, .seen_at = 0, .fade = WarpFade::NONE},
        {.name = "a dim cave floor (46) is no fade", .rule = TAP, .frames = {STREET, {46, 20}, {46, 20}, {46, 20}}, .seen_at = 0, .fade = WarpFade::NONE},
        {.name = "a cave mouth's dim fade (55)", .rule = TAP, .frames = {STREET, {80, 10}, {55, 1}, ROOM}, .seen_at = 3, .fade = WarpFade::BLACK},
        {.name = "a cave exit's white, its hold between two frames", .rule = TAP, .frames = {STREET, {180, 10}, ROOM}, .seen_at = 3, .fade = WarpFade::WHITE},
        {.name = "a cave exit's white", .rule = DOOR, .frames = {STREET, {250, 1}, {250, 1}, {250, 1}}, .seen_at = 4, .fade = WarpFade::WHITE},
    };
    for (const WarpFadeCase& item : cases){
        WarpFadeWatch watch(item.rule, [](const Pokemon::TransitionSample& before, const Pokemon::TransitionSample& after){
            return after.middle.average.r != before.middle.average.r ? Pokemon::SceneChange::DIFFERENT : Pokemon::SceneChange::SAME;
        });
        size_t seen_at = 0;
        WarpFade fade = WarpFade::NONE;
        for (size_t i = 0; i < item.frames.size() && seen_at == 0; i++){
            const FadeFrame& frame = item.frames[i];
            fade = watch.push({
                .time = WallClock() + Milliseconds(33 * (int64_t)i),
                .middle = ImageStats(FloatPixel(frame.brightness, frame.brightness, frame.brightness), FloatPixel(frame.stddev, frame.stddev, frame.stddev), 1),
            });
            seen_at = fade == WarpFade::NONE ? 0 : i + 1;
        }
        if (seen_at != item.seen_at || fade != item.fade){
            return item.name + ": " + warp_fade_name(fade) + " at frame " + std::to_string(seen_at) + ", expected " +
                warp_fade_name(item.fade) + " at " + std::to_string(item.seen_at) + ".";
        }
    }
    return true;
}

//  indoors() decides when leave_door_south() is out: r257 walked on south
//  past Sandgem's road into a house after leaving the Center, because the
//  evening tint made the outside as warm as a room. Synthetic screens: the
//  evening outside (warm all over), a room (warm, black along the bottom),
//  a fade (black all over), and warm with a thin dark edge (a shadow).
struct IndoorsCase{
    std::string name;
    uint32_t fill;
    double dark_bottom;     //  how much of the screen's height, from the bottom, is black
    bool indoors;
};

UnitTestResult test_indoors_any_light(Logger&){
        const std::vector<IndoorsCase> cases{
            {.name = "evening outside, orange all over", .fill = 0xffe0a040, .dark_bottom = 0, .indoors = false},
            {.name = "a room: orange, black below its floor", .fill = 0xffe0a040, .dark_bottom = 0.10, .indoors = true},
            {.name = "a fade: black all over", .fill = 0xff000000, .dark_bottom = 0, .indoors = false},
            {.name = "orange with a thin shadow at the bottom", .fill = 0xffe0a040, .dark_bottom = 0.01, .indoors = false},
            {.name = "daytime grass", .fill = 0xff60b040, .dark_bottom = 0.10, .indoors = false},
        };
        for (const IndoorsCase& item : cases){
            ImageRGB32 screen(1920, 1080);
            const size_t dark_from = (size_t)(1080 * (1 - item.dark_bottom));
            for (size_t y = 0; y < 1080; y++){
                for (size_t x = 0; x < 1920; x++){
                    screen.pixel(x, y) = y >= dark_from ? 0xff000000 : item.fill;
                }
            }
            if (indoors(screen) != item.indoors){
                return item.name + (item.indoors ? ": read as outside." : ": read as inside.");
            }
        }
        return true;
}

//  door_fade_look(): out of a door is a dark look, then a bright one that is
//  not a room. r259 turned round in Sandgem's lit Center, read "not a room"
//  once at full brightness, and walked east inside it as if outside.
struct DoorLook{
    double brightness;
    bool room;
};
struct DoorCase{
    std::string name;
    std::vector<DoorLook> looks;
    //  The look (1-based) that reads out, or 0 for none.
    size_t out_at;
};

UnitTestResult test_door_fade(Logger&){
        const std::vector<DoorCase> cases{
            {.name = "r259: a bright look in the Center that misses the room", .looks = {{458, true}, {440, false}, {457, true}}, .out_at = 0},
            {.name = "the fade, then the town", .looks = {{458, true}, {300, true}, {60, false}, {310, false}}, .out_at = 4},
            {.name = "the fade, then the room again", .looks = {{458, true}, {60, false}, {420, true}}, .out_at = 0},
            {.name = "the fade, still dim", .looks = {{60, false}, {150, false}, {320, false}}, .out_at = 3},
        };
        for (const DoorCase& item : cases){
            DoorFadeWatch watch;
            size_t out_at = 0;
            for (size_t i = 0; i < item.looks.size() && out_at == 0; i++){
                if (door_fade_look(watch, item.looks[i].brightness, item.looks[i].room)){
                    out_at = i + 1;
                }
            }
            if (out_at != item.out_at){
                return item.name + ": out at look " + std::to_string(out_at) + ", expected " + std::to_string(item.out_at) + ".";
            }
        }
        return true;
}

//  A fade is uniform (fade_of(): the shared watchers' stddev test), dark or
//  white; a floor is textured. The Oreburgh Gate's floor, about 46, is no
//  fade: read as one, the tap into the Gate from Oreburgh, already
//  through, waited 8 s for a banner gone (lab-B, 2026-11-08 15:31 to
//  15:54). A cave mouth's dim fade (45 to 64 measured) is one. The stddevs
//  are a model; the Gate's frames on the console are to check them.
UnitTestResult test_fade_of(Logger&){
    struct Case{
        std::string name;
        double brightness;
        double stddev;
        WarpFade fade;
    };
    const std::vector<Case> cases{
        {.name = "inside the Gate, its floor", .brightness = 46, .stddev = 20, .fade = WarpFade::NONE},
        {.name = "Oreburgh's street at night", .brightness = 88, .stddev = 20, .fade = WarpFade::NONE},
        {.name = "a door's black", .brightness = 7, .stddev = 1, .fade = WarpFade::BLACK},
        {.name = "a cave mouth's dim fade", .brightness = 64, .stddev = 2, .fade = WarpFade::BLACK},
        {.name = "a grey screen, neither dark nor white", .brightness = 120, .stddev = 1, .fade = WarpFade::NONE},
        {.name = "a cave exit's white", .brightness = 250, .stddev = 1, .fade = WarpFade::WHITE},
        {.name = "a bright textured screen", .brightness = 250, .stddev = 20, .fade = WarpFade::NONE},
    };
    for (const Case& item : cases){
        const ImageStats stats(FloatPixel(item.brightness, item.brightness, item.brightness), FloatPixel(item.stddev, item.stddev, item.stddev), 1);
        const WarpFade fade = fade_of(stats);
        if (fade != item.fade){
            return item.name + ": " + warp_fade_name(fade) + ", expected " + warp_fade_name(item.fade) + ".";
        }
    }
    return true;
}


UnitTestResult test_location_banner(Logger&){
        struct Case{
            const char* read;
            const char* expected;
            bool match;
        };
        const Case cases[] = {
            {"sandgemtown",     "Sandgem Town",     true},
            {"andgemltown",     "Sandgem Town",     true},     //  r109's OCR
            {"Route 202",       "Route 202",        true},
            {"route2o2",        "Route 202",        false},    //  a digit read as a letter is not trusted
            {"route201",        "Route 202",        false},
            {"jubilifecity",    "Sandgem Town",     false},
            {"twinleaftown",    "Sandgem Town",     false},
            {"",                "Sandgem Town",     false},
        };
        for (const Case& c : cases){
            if (location_banner_matches(c.read, c.expected) != c.match){
                return std::string("Banner \"") + c.read + "\" for " + c.expected +
                    (c.match ? " should match." : " should not match.");
            }
        }
        return true;
}


UnitTestResult test_indoors(Logger& logger){
        struct Case{ const char* file; bool inside; };
        const Case cases[] = {
            {"PokemonBDSP/AutoStory/JubilifeCenterInside-20261030-c6.png", true},
            {"PokemonBDSP/AutoStory/JubilifeCenterInside-20261030-c4.png", true},
            {"PokemonBDSP/AutoStory/JubilifeCenterInsideUpper-20261030-r151a.png", true},
            {"PokemonBDSP/AutoStory/SandgemCenterInside-20261030-053555.png", true},
            {"PokemonBDSP/AutoStory/JubilifeSchoolInside-20261030.png", true},
            {"PokemonBDSP/AutoStory/JubilifeCenterDoorstep-20261030-c5.png", false},
            {"PokemonBDSP/AutoStory/JubilifeSchoolDoorstep-20261030.png", false},
            {"PokemonBDSP/AutoStory/JubilifeSchoolDoorstep2-20261030.png", false},
            {"PokemonBDSP/AutoStory/JubilifeCenterFrontDoor-20261030-q2.png", false},
            {"PokemonBDSP/AutoStory/SandgemCenterBelowSign-20261030-053527.png", false},
            {"PokemonBDSP/AutoStory/SandgemMartDoorUpRight-20261030-023443.png", false},
        };
        for (const Case& c : cases){
            const std::string path = UNIT_TEST_RESOURCE_PATH() + c.file;
            if (!Filesystem::is_regular_file(path)){
                return UnitTestResult(UnitTestResult::SKIPPED, "Screenshot not found: " + path);
            }
            const bool inside = indoors(ImageRGB32(path));
            logger.log(std::string(c.file) + ": " + (inside ? "indoors" : "outside"));
            if (inside != c.inside){
                return std::string(c.file) + ": expected " + (c.inside ? "indoors" : "outside") + ".";
            }
        }
        return true;
}


UnitTestResult test_location_banner_showing(Logger& logger){
        struct Case{ const char* file; bool banner; };
        const Case cases[] = {
            {"PokemonBDSP/AutoStory/JubilifeCenterDoorstep-20261030-c5.png", true},
            {"PokemonBDSP/AutoStory/JubilifeSchoolDoorstep-20261030.png", true},
            {"PokemonBDSP/AutoStory/JubilifeCenterInside-20261030-c6.png", false},
            {"PokemonBDSP/AutoStory/JubilifeCenterInside-20261030-c4.png", false},
            {"PokemonBDSP/AutoStory/JubilifeCenterInsideUpper-20261030-r151a.png", false},
            {"PokemonBDSP/AutoStory/JubilifeCenterFrontDoor-20261030-q2.png", false},
        };
        for (const Case& c : cases){
            const std::string path = UNIT_TEST_RESOURCE_PATH() + c.file;
            if (!Filesystem::is_regular_file(path)){
                return UnitTestResult(UnitTestResult::SKIPPED, "Screenshot not found: " + path);
            }
            const bool banner = location_banner_showing(ImageRGB32(path));
            logger.log(std::string(c.file) + ": banner " + (banner ? "shows" : "none"));
            if (banner != c.banner){
                return std::string(c.file) + ": expected " + (c.banner ? "a banner" : "no banner") + ".";
            }
        }
        return true;
}



//  pokemon_center_on_screen(): the Center's warm floor in a room. r79 read
//  Jubilife's yellow plaza as a Center and called a won battle lost.
UnitTestResult test_pokemon_center_on_screen(Logger&){
    constexpr uint32_t FLOOR = 0xfff0c860;      //  the Center's warm yellow
    constexpr uint32_t WALL = 0xffe08030;       //  its orange back wall
    constexpr uint32_t GRASS = 0xff60b040;
    struct Case{
        std::string name;
        uint32_t top;
        uint32_t bottom;
        double dark_bottom;     //  how much of the screen's height, from the bottom, is black
        bool center;
    };
    const std::vector<Case> cases{
        {"a Center: wall over floor, black below it", WALL, FLOOR, 0.15, true},
        {"Jubilife's plaza: yellow paving, no black edge", FLOOR, FLOOR, 0, false},
        {"a street: grass over yellow paving", GRASS, FLOOR, 0, false},
        {"a grey gym in a room", 0xff808080, 0xff909090, 0.15, false},
    };
    for (const Case& item : cases){
        ImageRGB32 screen(1920, 1080);
        const size_t dark_from = (size_t)(1080 * (1 - item.dark_bottom));
        for (size_t y = 0; y < 1080; y++){
            for (size_t x = 0; x < 1920; x++){
                screen.pixel(x, y) = y >= dark_from ? 0xff000000 : y < 540 ? item.top : item.bottom;
            }
        }
        if (pokemon_center_on_screen(screen) != item.center){
            return item.name + (item.center ? ": not read as a Center." : ": read as a Center.");
        }
    }
    return true;
}

//  01.02's walk east beats the Youngster above Route 203's row 18; the rows
//  after it, from saves made since, do not meet him again.
UnitTestResult test_row_trainers(Logger&){
    const StoryWorld* world = story_plan_world("Route203", "JubilifeCity");
    if (world == nullptr){
        return std::string("no world joins Route 203 and Jubilife");
    }
    const auto has_youngster = [world](const std::string& row){
        const std::vector<TrainerSight> sights = row_trainers(*world, row);
        return std::any_of(sights.begin(), sights.end(), [](const TrainerSight& sight){
            return sight.name == "Youngster (above row 18)";
        });
    };
    if (!has_youngster("01.02")){
        return std::string("01.02 fights the Youngster: he must be on its trainers");
    }
    if (has_youngster("01.06")){
        return std::string("01.06 starts from a save after 01.02 beat the Youngster");
    }
    return true;
}

struct AutoStoryWalkInput{
    std::string name;
    std::function<UnitTestResult(Logger&)> body;
};

struct AutoStoryWalkExpectation{
};

struct AutoStoryWalkCase{
    AutoStoryWalkInput input;
    AutoStoryWalkExpectation expectation;
};

//  place_leg_battle() on synthetic walks of 10 taps, their events recorded
//  as record_tap_battle() records the console's.
struct LegBattleCase{
    std::string name;
    size_t battles;
    size_t tap;             //  the tap (from 1) that set the battle off
    size_t moved_after;     //  taps that moved after it was run from
    bool settled;           //  false: a white fade with no battle yet
    std::optional<TileSpan> expected;
};
UnitTestResult test_leg_battle_place(Logger& logger){
    const LegBattleCase CASES[] = {
        {"mid-leg, the taps went on", 1, 3, 7, true, TileSpan{9, 10}},
        {"seen after the hold", 1, 6, 0, true, TileSpan{5, 7}},
        {"on the first tap", 1, 1, 0, true, TileSpan{0, 2}},
        {"before the walk", 1, 0, 0, true, TileSpan{0, 1}},
        {"two battles", 2, 3, 0, true, std::nullopt},
        {"unsettled: a white fade, no battle yet", 1, 4, 0, false, std::nullopt},
    };
    for (const LegBattleCase& item : CASES){
        WalkResult walk{.stop = WalkStop::LIMIT, .walked = Milliseconds(8000), .wild_battles = item.battles};
        walk.moved_after_battle = item.moved_after;
        walk.events.record({}, walk.started);
        if (item.settled){
            record_tap_battle(walk, item.tap);
        }else{
            walk.events.record({Pokemon::ScreenEvent::FADE_WHITE}, walk.started + Milliseconds(800 * (item.tap - 1) + 500));
        }
        const LegBattlePlace place = place_leg_battle(walk, 10);
        logger.log(leg_battle_line(item.name, walk, place));
        const bool same = place.moved.has_value() == item.expected.has_value() &&
            (!place.moved || (place.moved->min == item.expected->min && place.moved->max == item.expected->max));
        if (!same){
            return item.name + ": " + leg_battle_line(item.name, walk, place);
        }
    }
    return true;
}

class Test_AutoStoryWalk : public UnitTest{
public:
    explicit Test_AutoStoryWalk(AutoStoryWalkCase test_case)
        : UnitTest("PokemonBDSP::AutoStory - " + test_case.input.name)
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        try{
            return m_case.input.body(logger);
        }catch (Exception& e){
            return std::string(e.name()) + ": " + e.message();
        }
    }

private:
    AutoStoryWalkCase m_case;
};

} // namespace


const AutoStoryWalkCase AUTO_STORY_WALK_CASES[] = {
    {
        .input = {"a turn is a D-pad tap too short to step", test_route_turn_tap},
        .expectation = {},
    },
    {
        .input = {"indoors() under the evening tint", test_indoors_any_light},
        .expectation = {},
    },
    {
        .input = {"out of a door needs its fade", test_door_fade},
        .expectation = {},
    },
    {
        .input = {"a fade is uniform, dark or white; a dim floor is none (lab-B, 2026-11-08)", test_fade_of},
        .expectation = {},
    },
    {
        .input = {"a warp's fade, followed frame by frame, its hold seen or not", test_warp_fade_watch},
        .expectation = {},
    },
    {
        .input = {"location banner match", test_location_banner},
        .expectation = {},
    },
    {
        .input = {"indoors", test_indoors},
        .expectation = {},
    },
    {
        .input = {"location banner showing", test_location_banner_showing},
        .expectation = {},
    },
    {
        .input = {"a Pokemon Center is its floor in a room, not a yellow plaza", test_pokemon_center_on_screen},
        .expectation = {},
    },
    {
        .input = {"a wild battle on an open leg's taps, placed by its screen events", test_leg_battle_place},
        .expectation = {},
    },
    {
        .input = {"a row's trainers leave out those an earlier row beat", test_row_trainers},
        .expectation = {},
    },
};

void add_tests_AutoStory_Walk(UnitTestDatabase& database){
    for (const auto& item : AUTO_STORY_WALK_CASES){
        database.add<Test_AutoStoryWalk>(item);
    }
}


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
