/*  AutoStory Snorlax Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Row 11.01 on the overworld sim: from Mr. Fuji's house (10.06) out to
 *  Lavender Town, a Fly to Celadon City, west on the Bicycle to Route 16's
 *  Snorlax, and over its tile once it is gone to the Cycling Road gate.
 *  The flute's wild battle is the script's: the sim goes on from where the
 *  table says it ends, with the Snorlax hidden (FLAG_HIDE_ROUTE_16_SNORLAX,
 *  as story_maps() has it).
 *
 */

#include <algorithm>
#include <format>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Pokemon_BuiltinStoryActions.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMaps.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldSim.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryFieldMoves.h"
#include "PokemonFRLG_AutoStoryRowSim.h"
#include "PokemonFRLG_AutoStoryRows_Tests.h"
#include "PokemonFRLG_AutoStoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

struct AutoStorySnorlaxInput{
    std::string name;
    std::function<UnitTestResult()> body;
};

struct AutoStorySnorlaxExpectation{
};

struct AutoStorySnorlaxCase{
    AutoStorySnorlaxInput input;
    AutoStorySnorlaxExpectation expectation;
};

class Test_AutoStorySnorlax : public UnitTest{
public:
    explicit Test_AutoStorySnorlax(AutoStorySnorlaxCase test_case)
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
    AutoStorySnorlaxCase m_case;
};

const StoryCheckpoint& checkpoint(std::string_view id){
    const std::vector<StoryCheckpoint>& rows = story_checkpoints();
    const auto row = std::find_if(rows.begin(), rows.end(), [&](const StoryCheckpoint& data){ return data.id == id; });
    if (row == rows.end()){
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("No checkpoint {}.", id));
    }
    return *row;
}

//  The maps as the story leaves them by 11.01 (as the Tower's rows), with
//  the Snorlax on Route 16 unless `snorlax_gone`.
std::vector<MapData> story_maps_by_11_01(bool snorlax_gone){
    const std::vector<MapData> all = overworld_maps();
    std::set<std::string> flags = map_flag_set(all, "after_hall_of_fame");
    flags.merge(map_flag_set(all, "new_game"));
    flags.erase("FLAG_HIDE_VERMILION_CITY_OAKS_AIDE");
    if (snorlax_gone){
        flags.insert("FLAG_HIDE_ROUTE_16_SNORLAX");
    }else{
        flags.erase("FLAG_HIDE_ROUTE_16_SNORLAX");
    }
    return overworld_maps(flags);
}

//  The Snorlax's battle: a BATTLE step with no trainer, which the sim
//  can't play; the table says where it ends.
bool scripted(const StoryStep& step){
    return step.kind == StoryStepKind::BATTLE && Pokemon::story_arg<std::string>(step.args, "trainer").empty();
}

//  11.01: out of Mr. Fuji's house, Fly to Celadon, the Bicycle to the
//  Snorlax with no trainer fought, and, the Snorlax gone, on west over its
//  tile to east of the Cycling Road gate's door.
UnitTestResult test_snorlax(){
    const StoryCheckpoint& row = checkpoint("11.01");
    StoryPose at = *story_pose(Pokemon::pose_of(*row.start));
    const std::vector<MapData> maps = story_maps_by_11_01(false);
    if (std::none_of(maps.begin(), maps.end(), [](const MapData& map){
        return map.name == "Route16" && std::any_of(map.objects.begin(), map.objects.end(), [](const MapObject& object){
            return object.x == 31 && object.y == 13;
        });
    })){
        return "11.01: the Snorlax isn't at (31, 13) on Route 16 before the flute.";
    }
    auto sim = std::make_unique<OverworldSim>(maps, MapPosition{at.map, at.tile}, *at.facing);
    for (const StoryStep& step : row.steps){
        if (scripted(step)){
            if (!sim->state().trainers_beaten.empty() || !sim->state().violations.empty()){
                return std::format("11.01: a battle or a violation before {}.", step.name);
            }
            at = *story_pose(Pokemon::pose_of(*step.pose_after));
            sim = std::make_unique<OverworldSim>(story_maps_by_11_01(true), MapPosition{at.map, at.tile}, *at.facing);
            continue;
        }
        if (const std::string failure = row_step_on_sim(*sim, at, step, row.id); !failure.empty()){
            return std::format("11.01: ended on {}; {}", to_string(sim->state().position), failure);
        }
    }
    if (!sim->state().trainers_beaten.empty() || !sim->state().violations.empty()){
        return "11.01: a battle or a violation after the Snorlax.";
    }
    const MapPosition end{"Route16", {28, 13}};
    if (!(sim->state().position == end)){
        return std::format("11.01: ended on {}, not {}.", to_string(sim->state().position), to_string(end));
    }
    return true;
}

