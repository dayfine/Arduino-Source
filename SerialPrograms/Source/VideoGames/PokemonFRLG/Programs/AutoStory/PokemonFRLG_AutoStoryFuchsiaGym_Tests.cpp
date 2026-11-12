/*  AutoStory Fuchsia Gym Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Rows 12.01 and 12.02 (StoryData/frlg-12.01.plan.json, 12.02) on the
 *  overworld sim: from Fly's landing in Fuchsia to the nurse, into the
 *  gym past Juggler Kirk and round its top edge to west of Koga; and from
 *  Koga out to the Warden and back to the nurse. And the lead's attacks
 *  against Kirk's and Koga's parties.
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
#include "PokemonFRLG_AutoStoryRowSim.h"
#include "PokemonFRLG_AutoStoryRows_Tests.h"
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
        sim.emplace(story_maps(), MapPosition{at.map, at.tile}, *at.facing);
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
//  the player where the table says.
std::string gym_step_on_sim(const StoryStep& step, const std::string& id, GymWalk& walk){
    const bool center = step.action == CENTER_ENTER_VARIANT;
    if (center || (step.kind == StoryStepKind::BATTLE && step.name == "koga")){
        walk.at = *story_pose(Pokemon::pose_of(*step.pose_after));
        if (center){
            walk.place();
        }
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

//  12.01 from Fly's landing: the nurse, the gym, Kirk fought from (14, 11),
//  round the top edge to west of Koga, where 12.02 starts; then 12.02 from
//  there to the Warden and the nurse, meeting no one else.
UnitTestResult test_gym_rows(){
    GymWalk walk(*story_pose(Pokemon::pose_of(*checkpoint("12.01").start)));
    if (const std::string failure = row_on_sim("12.01", walk); !failure.empty()){
        return failure;
    }
    const MapPosition west_of_koga{GYM, {6, 13}};
    if (!(walk.sim->state().position == west_of_koga) || walk.sim->state().facing != Direction::EAST){
        return std::format("12.01: ended on {}, not west of Koga {} facing east.", to_string(walk.sim->state().position), to_string(west_of_koga));
    }
    if (walk.all_fought() != std::vector<std::string>{"rocker"}){
        return std::format("12.01: fought {} trainers, not Kirk alone.", walk.all_fought().size());
    }
    const StoryPose next = *story_pose(Pokemon::pose_of(*checkpoint("12.02").start));
    if (next.map != GYM || !(next.tile == west_of_koga.point)){
        return "12.02 doesn't start where 12.01 ends.";
    }
    if (const std::string failure = row_on_sim("12.02", walk); !failure.empty()){
        return failure;
    }
    if (walk.all_fought().size() != 1){
        return "12.02: fought a trainer after Koga.";
    }
    if (walk.at.map != "FuchsiaCity_PokemonCenter_1F"){
        return std::format("12.02: ended on {}, not at Fuchsia's nurse.", walk.at.map);
    }
    return true;
}

//  The lead as pokeguy brings it to the gym after HM03 (12.01): Surf over
//  Water Gun, Water Pulse, Bite, Mega Kick. It picks Surf against Kirk's
//  Psychic types and Koga's Poison types (pokeguy: Surf throughout).
UnitTestResult test_lead_moves_in_gym(){
    PartyMember lead;
    lead.species = "blastoise";
    lead.moves_read = true;
    lead.level = 39;
    lead.moves = {{"surf", 15}, {"water-pulse", 20}, {"bite", 25}, {"mega-kick", 5}};
    for (std::string_view foe : {"drowzee", "kadabra", "koffing", "muk", "weezing"}){
        if (attack_slot_against(lead, {std::string(foe)}) != 0){
            return std::format("Against {}: expected Surf.", foe);
        }
    }
    return true;
}

const AutoStoryFuchsiaGymCase AUTO_STORY_FUCHSIA_GYM_CASES[] = {
    {
        .input = {.name = "12.01 past Kirk to west of Koga and 12.02 to the Warden and the nurse", .body = test_gym_rows},
        .expectation = {.description = "Kirk fought where the table steps into his line, no one else; ends at Fuchsia's nurse"},
    },
    {
        .input = {.name = "the lead's attacks in Fuchsia Gym", .body = test_lead_moves_in_gym},
        .expectation = {.description = "Surf against Kirk's and Koga's Pokemon"},
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
