/*  AutoStory Rocket Hideout Steps Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <string>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_CodedRooms.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_HideoutLiftRoom.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_MovementMechanics.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMaps.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldSim.h"
#include "PokemonFRLG_AutoStoryHideout.h"
#include "PokemonFRLG_AutoStoryHideoutLift.h"
#include "PokemonFRLG_AutoStoryRowSim.h"
#include "PokemonFRLG_AutoStoryTableSteps.h"
#include "PokemonFRLG_AutoStoryFieldSteps.h"
#include "PokemonFRLG_AutoStoryVariantWords.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldSpin.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

struct LiftInput{
    std::string name;
    std::string variant;
};
struct LiftExpectation{
    //  Nothing: the variant isn't a ride.
    std::optional<MenuPresses> presses;
};
struct LiftCase{
    LiftInput input;
    LiftExpectation expectation;
};

class Test_AutoStoryHideout : public UnitTest{
public:
    explicit Test_AutoStoryHideout(LiftCase test_case)
        : UnitTest("PokemonFRLG::AutoStoryHideout - " + test_case.input.name)
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::string_view floors[] = {"from", "to"};
        const StoryStep step = legacy_game_step(m_case.input.variant, floors);
        const std::optional<LiftRide> ride = parse_lift_floor_step(step);
        const std::optional<MenuPresses>& expected = m_case.expectation.presses;
        if (!ride || !expected){
            if (ride.has_value() != expected.has_value()){
                return std::format("\"{}\": a ride {}, expected {}.", m_case.input.variant, ride.has_value(), expected.has_value());
            }
            return true;
        }
        const MenuPresses got = lift_menu_presses(*ride, rocket_hideout_lift_menu().size());
        if (got.direction != expected->direction || got.presses != expected->presses){
            return std::format(
                "\"{}\": {} press(es) {}, expected {} {}.", m_case.input.variant,
                got.presses, Pokemon::direction_name(got.direction), expected->presses, Pokemon::direction_name(expected->direction)
            );
        }
        if (!is_hideout_step(step)){
            return "A ride that isn't a hideout step.";
        }
        return true;
    }

private:
    LiftCase m_case;
};

UnitTestResult test_variants(){
    const auto game_step = [](std::string variant, Pokemon::StoryArgs args){
        StoryStep step;
        step.kind = Pokemon::StoryStepKind::GAME;
        step.variant = std::move(variant);
        step.args = std::move(args);
        return step;
    };
    if (!is_hideout_step(game_step(POSTER_SWITCH_VARIANT, {})) || is_hideout_step(game_step("cut_tree", {}))
        || is_hideout_step(game_step("", {})) || is_hideout_step(game_step(POSTER_SWITCH_VARIANT, {{"from", "B1F"}}))
    ){
        return "Expected only the poster switch and lift rides as hideout steps.";
    }
    const StoryStep ride = StoryTable::lift_floor("ride", "B2F", "B4F", "");
    const Pokemon::StoryArgs expected{{"from", "B2F"}, {"to", "B4F"}};
    if (ride.variant != LIFT_FLOOR_VARIANT || ride.args != expected){
        return "Expected lift_floor with {from: B2F, to: B4F}, got \"" + ride.variant + "\".";
    }
    //  A three-row menu doesn't wrap.
    const MenuPresses no_wrap = lift_menu_presses(LiftRide{0, 2}, 3);
    if (no_wrap.direction != Pokemon::Direction::SOUTH || no_wrap.presses != 2){
        return "Expected 2 down on a three-row menu.";
    }
    return true;
}

//  The table's steps, run as a row runs them on the sim: the poster's
//  switch opens the stairs north-east of it, and the lift lets out on the
//  floor picked.
std::string poster_on_sim();
std::string lift_on_sim();

UnitTestResult test_steps_on_sim(){
    const std::string poster = poster_on_sim();
    if (!poster.empty()){
        return poster;
    }
    const std::string lift = lift_on_sim();
    if (!lift.empty()){
        return lift;
    }
    return true;
}

//  The Game Corner with no poster or stairs in its map: the sim adds them
//  from code (add_coded_objects()). The room ends at x 16, so the walk
//  east ends on (15, 2), the stairs' top, only once the switch opens it.
std::string poster_on_sim(){
    std::string rows;
    for (int y = 0; y < 5; y++){
        rows += std::string(y == 0 ? "" : ", ") + (y == 0 || y == 4 ? "\"#################\"" : "\"#...............#\"");
    }
    const MapData room = parse_map(
        "{\"name\": \"CeladonCity_GameCorner\", \"source\": {\"decomp_file\": \"\", \"revision\": \"test\", \"date\": \"2026-11-09\"},"
        " \"tiles\": [" + rows + "]}",
        "CeladonCity_GameCorner"
    );
    const MapPosition below_poster{"CeladonCity_GameCorner", {GAME_CORNER_POSTER.x, GAME_CORNER_POSTER.y + 1}};
    OverworldSim sim({room}, below_poster, Direction::NORTH);
    StoryPose at{below_poster.map, below_poster.point, Direction::NORTH};
    const std::string failure = row_step_on_sim(sim, at, StoryTable::poster_switch("poster_switch", "test"), "00.31");
    if (!failure.empty()){
        return failure;
    }
    const Milliseconds tile = MovementMechanics::TILE_TIME;
    sim.hold(Direction::EAST, tile * 6, MovementMechanics::STEP_RELEASE, tile * 6, {});
    if (!(sim.state().position == MapPosition{"CeladonCity_GameCorner", {15, 2}})){
        return "After the poster step, walked onto " + to_string(sim.state().position) + ", not the stairs' (15, 2).";
    }
    return "";
}

//  A hideout floor of 32 by 26 open tiles with its lift door, which lets
//  into the lift as pokefirered's floors' warps do (warp 1 of the lift).
MapData hideout_floor(const std::string& name, const TilePos& door){
    std::string rows;
    for (int y = 0; y < 26; y++){
        std::string row(32, '.');
        if (y == door.y){
            row[door.x] = 'D';
        }
        rows += (y == 0 ? "\"" : ", \"") + row + "\"";
    }
    return parse_map(std::format(
        "{{\"name\": \"{}\", \"source\": {{\"decomp_file\": \"\", \"revision\": \"test\", \"date\": \"2026-11-09\"}},"
        " \"tiles\": [{}], \"warps\": [{{\"x\": {}, \"y\": {}, \"enter\": \"walk_into\", \"to_map\": \"{}\", \"to_point\": \"warp_1\"}}]}}",
        name, rows, door.x, door.y, LIFT_MAP
    ), name);
}

//  The ride from B2F to B4F with the lift's room coded, not mapped: from
//  below B2F's lift door, in, the panel, out onto B4F's lift door.
std::string lift_on_sim(){
    const std::vector<MapPosition>& doors = rocket_hideout_lift_doors();
    std::vector<MapData> maps{
        hideout_floor("RocketHideout_B2F", doors[1].point),
        hideout_floor("RocketHideout_B4F", doors[2].point),
        rocket_hideout_lift_room(),
    };
    SimScript script;
    script.lift_floors[std::string(LIFT_MAP)] = doors;
    const MapPosition below_door{"RocketHideout_B2F", doors[1].point.next(Direction::SOUTH)};
    OverworldSim sim(maps, below_door, Direction::NORTH, script);
    StoryPose at{below_door.map, below_door.point, Direction::NORTH};
    const std::string failure = row_step_on_sim(sim, at, StoryTable::lift_floor("lift_to_b4f", "B2F", "B4F", "test"), "00.32");
    if (!failure.empty()){
        return failure;
    }
    if (!(sim.state().position == doors[2])){
        return "After the lift step, the door let out on " + to_string(sim.state().position) + ".";
    }
    if (!sim.state().violations.empty()){
        return "The lift step broke the game's rules: " + sim.state().violations.front();
    }
    return "";
}

//  The hideout's spinners are coded: a map named as B2F or B3F slides the
//  player with no spin_* regions of its own.
UnitTestResult test_coded_spinners(){
    MapData b3f = parse_map(R"({
        "name": "RocketHideout_B3F", "source": {"decomp_file": "", "revision": "test", "date": "2026-11-09"},
        "tiles": ["##########", "#........#", "#........#", "##########"]
    })", "RocketHideout_B3F");
    if (spin_direction(b3f, TilePos{4, 9}) != Direction::WEST || !stops_spinning(b3f, TilePos{2, 9})){
        return "Expected B3F (4, 9) to slide west and (2, 9) to stop a slide.";
    }
    if (coded_spin_direction("RocketHideout_B2F", TilePos{3, 4}) != Direction::WEST
        || coded_spin_direction("RocketHideout_B2F", TilePos{9, 5}) != Direction::NORTH
        || !coded_spin_stop("RocketHideout_B2F", TilePos{1, 4})
        || coded_spin_direction("RocketHideout_B1F", TilePos{3, 4}).has_value()
    ){
        return "Expected B2F's arrows and stops from pokefirered, and none on B1F.";
    }
    return true;
}

struct AutoStoryHideoutBodyInput{
    std::string name;
    UnitTestResult (*body)();
};
struct AutoStoryHideoutBodyExpectation{
    std::string description;
};
struct AutoStoryHideoutBodyCase{
    AutoStoryHideoutBodyInput input;
    AutoStoryHideoutBodyExpectation expectation;
};

class Test_AutoStoryHideoutBody : public UnitTest{
public:
    explicit Test_AutoStoryHideoutBody(AutoStoryHideoutBodyCase test_case)
        : UnitTest("PokemonFRLG::AutoStoryHideout - " + test_case.input.name)
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return m_case.input.body();
    }

private:
    AutoStoryHideoutBodyCase m_case;
};

const AutoStoryHideoutBodyCase HIDEOUT_BODY_CASES[] = {
    {
        .input = {.name = "the poster and lift variants", .body = test_variants},
        .expectation = {.description = "only the poster switch and lift rides are hideout steps"},
    },
    {
        .input = {.name = "table steps on the sim", .body = test_steps_on_sim},
        .expectation = {.description = "the poster opens the stairs; the lift lets out on the floor picked"},
    },
    {
        .input = {.name = "coded spinners", .body = test_coded_spinners},
        .expectation = {.description = "B2F's and B3F's arrows come from code, not the map"},
    },
};

const LiftCase LIFT_CASES[] = {
    {
        .input = {.name = "B2F to B4F: one down", .variant = "lift_floor B2F B4F"},
        .expectation = {.presses = MenuPresses{Pokemon::Direction::SOUTH, 1}},
    },
    {
        //  Down past EXIT wraps to B1F: as few presses as up, and down is
        //  taken.
        .input = {.name = "B4F to B1F: two down past EXIT", .variant = "lift_floor B4F B1F"},
        .expectation = {.presses = MenuPresses{Pokemon::Direction::SOUTH, 2}},
    },
    {
        .input = {.name = "B4F to B2F: one up", .variant = "lift_floor B4F B2F"},
        .expectation = {.presses = MenuPresses{Pokemon::Direction::NORTH, 1}},
    },
    {
        //  B1F up past the top wraps to EXIT, then B4F: two up equal two
        //  down; down is taken.
        .input = {.name = "B1F to B4F: two down", .variant = "lift_floor B1F B4F"},
        .expectation = {.presses = MenuPresses{Pokemon::Direction::SOUTH, 2}},
    },
    {
        .input = {.name = "the lift's own floor: no press", .variant = "lift_floor B4F B4F"},
        .expectation = {.presses = MenuPresses{Pokemon::Direction::SOUTH, 0}},
    },
    {
        .input = {.name = "no B3F on the panel", .variant = "lift_floor B2F B3F"},
        .expectation = {.presses = std::nullopt},
    },
    {
        .input = {.name = "EXIT is no floor", .variant = "lift_floor B2F EXIT"},
        .expectation = {.presses = std::nullopt},
    },
    {
        .input = {.name = "one floor only", .variant = "lift_floor B2F"},
        .expectation = {.presses = std::nullopt},
    },
};

} // namespace


void add_tests_AutoStoryHideout(UnitTestDatabase& database){
    for (const LiftCase& item : LIFT_CASES){
        database.add<Test_AutoStoryHideout>(item);
    }
    for (const AutoStoryHideoutBodyCase& item : HIDEOUT_BODY_CASES){
        database.add<Test_AutoStoryHideoutBody>(item);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
