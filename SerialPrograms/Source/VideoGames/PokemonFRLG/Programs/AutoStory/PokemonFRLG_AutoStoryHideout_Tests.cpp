/*  AutoStory Rocket Hideout Steps Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <string>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_MovementMechanics.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMaps.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldSim.h"
#include "PokemonFRLG_AutoStoryHideout.h"
#include "PokemonFRLG_AutoStoryRowSim.h"
#include "PokemonFRLG_AutoStoryTableSteps.h"

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
        const std::optional<LiftRide> ride = parse_lift_floor_variant(m_case.input.variant);
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
        if (!is_hideout_variant(m_case.input.variant)){
            return "A ride that isn't a hideout variant.";
        }
        return true;
    }

private:
    LiftCase m_case;
};

UnitTestResult test_variants(){
    if (!is_hideout_variant(POSTER_SWITCH_VARIANT) || is_hideout_variant("cut_tree") || is_hideout_variant("")){
        return "Expected only the poster switch and lift rides as hideout variants.";
    }
    if (lift_floor_variant("B2F", "B4F") != "lift_floor B2F B4F"){
        return "Expected \"lift_floor B2F B4F\", got \"" + lift_floor_variant("B2F", "B4F") + "\".";
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

std::string poster_on_sim(){
    const MapData room = parse_map(R"({
        "name": "Corner", "source": {"decomp_file": "", "revision": "test", "date": "2026-11-09"},
        "tiles": ["#####", "#...#", "#...#", "#####"],
        "objects": [
            {"name": "poster", "kind": "sign", "x": 2, "y": 1},
            {"name": "poster_stairs", "kind": "npc", "x": 3, "y": 1}
        ]
    })", "Corner");
    OverworldSim sim({room}, MapPosition{"Corner", {2, 2}}, Direction::NORTH);
    StoryPose at{"Corner", {2, 2}, Direction::NORTH};
    const std::string failure = row_step_on_sim(sim, at, StoryTable::poster_switch("poster_switch", "test"), "00.31");
    if (!failure.empty()){
        return failure;
    }
    const Milliseconds tile = MovementMechanics::TILE_TIME;
    sim.hold(Direction::EAST, tile * 2, MovementMechanics::STEP_RELEASE, tile * 2, {});
    sim.hold(Direction::NORTH, tile * 2, MovementMechanics::STEP_RELEASE, tile * 2, {});
    if (!(sim.state().position == MapPosition{"Corner", {3, 1}})){
        return "After the poster step, walked onto " + to_string(sim.state().position) + ", not the stairs.";
    }
    return "";
}

std::string lift_on_sim(){
    std::vector<MapData> maps;
    maps.push_back(parse_map(R"({
        "name": "Lift", "source": {"decomp_file": "", "revision": "test", "date": "2026-11-09"},
        "tiles": ["###", "#.#", "#.#", "###"],
        "warps": [{"x": 1, "y": 2, "enter": "walk_south"}]
    })", "Lift"));
    maps.push_back(parse_map(R"({
        "name": "B4F", "source": {"decomp_file": "", "revision": "test", "date": "2026-11-09"},
        "tiles": ["###", "#.#", "###"]
    })", "B4F"));
    SimScript script;
    const MapPosition b4f{"B4F", {1, 1}};
    script.lift_floors["Lift"] = {MapPosition{"Lift", {1, 1}}, MapPosition{"Lift", {1, 1}}, b4f};
    OverworldSim sim(maps, MapPosition{"Lift", {1, 2}}, Direction::SOUTH, script);
    StoryPose at{"Lift", {1, 2}, Direction::SOUTH};
    const std::string failure = row_step_on_sim(sim, at, StoryTable::lift_floor("lift_to_b4f", "B2F", "B4F", "test"), "00.32");
    if (!failure.empty()){
        return failure;
    }
    sim.hold(Direction::SOUTH, MovementMechanics::TILE_TIME, MovementMechanics::STEP_RELEASE, std::chrono::milliseconds(3000), {});
    if (!(sim.state().position == b4f)){
        return "After the lift step, the door let out on " + to_string(sim.state().position) + ".";
    }
    return "";
}

struct AutoStoryHideoutBodyInput{
    std::string name;
    UnitTestResult (*body)();
};
struct AutoStoryHideoutBodyExpectation{
    const char* description;
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
