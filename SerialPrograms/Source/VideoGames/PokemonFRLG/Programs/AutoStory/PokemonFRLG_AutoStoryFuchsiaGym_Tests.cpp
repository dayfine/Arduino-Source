/*  AutoStory Fuchsia Gym Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Rows 12.01 and 12.02 (StoryData/frlg-12.01.plan.json, 12.02) on the
 *  overworld sim: from Fuchsia's nurse into the gym past Jugglers Kayden
 *  and Kirk and round its top edge to west of Koga; and from Koga out to
 *  the Warden and back to the nurse. And the lead's attacks against the
 *  gym's parties.
 *
 */

#include <format>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Pokemon_BuiltinStoryActions.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepExecutor.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMaps.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldSim.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryParty.h"
#include "PokemonFRLG_AutoStoryRooms.h"
#include "PokemonFRLG_AutoStoryMember.h"
#include "PokemonFRLG_AutoStoryRowSim.h"
#include "PokemonFRLG_AutoStoryRows12_Tests.h"
#include "PokemonFRLG_AutoStoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

const std::string GYM = "FuchsiaCity_Gym";

struct AutoStoryFuchsiaGymInput{
    std::string name;
    std::function<UnitTestResult()> body;
};

struct AutoStoryFuchsiaGymExpectation{
    std::string_view description;
};

struct AutoStoryFuchsiaGymCase{
    AutoStoryFuchsiaGymInput input;
    AutoStoryFuchsiaGymExpectation expectation;
};

class Test_AutoStoryFuchsiaGym : public UnitTest{
public:
    explicit Test_AutoStoryFuchsiaGym(AutoStoryFuchsiaGymCase test_case)
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
    AutoStoryFuchsiaGymCase m_case;
};

