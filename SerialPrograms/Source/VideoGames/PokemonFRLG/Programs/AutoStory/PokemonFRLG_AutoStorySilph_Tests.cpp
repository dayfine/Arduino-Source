/*  AutoStory Silph Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Row 16.01 (StoryData/frlg-16.01.plan.json) on the overworld sim: from
 *  Saffron's nurse into Silph Co., up to 3F and through its pad to 5F.
 *
 */

#include <algorithm>
#include <format>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Pokemon_BuiltinStoryActions.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepExecutor.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldGatedRoute.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMaps.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldSim.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryRooms.h"
#include "PokemonFRLG_AutoStoryRowSim.h"
#include "PokemonFRLG_AutoStoryRows_Tests.h"
#include "PokemonFRLG_AutoStoryRows16_Tests.h"
#include "PokemonFRLG_AutoStoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

struct AutoStorySilphInput{
    std::string name;
    std::function<UnitTestResult()> body;
};

struct AutoStorySilphExpectation{
    std::string_view description;
};

struct AutoStorySilphCase{
    AutoStorySilphInput input;
    AutoStorySilphExpectation expectation;
};

class Test_AutoStorySilph : public UnitTest{
public:
    explicit Test_AutoStorySilph(AutoStorySilphCase test_case)
        : UnitTest(std::format("PokemonFRLG::AutoStory - Simulation: {}", test_case.input.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        try{
            return m_case.input.body();
        }catch (Exception& e){
            return std::format("{}: {}", e.name(), e.message());
        }
    }

private:
    AutoStorySilphCase m_case;
};

const StoryCheckpoint& checkpoint(std::string_view id){
    for (const StoryCheckpoint& row : story_checkpoints()){
        if (row.id == id){
            return row;
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("No checkpoint {}.", id));
}

//  16.01 on the sim from Saffron's nurse: Silph Co.'s stairs to 3F, west
//  along the hall and onto the pad to 5F, where the player stands on the pad
//  that leads back. No trainer is met and no rule is broken.
UnitTestResult test_pad_to_5f(){
    const StoryCheckpoint& row = checkpoint("16.01");
    StoryPose at = *story_pose(Pokemon::pose_of(*row.start));
    OverworldSim sim(story_maps(), MapPosition{at.map, at.tile}, *at.facing);
    std::vector<std::string> floors;
    for (const StoryStep& step : row.steps){
        if (const std::string failure = row_step_on_sim(sim, at, step, row.id); !failure.empty()){
            return std::format("16.01: ended on {}; {}", to_string(sim.state().position), failure);
        }
        floors.push_back(sim.state().position.map);
    }
    const MapPosition pad{"SilphCo_5F", {15, 7}};
    if (!(sim.state().position == pad)){
        return std::format("16.01: ended on {}, not on 5F's pad {}.", to_string(sim.state().position), to_string(pad));
    }
    if (floors != std::vector<std::string>{"SilphCo_3F", "SilphCo_5F"}){
        return "16.01: the steps ended on the wrong floors.";
    }
    if (!sim.state().trainers_beaten.empty() || !sim.state().violations.empty()){
        return "16.01: met a trainer or broke a rule on the way.";
    }
    return true;
}

//  The steps of row `id` walked on the sim over `maps`, from the row's start with
//  the Card Key and doors its start_flags and start_gates say.
class RowWalk{
public:
    RowWalk(std::string_view id, const std::vector<MapData>& maps)
        : m_row(checkpoint(id))
        , m_at(*story_pose(Pokemon::pose_of(*m_row.start)))
        , m_sim(maps, MapPosition{m_at.map, m_at.tile}, *m_at.facing, script_of(m_row))
    {}

    //  Each step in turn; the failure of the first that fails, or empty.
    std::string run(){
        for (const StoryStep& step : m_row.steps){
            //  The sim has no trigger scenes: a scripted battle (the rival's, Giovanni's)
            //  is taken as won where it leaves the player.
            if (step.kind == StoryStepKind::BATTLE){
                m_at = *story_pose(Pokemon::pose_of(*step.pose_after));
                m_floors.push_back(m_sim.state().position.map);
                continue;
            }
            if (const std::string failure = row_step_on_sim(m_sim, m_at, step, m_row.id); !failure.empty()){
                return failure;
            }
            m_floors.push_back(m_sim.state().position.map);
        }
        return "";
    }
    const StoryCheckpoint& row() const{ return m_row; }
    const OverworldSim& sim() const{ return m_sim; }
    const std::vector<std::string>& floors() const{ return m_floors; }

    //  What the row left out as a line, or empty: the sim's rule breaks and the
    //  trainers it fought, and the gates it has open against the ones the row's
    //  steps declare (gates_after).
    std::string problems() const{
        std::string text;
        if (!m_sim.state().violations.empty()){
            text += std::format(" broke a rule: {};", m_sim.state().violations.front());
        }
        const Pokemon::GateState declared = Pokemon::checkpoint_gates(m_row, m_row.steps.size());
        if (!(declared == m_sim.state().gates)){
            text += std::format(" the sim has {} gates open, the row's gates_after {};", m_sim.state().gates.opened.size(), declared.opened.size());
        }
        return text;
    }

private:
    static SimScript script_of(const StoryCheckpoint& row){
        SimScript script;
        script.card_key_held = std::find(row.start_flags.begin(), row.start_flags.end(), "card_key_held") != row.start_flags.end();
        script.gates = row.start_gates.value_or(Pokemon::GateState());
        return script;
    }

    const StoryCheckpoint& m_row;
    StoryPose m_at;
    OverworldSim m_sim;
    std::vector<std::string> m_floors;
};

std::string opened_doors(const OverworldSim& sim){
    std::string text;
    for (const std::string& key : sim.state().gates.opened){
        text += " " + key;
    }
    return text;
}

//  `maps` with the trainer at `at` on `map_name` beaten (his sight is gone, as in a
//  save made after his fight).
std::vector<MapData> with_beaten(std::vector<MapData> maps, std::string_view map_name, int x, int y){
    for (MapData& map : maps){
        for (MapObject& object : map.objects){
            if (map.name == map_name && object.x == x && object.y == y){
                object.trainer.reset();
            }
        }
    }
    return maps;
}

//  The maps a row after 16.02 starts on: the Rocket at 5F's (9, 21), beaten by
//  16.02 on its way, no longer sees the player (his fight is in the save).
std::vector<MapData> maps_after_16_02(){
    return with_beaten(story_maps(), "SilphCo_5F", 9, 21);
}

//  Row `id` walked on the sim: the steps run, no rule broken, the gates the
//  row declares open, and the Card Key held or not as `card_key` says. The
//  trainers it fought and the tile it ends on are returned for the caller.
struct RowResult{
    std::string failure;
    std::vector<std::string> trainers;
    std::vector<std::string> floors;
    MapPosition end;
    bool card_key = false;
};
RowResult walk_row(std::string_view id, const std::vector<MapData>& maps){
    RowWalk walk(id, maps);
    RowResult out;
    out.failure = walk.run();
    if (out.failure.empty()){
        out.failure = walk.problems();
    }
    out.trainers = walk.sim().state().trainers_beaten;
    out.floors = walk.floors();
    out.end = walk.sim().state().position;
    out.card_key = walk.sim().card_key_held();
    return out;
}

std::string wrong_end(std::string_view id, const RowResult& result, const MapPosition& want){
    if (!result.failure.empty()){
        return std::format("{}: ended on {}; {}", id, to_string(result.end), result.failure);
    }
    if (!(result.end == want)){
        return std::format("{}: ended on {}, not {}.", id, to_string(result.end), to_string(want));
    }
    return "";
}

//  16.02: 5F's pad to 9F and back by the loop to the corridor, east to the
//  ball with the CARD KEY (21, 21); the scientist on the way fights.
UnitTestResult test_card_key_row(){
    const RowResult result = walk_row("16.02", story_maps());
    if (const std::string wrong = wrong_end("16.02", result, MapPosition{"SilphCo_5F", {21, 21}}); !wrong.empty()){
        return wrong;
    }
    if (!result.card_key){
        return "16.02: the Card Key was not picked up.";
    }
    if (result.trainers != std::vector<std::string>{"scientist", "rocket_m"}){
        return "16.02: did not fight just the scientist and the Rocket on 5F.";
    }
    return true;
}

//  16.03: up to 7F by 3F's Card Key door (opened with the key), the rival on
//  his trigger, then to the employee with the Lapras.
UnitTestResult test_rival_row(){
    const RowResult result = walk_row("16.03", maps_after_16_02());
    if (const std::string wrong = wrong_end("16.03", result, MapPosition{"SilphCo_7F", {1, 7}}); !wrong.empty()){
        return wrong;
    }
    const std::vector<std::string> floors_have = {"SilphCo_3F", "SilphCo_7F"};
    for (const std::string& floor : floors_have){
        if (std::find(result.floors.begin(), result.floors.end(), floor) == result.floors.end()){
            return "16.03: the steps never reached " + floor + ".";
        }
    }
    return true;
}

//  16.03's checkpoint: Gift RNG's Lapras, on 7F beside the employee.
UnitTestResult test_lapras_handoff(){
    const StoryCheckpoint& gift = checkpoint("16.03");
    if (gift.handoff != StoryHandoff::GIFT_RNG || gift.gift != PokemonFRLG_RngTarget::lapras){
        return "16.03 does not hand off to Gift RNG for the Lapras.";
    }
    const StoryCheckpoint& live = checkpoint("16.04");
    if (!live.live_start || live.handoff != StoryHandoff::NONE){
        return "16.04 is not the save Gift RNG leaves.";
    }
    return true;
}

//  16.04 runs one step, the summary Gift RNG leaves open; 16.05 starts where it does.
UnitTestResult test_after_gift_row(){
    const RowResult result = walk_row("16.04", maps_after_16_02());
    const std::string wrong = wrong_end("16.04", result, MapPosition{"SilphCo_7F", {1, 7}});
    if (!wrong.empty()){
        return wrong;
    }
    return true;
}

//  16.05: the pad to 11F, down its left wall out of the Rocket's column, the
//  Rocket beside the player (his fight), the Card Key door and Giovanni's
//  trigger, then to the president.
UnitTestResult test_giovanni_row(){
    const RowResult result = walk_row("16.05", maps_after_16_02());
    if (const std::string wrong = wrong_end("16.05", result, MapPosition{"SilphCo_11F", {9, 10}}); !wrong.empty()){
        return wrong;
    }
    if (result.trainers.size() != size_t(1)){
        return "16.05: the Rocket at (2, 19) did not fight alone.";
    }
    return true;
}

//  16.06: the walk back down to Saffron's Center on the maps with no Rocket in
//  Silph Co. or Saffron City.
UnitTestResult test_return_row(){
    const std::set<std::string> flags{std::string(SILPH_CLEARED_FLAG)};
    const RowResult result = walk_row("16.06", with_beaten(story_maps(flags), "SilphCo_5F", 9, 21));
    if (const std::string wrong = wrong_end("16.06", result, MapPosition{"SaffronCity", {24, 39}}); !wrong.empty()){
        return wrong;
    }
    if (!result.trainers.empty()){
        return "16.06: walked into a trainer's line.";
    }
    return true;
}

bool has_object_at(const std::vector<MapData>& maps, std::string_view map_name, int x, int y){
    for (const MapData& map : maps){
        for (const MapObject& object : map.objects){
            if (map.name == map_name && object.x == x && object.y == y){
                return true;
            }
        }
    }
    return false;
}

//  The Rocket at Saffron (46, 13) blocks the Gym's door until the story sets
//  silph_cleared (16.05's Giovanni); the maps of 15.01 and 16.01 keep him.
UnitTestResult test_saffron_rocket_removed(){
    if (!has_object_at(story_maps(), "SaffronCity", 46, 13)){
        return "No Rocket at Saffron (46, 13) before Silph Co. is cleared.";
    }
    if (!has_object_at(story_maps({"card_key_held"}), "SaffronCity", 46, 13)){
        return "Another flag hides the Rocket at Saffron (46, 13).";
    }
    if (has_object_at(story_maps({std::string(SILPH_CLEARED_FLAG)}), "SaffronCity", 46, 13)){
        return "A Rocket still stands at Saffron (46, 13) after Silph Co.";
    }
    return true;
}

//  silph_cleared is set by the Giovanni step, nothing earlier.
UnitTestResult test_silph_cleared_flag(){
    for (const std::string_view id : {"16.01", "16.02", "16.03", "16.04"}){
        for (const StoryStep& step : checkpoint(id).steps){
            if (std::find(step.flags_after.begin(), step.flags_after.end(), SILPH_CLEARED_FLAG) != step.flags_after.end()){
                return std::format("{} sets silph_cleared.", id);
            }
        }
    }
    const StoryCheckpoint& row = checkpoint("16.05");
    const bool set = std::any_of(row.steps.begin(), row.steps.end(), [](const StoryStep& step){
        return step.name == "giovanni" && std::find(step.flags_after.begin(), step.flags_after.end(), SILPH_CLEARED_FLAG) != step.flags_after.end();
    });
    if (!set){
        return "16.05's Giovanni step does not set silph_cleared.";
    }
    const std::vector<std::string>& start = checkpoint("16.06").start_flags;
    if (std::find(start.begin(), start.end(), SILPH_CLEARED_FLAG) == start.end()){
        return "16.06 does not start with silph_cleared.";
    }
    return true;
}

//  The president's Master Ball is skipped by the option or a SPEEDRUN, taken otherwise.
UnitTestResult test_master_ball_option(){
    using Pokemon::StoryCheckLevel;
    if (StoryExecutor::master_ball_skipped(true, StoryCheckLevel::DEFAULT)){
        return "The Master Ball is skipped with the option on.";
    }
    if (!StoryExecutor::master_ball_skipped(false, StoryCheckLevel::DEFAULT)){
        return "The Master Ball is taken with the option off.";
    }
    if (!StoryExecutor::master_ball_skipped(true, StoryCheckLevel::SPEEDRUN)){
        return "The Master Ball is taken in a SPEEDRUN.";
    }
    const StoryCheckpoint& row = checkpoint("16.05");
    const bool skippable = std::any_of(row.steps.begin(), row.steps.end(), [](const StoryStep& step){
        return step.name == "master_ball" && step.skip_if == std::vector<std::string>{"master_ball_skipped"};
    });
    if (!skippable){
        return "16.05's master_ball step is not skip_if master_ball_skipped.";
    }
    return true;
}

const AutoStorySilphCase AUTO_STORY_SILPH_CASES[] = {
    {
        .input = {.name = "16.01 Silph Co.'s stairs to 3F and its pad to 5F", .body = test_pad_to_5f},
        .expectation = {.description = "to 3F's pad (4, 6), onto 5F's (15, 7), nobody met"},
    },
    {
        .input = {.name = "16.02 the pad loop on 5F to the Card Key", .body = test_card_key_row},
        .expectation = {.description = "ends at (21, 21) with the key, the scientist and a Rocket fought"},
    },
    {
        .input = {.name = "16.03 3F's Card Key door, the pad to 7F and the rival", .body = test_rival_row},
        .expectation = {.description = "opens 3F's door_2, ends by the employee on 7F"},
    },
    {
        .input = {.name = "16.03/16.04 the Lapras by Gift RNG", .body = test_lapras_handoff},
        .expectation = {.description = "16.03 hands off to Gift RNG for a Lapras; 16.04 is the live save"},
    },
    {
        .input = {.name = "16.04 the save after Gift RNG", .body = test_after_gift_row},
        .expectation = {.description = "one step, still beside the employee"},
    },
    {
        .input = {.name = "16.05 11F's Card Key door and Giovanni", .body = test_giovanni_row},
        .expectation = {.description = "one Rocket fought, 11F's door opened, ends below the president"},
    },
    {
        .input = {.name = "16.06 back to Saffron's Center with no Rockets", .body = test_return_row},
        .expectation = {.description = "no trainer met, ends below the Center's door"},
    },
    {
        .input = {.name = "the Saffron Rocket is gone after Silph Co.", .body = test_saffron_rocket_removed},
        .expectation = {.description = "present for no flags, hidden by silph_cleared"},
    },
    {
        .input = {.name = "silph_cleared is set by the Giovanni step", .body = test_silph_cleared_flag},
        .expectation = {.description = "16.05's giovanni sets it; 16.06 starts with it"},
    },
    {
        .input = {.name = "the Master Ball option and SPEEDRUN", .body = test_master_ball_option},
        .expectation = {.description = "skipped when the option is off or in a SPEEDRUN"},
    },
};

} // namespace


void add_tests_AutoStorySilph(UnitTestDatabase& database){
    for (const auto& item : AUTO_STORY_SILPH_CASES){
        database.add<Test_AutoStorySilph>(item);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
