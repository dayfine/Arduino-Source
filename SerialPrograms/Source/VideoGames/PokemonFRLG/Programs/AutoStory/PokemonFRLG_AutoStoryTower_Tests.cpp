/*  AutoStory Tower Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Rows 10.01 to 10.06 on the overworld sim: from the Eevee's room in
 *  Celadon Mansion (09.04) by the Department Store's lift to its 5F's X
 *  items, to Route 16's house for HM02 and a Fly to Lavender Town, up
 *  the Pokemon Tower past the rival, its Channelers, the Marowak ghost and
 *  the three Grunts to Mr. Fuji, and the POKE FLUTE in his house. Each row
 *  fights only the trainers it plans to (the chain from row to row is
 *  PokemonFRLG_AutoStory_Tests.cpp's).
 *
 */

#include <algorithm>
#include <format>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Pokemon_BuiltinStoryActions.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_HideoutLiftRoom.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMaps.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldObjects.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldSim.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryFieldMoves.h"
#include "PokemonFRLG_AutoStoryLegs.h"
#include "PokemonFRLG_AutoStoryMember.h"
#include "PokemonFRLG_AutoStoryMemberInfo.h"
#include "PokemonFRLG_AutoStoryPacedRoute.h"
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
    maps.push_back(department_store_lift_room());
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
    //  The Department Store's lift (10.01): its room and its floors.
    SimScript script;
    script.lift_floors[std::string(DEPT_STORE_LIFT_MAP)] = department_store_lift_exits();
    auto sim = std::make_unique<OverworldSim>(maps, MapPosition{at.map, at.tile}, *at.facing, script);
    RowRun run;
    for (const StoryStep& step : row.steps){
        if (scripted(step)){
            at = *story_pose(Pokemon::pose_of(*step.pose_after));
            run.fought.insert(run.fought.end(), sim->state().trainers_beaten.begin(), sim->state().trainers_beaten.end());
            if (!sim->state().violations.empty()){
                run.failure = std::format("{}: a violation before {}: {}", id, step.name, sim->state().violations.back());
                return run;
            }
            sim = std::make_unique<OverworldSim>(maps, MapPosition{at.map, at.tile}, *at.facing, script);
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

//  10.01: the Department Store's lift up to the X items clerk on 5F and
//  back down, Celadon City past Route 16's Cut tree to the house with
//  HM02, out of Lea's and Jed's lines, then a Fly to Lavender's Center.
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

//  Battle checkpoint `id`'s precondition: a LEVEL step on the lead before
//  its battle `battle`, at least `pokeguy`, pokeguy's level there (whose
//  sims stand for the fight at that level or above). A lead at that level
//  passes it; one a level below stops the checkpoint before the fight.
UnitTestResult check_lead_level_precondition(Logger& logger, std::string_view id, std::string_view battle, unsigned pokeguy){
    std::optional<LevelCheck> check;
    for (const StoryStep& step : checkpoint(id).steps){
        if (step.kind == StoryStepKind::BATTLE && step.name == battle){
            break;
        }
        if (step.action == LEVEL_ACTION){
            check = level_check_of(step.args);
        }
    }
    if (!check || check->slot != 0 || check->level != pokeguy){
        return std::format("{}: expected a LEVEL step on the lead, at least Lv{}, before {}.", id, pokeguy, battle);
    }
    for (const unsigned level : {pokeguy, pokeguy - 1}){
        const MemberReader lead = [level](size_t slot) -> std::optional<MemberInfo>{
            if (slot != 0){
                return std::nullopt;
            }
            return MemberInfo{.species = "blastoise", .level = level};
        };
        const LevelResult result = run_level_check(logger, lead, *check);
        const LevelEnd expected = level >= pokeguy ? LevelEnd::REACHED : LevelEnd::BELOW;
        if (result.end != expected || result.level != level){
            return std::format("{}: a Lv{} lead {} the Lv{} check.", id, level, level >= pokeguy ? "fails" : "passes", pokeguy);
        }
    }
    return true;
}

//  10.01's Fly names the Pidgey, not a slot, and flies with the slot the
//  party reads it in: the third with the Oddish and the Eevee, the second
//  without the Oddish, none without a Pidgey.
UnitTestResult test_fly_finds_pidgey(Logger&){
    const StoryCheckpoint& row = checkpoint("10.01");
    const auto fly = std::find_if(row.steps.begin(), row.steps.end(), [](const StoryStep& step){
        return parse_fly_step(step).has_value();
    });
    if (fly == row.steps.end() || parse_fly_step(*fly)->species != "pidgey" || fly->args.contains("slot")){
        return std::string("10.01: expected a Fly by the Pidgey's species, with no slot.");
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
    if (plan.party_slot(party({"blastoise", "oddish", "pidgey", "eevee"})) != size_t(2)
        || plan.party_slot(party({"blastoise", "pidgey"})) != size_t(1)
        || plan.party_slot(party({"blastoise", "oddish"})).has_value()
    ){
        return std::string("10.01: the Fly doesn't find the Pidgey's slot as the party reads it.");
    }
    return true;
}

//  10.01 below_fly_woman from east of Route 16's north gate on the
//  Bicycle, which the gate's warps leave the rider on (allow_cycling): the
//  open leg west to the Fly house's door must be walked at the Bicycle's
//  pace, or it overshoots the door (console B, 2026-11-12 01:05: a
//  run-paced west 10 ended four tiles past the door, and north bumped the
//  fence).
UnitTestResult test_fly_house_door_on_bike(Logger& logger){
    const StoryCheckpoint& row = checkpoint("10.01");
    const auto step = std::find_if(row.steps.begin(), row.steps.end(), [](const StoryStep& data){
        return data.name == "below_fly_woman";
    });
    if (step == row.steps.end()){
        return std::string("10.01: no below_fly_woman.");
    }
    SimScript script;
    script.on_bike = true;
    script.bike_registered = true;
    OverworldSim sim(maps_with_beaten({}), MapPosition{"Route16", {28, 6}}, Direction::WEST, script);
    StoryPose at{"Route16", {28, 6}, Direction::WEST};
    const std::string failure = row_step_on_sim(sim, at, *step, "10.01");
    for (const std::string& line : sim.state().log){
        logger.log(line);
    }
    if (!failure.empty()){
        return failure;
    }
    const MapPosition end{"Route16_House", {4, 4}};
    if (!(sim.state().position == end)){
        return std::format("10.01 below_fly_woman: ended on {}, not {}.", to_string(sim.state().position), to_string(end));
    }
    return true;
}

//  The step a press makes off a warp, ending where the game leaves the
//  player and facing the way it turns them (arrival_facing()), not the way
//  the press went. 10.01 below_store_lift comes down from the roof room on
//  Celadon Mansion's 2F stairs (4, 2), taken east, to 1F's (4, 2), where
//  the player faces west (console B, 2026-11-11 23:40). Out of Mt. Moon
//  B1F's north exit, the game walks the player south out of Route 4's cave
//  entrance (32, 5).
UnitTestResult test_warp_press_arrivals(Logger&){
    const struct{
        MapPosition from;
        Direction press;
        MapPosition end;
        Direction facing;
    } CASES[] = {
        {{"CeladonCity_Condominiums_2F", {4, 2}}, Direction::EAST, {"CeladonCity_Condominiums_1F", {4, 2}}, Direction::WEST},
        {{"MtMoon_B1F", {45, 5}}, Direction::NORTH, {"Route4", {32, 6}}, Direction::SOUTH},
    };
    for (const auto& c : CASES){
        const std::optional<RouteStep> step = warp_press(story_maps(), c.from, c.press);
        if (!step){
            return std::format("A press {} from {}: no warp taken.", direction_name(c.press), to_string(c.from));
        }
        if (!(step->end == c.end) || step->facing_after != c.facing){
            return std::format(
                "A press {} from {}: ends on {} facing {}, expected {} facing {}.",
                direction_name(c.press), to_string(c.from), to_string(step->end), direction_name(step->facing_after),
                to_string(c.end), direction_name(c.facing)
            );
        }
    }
    return true;
}

const AutoStoryTowerCase AUTO_STORY_TOWER_CASES[] = {
    {.input = {"10.01 Route 16's house for HM02, out of every line, and Fly to Lavender's Pokemon Center", test_back_to_lavender}, .expectation = {}},
    {.input = {"10.01 flies with the Pidgey found by species", test_fly_finds_pidgey}, .expectation = {}},
    {.input = {"10.01 on the Bicycle through Route 16's north gate to the Fly house's door", test_fly_house_door_on_bike}, .expectation = {}},
    {.input = {"10.01 a press off Celadon Mansion's 2F stairs arrives on 1F facing west", test_warp_press_arrivals}, .expectation = {}},
    {.input = {"10.02 the Pokemon Tower's 1F and 2F to east of the rival's trigger", test_tower_to_rival}, .expectation = {}},
    {.input = {"10.03 onto the rival's trigger", test_tower_rival}, .expectation = {}},
    {.input = {"10.04 the Tower's 3F to 5F, Ruth and Tammy alone fought", test_tower_3f_to_5f}, .expectation = {}},
    {.input = {"10.05 the Tower's 6F, Jennifer and Emilia alone fought, to the Marowak's trigger", test_tower_6f}, .expectation = {}},
    {.input = {"10.06 the Tower's 7F Grunts, Mr. Fuji and his house", test_tower_7f_and_fuji}, .expectation = {}},
    {.input = {"10.03 stops before the Tower rival unless the lead is at least pokeguy's Lv36", [](Logger& logger){ return check_lead_level_precondition(logger, "10.03", "rival_tower", 36); }}, .expectation = {}},
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
