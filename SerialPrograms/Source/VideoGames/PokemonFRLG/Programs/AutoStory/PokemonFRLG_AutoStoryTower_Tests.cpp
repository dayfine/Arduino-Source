/*  AutoStory Tower Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Rows 10.01 to 10.06 on the overworld sim: from where 09.02's Escape
 *  Rope leaves the player in Celadon City to Route 16's house for HM02
 *  and a Fly to Lavender Town, up
 *  the Pokemon Tower past the rival, its Channelers, the Marowak ghost and
 *  the three Grunts to Mr. Fuji, and the POKE FLUTE in his house. Each row
 *  fights only the trainers it plans to (the chain from row to row is
 *  PokemonFRLG_AutoStory_Tests.cpp's).
 *
 */

#include <algorithm>
#include <format>
#include <functional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Pokemon_BuiltinStoryActions.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMaps.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldObjects.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldSim.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryRowSim.h"
#include "PokemonFRLG_AutoStoryRows_Tests.h"
#include "PokemonFRLG_AutoStoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

struct AutoStoryTowerInput{
    std::string name;
    std::function<UnitTestResult(Logger&)> body;
};

struct AutoStoryTowerExpectation{
};

struct AutoStoryTowerCase{
    AutoStoryTowerInput input;
    AutoStoryTowerExpectation expectation;
};

class Test_AutoStoryTower : public UnitTest{
public:
    explicit Test_AutoStoryTower(AutoStoryTowerCase test_case)
        : UnitTest(std::format("PokemonFRLG::AutoStory - Simulation: {}", test_case.input.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        try{
            return m_case.input.body(logger);
        }catch (Exception& e){
            return std::format("{}: {}", e.name(), e.message());
        }
    }

private:
    AutoStoryTowerCase m_case;
};

const StoryCheckpoint& checkpoint(std::string_view id){
    const std::vector<StoryCheckpoint>& rows = story_checkpoints();
    const auto row = std::find_if(rows.begin(), rows.end(), [&](const StoryCheckpoint& data){ return data.id == id; });
    if (row == rows.end()){
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("No checkpoint {}.", id));
    }
    return *row;
}

//  A trainer the story has beaten before the row: still standing where
//  the map puts it, but no longer looking for the player.
struct Beaten{
    std::string map;
    int x = 0;
    int y = 0;
};

//  The maps as the story leaves them by 10.01: the story's flags, as for
//  the rows before (maps_after_route10()), and `beaten` sees no one.
std::vector<MapData> maps_with_beaten(const std::vector<Beaten>& beaten){
    const std::vector<MapData> all = overworld_maps();
    std::set<std::string> flags = map_flag_set(all, "after_hall_of_fame");
    flags.merge(map_flag_set(all, "new_game"));
    flags.erase("FLAG_HIDE_VERMILION_CITY_OAKS_AIDE");
    std::vector<MapData> maps = overworld_maps(flags);
    for (MapData& map : maps){
        for (MapObject& object : map.objects){
            const bool hit = std::any_of(beaten.begin(), beaten.end(), [&](const Beaten& trainer){
                return trainer.map == map.name && trainer.x == object.x && trainer.y == object.y;
            });
            if (hit){
                object.trainer.reset();
            }
        }
    }
    return maps;
}

//  A step the sim can't play but the table says the end of: the rival's
//  and the Marowak's scripted battles, which no trainer of the sim's
//  starts, and a talk whose script warps the player (Mr. Fuji's).
bool scripted(const StoryStep& step){
    if (step.kind == StoryStepKind::TALK){
        return step.pose_after.has_value();
    }
    if (step.kind != StoryStepKind::BATTLE){
        return false;
    }
    const std::string& trainer = Pokemon::story_arg<std::string>(step.args, "trainer");
    return trainer.empty() || trainer.starts_with("TRAINER_RIVAL_");
}

struct RowRun{
    std::string failure;
    std::vector<std::string> fought;
    MapPosition end;
};