const StoryCheckpoint& checkpoint(std::string_view id){
    for (const StoryCheckpoint& row : story_checkpoints()){
        if (row.id == id){
            return row;
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("No checkpoint {}.", id));
}

//  The rows on the sim, which can't walk into a Center with no map. Kept
//  across sims: the trainers fought.
struct GymWalk{
    StoryPose at;
    std::optional<OverworldSim> sim;
    std::vector<std::string> fought;
    //  Koga is beaten: so is every trainer of the gym
    //  (FuchsiaCity_Gym_EventScript_DefeatedKoga, set_gym_trainers 5).
    bool gym_beaten = false;

    explicit GymWalk(StoryPose start)
        : at(std::move(start))
    {
        place();
    }
    //  A fresh sim on `at`, after a step it can't walk.
    void place(){
        if (sim){
            const std::vector<std::string>& beaten = sim->state().trainers_beaten;
            fought.insert(fought.end(), beaten.begin(), beaten.end());
        }
        std::vector<MapData> maps = story_maps();
        for (MapData& map : maps){
            if (!gym_beaten || map.name != GYM){
                continue;
            }
            for (MapObject& object : map.objects){
                object.trainer.reset();
            }
        }
        sim.emplace(std::move(maps), MapPosition{at.map, at.tile}, *at.facing);
    }
    std::vector<std::string> all_fought() const{
        std::vector<std::string> all = fought;
        const std::vector<std::string>& beaten = sim->state().trainers_beaten;
        all.insert(all.end(), beaten.begin(), beaten.end());
        return all;
    }
};

//  A step of a row on the sim: row_step_on_sim(), but the way into a
//  Center, which has no map, and Koga's battle, which a talk starts, leave
//  the player where the table says; and Koga's leaves the gym's trainers
//  beaten.
std::string gym_step_on_sim(const StoryStep& step, const std::string& id, GymWalk& walk){
    const bool center = step.action == CENTER_ENTER_VARIANT;
    if (center || (step.kind == StoryStepKind::BATTLE && step.name == "koga")){
        walk.at = *story_pose(Pokemon::pose_of(*step.pose_after));
        walk.gym_beaten = walk.gym_beaten || !center;
        walk.place();
        return "";
    }
    return row_step_on_sim(*walk.sim, walk.at, step, id);
}

//  Row `id`'s steps on the sim from `walk.at`: the failure, or empty.
std::string row_on_sim(std::string_view id, GymWalk& walk){
    const StoryCheckpoint& row = checkpoint(id);
    for (const StoryStep& step : row.steps){
        if (const std::string failure = gym_step_on_sim(step, row.id, walk); !failure.empty()){
            return std::format("{}: ended on {}; {}", id, to_string(walk.sim->state().position), failure);
        }
        if (!walk.sim->state().violations.empty()){
            return std::format("{}/{}: a violation on the way: {}", id, step.name, walk.sim->state().violations.back());
        }
    }
    return "";
}

//  12.01 from the nurse: the gym, Kayden fought from (14, 16) and Kirk from
//  (14, 11), round the top edge to west of Koga, where 12.02 starts; then
//  12.02 from there to the Warden and the nurse, meeting no one else. The
//  sim's lookers see every way at once, so Edgar, whose lines the legs to
//  the walls cross, always stops the walk there: the worst case, which
//  the console meets only when he looks that way.
UnitTestResult test_gym_rows(){
    GymWalk walk(*story_pose(Pokemon::pose_of(*checkpoint("12.01").start)));
    if (const std::string failure = row_on_sim("12.01", walk); !failure.empty()){
        return failure;
    }
    const MapPosition west_of_koga{GYM, {6, 13}};
    if (!(walk.sim->state().position == west_of_koga) || walk.sim->state().facing != Direction::EAST){
        return std::format("12.01: ended on {}, not west of Koga {} facing east.", to_string(walk.sim->state().position), to_string(west_of_koga));
    }
    if (walk.all_fought() != std::vector<std::string>{"rocker", "rocker", "man"}){
        return std::format("12.01: fought {} trainers, not Kayden, Kirk and Edgar.", walk.all_fought().size());
    }
    const StoryPose next = *story_pose(Pokemon::pose_of(*checkpoint("12.02").start));
    if (next.map != GYM || !(next.tile == west_of_koga.point)){
        return "12.02 doesn't start where 12.01 ends.";
    }
    if (const std::string failure = row_on_sim("12.02", walk); !failure.empty()){
        return failure;
    }
    if (walk.all_fought().size() != 3){
        return "12.02: fought a trainer after Koga.";
    }
    if (walk.at.map != "FuchsiaCity_PokemonCenter_1F"){
        return std::format("12.02: ended on {}, not at Fuchsia's nurse.", walk.at.map);
    }
    return true;
}

//  The lead as pokeguy brings it to the gym after HM03 (12.01): Surf over
//  Water Gun, Water Pulse, Bite, Mega Kick. It picks Surf against Koga's
//  Poison types (pokeguy: Surf throughout) and Surf or Bite, never Mega
//  Kick (5 PP) or Water Pulse, against the Jugglers' Psychic types.
UnitTestResult test_lead_moves_in_gym(){
    PartyMember lead;
    lead.species = "blastoise";
    lead.moves_read = true;
    lead.level = 39;
    lead.moves = {{"surf", 15}, {"water-pulse", 20}, {"bite", 25}, {"mega-kick", 5}};
    constexpr size_t SURF = 0;
    constexpr size_t BITE = 2;
    for (std::string_view foe : {"koffing", "muk", "weezing"}){
        if (attack_slot_against(lead, {std::string(foe)}) != SURF){
            return std::format("Against {}: expected Surf.", foe);
        }
    }
    for (std::string_view foe : {"drowzee", "kadabra", "hypno"}){
        const std::optional<size_t> slot = attack_slot_against(lead, {std::string(foe)});
        if (slot != SURF && slot != BITE){
            return std::format("Against {}: expected Surf or Bite.", foe);
        }
    }
    return true;
}

class QuietLogger : public Logger{
public:
    using Logger::log;
    virtual void log(const std::string&, Color) override{}
};

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

const AutoStoryFuchsiaGymCase AUTO_STORY_FUCHSIA_GYM_CASES[] = {
    {
        .input = {.name = "12.01 past Kayden and Kirk to west of Koga and 12.02 to the Warden and the nurse", .body = test_gym_rows},
        .expectation = {.description = "Kayden and Kirk fought where the table steps into their lines, no one else; ends at Fuchsia's nurse"},
    },
    {
        .input = {.name = "the lead's attacks in Fuchsia Gym", .body = test_lead_moves_in_gym},
        .expectation = {.description = "Surf against Koga's Pokemon, Surf or Bite against the Jugglers'"},
    },
    {
        .input = {.name = "12.02 stops before Koga unless the lead is at least pokeguy's Lv39", .body = []{
            QuietLogger logger;
            return check_lead_level_precondition(logger, "12.02", "koga", 39);
        }},
        .expectation = {.description = "a Lv39 lead passes the LEVEL step, a Lv38 one stops there"},
    },
};

} // namespace


void add_tests_AutoStoryFuchsiaGym(UnitTestDatabase& database){
    for (const auto& item : AUTO_STORY_FUCHSIA_GYM_CASES){
        database.add<Test_AutoStoryFuchsiaGym>(item);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