//  11.01's Snorlax battle runs (the "run" arg), and only that battle.
UnitTestResult test_snorlax_battle_runs(){
    for (const StoryStep& step : checkpoint("11.01").steps){
        if (step.kind != StoryStepKind::BATTLE){
            continue;
        }
        if (!Pokemon::story_arg<bool>(step.args, "run")){
            return std::format("11.01: {} fights; expected it to run.", step.name);
        }
    }
    for (const StoryCheckpoint& row : story_checkpoints()){
        for (const StoryStep& step : row.steps){
            if (step.kind == StoryStepKind::BATTLE && row.id != "11.01" && Pokemon::story_arg<bool>(step.args, "run")){
                return std::format("{}: {} runs; only 11.01's Snorlax should.", row.id, step.name);
            }
        }
    }
    return true;
}

//  11.01's Fly names the Pidgey, not a slot, and flies with the slot the
//  party reads it in: the third here, the second with the Oddish gone,
//  none without a Pidgey.
UnitTestResult test_fly_finds_pidgey(){
    const StoryCheckpoint& row = checkpoint("11.01");
    const auto fly = std::find_if(row.steps.begin(), row.steps.end(), [](const StoryStep& step){
        return parse_fly_step(step).has_value();
    });
    if (fly == row.steps.end() || parse_fly_step(*fly)->species != "pidgey" || fly->args.contains("slot")){
        return "11.01: expected a Fly by the Pidgey's species, with no slot.";
    }
    const FlyPlan plan = *parse_fly_step(*fly);
    const auto party = [](std::vector<std::string> species){
        return MemberReader([species](size_t slot) -> std::optional<MemberInfo>{
            if (slot >= species.size()){
                return std::nullopt;
            }
            return MemberInfo{.species = species[slot], .level = 30};
        });
    };
    if (fly_slot(plan, party({"blastoise", "oddish", "pidgey", "eevee"})) != size_t(2)
        || fly_slot(plan, party({"blastoise", "pidgey"})) != size_t(1)
        || fly_slot(plan, party({"blastoise", "oddish"})).has_value()
    ){
        return "11.01: the Fly doesn't find the Pidgey's slot as the party reads it.";
    }
    StoryStep both = *fly;
    both.args["slot"] = int64_t(3);
    if (parse_fly_step(both)){
        return "A Fly naming both a slot and a species should not parse.";
    }
    return true;
}

const AutoStorySnorlaxCase AUTO_STORY_SNORLAX_CASES[] = {
    {.input = {"11.01 Fly to Celadon, Route 16's Snorlax, and over its tile to the Cycling Road gate", test_snorlax}, .expectation = {}},
    {.input = {"11.01 runs from the Snorlax's battle", test_snorlax_battle_runs}, .expectation = {}},
    {.input = {"11.01 flies with the Pidgey found by species", test_fly_finds_pidgey}, .expectation = {}},
};

} // namespace


void add_tests_AutoStorySnorlax(UnitTestDatabase& database){
    for (const auto& item : AUTO_STORY_SNORLAX_CASES){
        database.add<Test_AutoStorySnorlax>(item);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