//  Row `id` on the sim from its start on `maps`: each step played, or for
//  a scripted one, the sim started again where the table says it ends.
RowRun run_row(std::string_view id, std::vector<MapData> maps){
    const StoryCheckpoint& row = checkpoint(id);
    StoryPose at = *story_pose(Pokemon::pose_of(*row.start));
    auto sim = std::make_unique<OverworldSim>(maps, MapPosition{at.map, at.tile}, *at.facing);
    RowRun run;
    for (const StoryStep& step : row.steps){
        if (scripted(step)){
            at = *story_pose(Pokemon::pose_of(*step.pose_after));
            run.fought.insert(run.fought.end(), sim->state().trainers_beaten.begin(), sim->state().trainers_beaten.end());
            if (!sim->state().violations.empty()){
                run.failure = std::format("{}: a violation before {}: {}", id, step.name, sim->state().violations.back());
                return run;
            }
            sim = std::make_unique<OverworldSim>(maps, MapPosition{at.map, at.tile}, *at.facing);
            continue;
        }
        if (const std::string failure = row_step_on_sim(*sim, at, step, row.id); !failure.empty()){
            run.failure = std::format("{}: ended on {}; {}", id, to_string(sim->state().position), failure);
            return run;
        }
    }
    run.fought.insert(run.fought.end(), sim->state().trainers_beaten.begin(), sim->state().trainers_beaten.end());
    if (!sim->state().violations.empty()){
        run.failure = std::format("{}: a violation on the way: {}", id, sim->state().violations.back());
    }
    run.end = sim->state().position;
    return run;
}

//  Row `id` ends on `end`, having fought `fought` (the sim's object names)
//  in order and no one else.
UnitTestResult row_checked(
    std::string_view id, const std::vector<Beaten>& beaten, const std::vector<std::string>& fought, const MapPosition& end
){
    const RowRun run = run_row(id, maps_with_beaten(beaten));
    if (!run.failure.empty()){
        return run.failure;
    }
    if (run.fought != fought){
        std::string names;
        for (const std::string& name : run.fought){
            names += std::format(" {}", name);
        }
        return std::format("{}: fought{} on the sim, not the {} planned.", id, names.empty() ? " no one" : names, fought.size());
    }
    if (!(run.end == end)){
        return std::format("{}: ended on {}, not {}.", id, to_string(run.end), to_string(end));
    }
    return true;
}

//  10.01: Celadon City past Route 16's Cut tree to the house with HM02,
//  out of Lea's and Jed's lines, then a Fly to Lavender's Center.
UnitTestResult test_back_to_lavender(Logger&){
    return row_checked("10.01", {}, {}, MapPosition{"LavenderTown_PokemonCenter_1F", {7, 4}});
}

//  10.02: the Tower's door, 1F, and 2F to east of the rival's trigger.
UnitTestResult test_tower_to_rival(Logger&){
    return row_checked("10.02", {}, {}, MapPosition{"PokemonTower_2F", {17, 6}});
}

//  10.03: the step onto the trigger below the rival; his battle is the
//  script's.
UnitTestResult test_tower_rival(Logger&){
    return row_checked("10.03", {}, {}, MapPosition{"PokemonTower_2F", {16, 6}});
}

//  10.04: 3F and 4F out of every Channeler's line, Ruth and Tammy fought
//  on 5F.
UnitTestResult test_tower_3f_to_5f(Logger&){
    return row_checked("10.04", {}, {"channeler", "channeler"}, MapPosition{"PokemonTower_5F", {18, 7}});
}

//  10.05: from beside Tammy, beaten, down to 6F: Jennifer and Emilia
//  fought, Angelica walked round, the Rare Candy, the Marowak's trigger.
UnitTestResult test_tower_6f(Logger&){
    return row_checked(
        "10.05", {{"PokemonTower_5F", 19, 7}}, {"channeler", "channeler"}, MapPosition{"PokemonTower_6F", {11, 15}}
    );
}

//  10.06: the three Grunts up column 11, Mr. Fuji, and his house.
UnitTestResult test_tower_7f_and_fuji(Logger&){
    return row_checked(
        "10.06", {}, {"rocket_m", "rocket_m", "rocket_m"}, MapPosition{"LavenderTown_VolunteerPokemonHouse", {3, 4}}
    );
}

const AutoStoryTowerCase AUTO_STORY_TOWER_CASES[] = {
    {.input = {"10.01 Route 16's house for HM02, out of every line, and Fly to Lavender's Pokemon Center", test_back_to_lavender}, .expectation = {}},
    {.input = {"10.02 the Pokemon Tower's 1F and 2F to east of the rival's trigger", test_tower_to_rival}, .expectation = {}},
    {.input = {"10.03 onto the rival's trigger", test_tower_rival}, .expectation = {}},
    {.input = {"10.04 the Tower's 3F to 5F, Ruth and Tammy alone fought", test_tower_3f_to_5f}, .expectation = {}},
    {.input = {"10.05 the Tower's 6F, Jennifer and Emilia alone fought, to the Marowak's trigger", test_tower_6f}, .expectation = {}},
    {.input = {"10.06 the Tower's 7F Grunts, Mr. Fuji and his house", test_tower_7f_and_fuji}, .expectation = {}},
};

} // namespace


void add_tests_AutoStoryTower(UnitTestDatabase& database){
    for (const auto& item : AUTO_STORY_TOWER_CASES){
        database.add<Test_AutoStoryTower>(item);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
