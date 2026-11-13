/*  AutoStory Celadon Gym Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Rows 12.03, 14.03 and 14.04 (StoryData/frlg-12.03.plan.json, 14.03, 14.04)
 *  on the overworld sim: from Fuchsia's nurse out of the Center and Fly to
 *  Celadon; from Fly's landing in Celadon past the yard's tree, up
 *  the gym's west aisle into Beauty Tamia's line, round her by column 1,
 *  the inner tree, Beauty Lori, to below Erika; and from Erika back out
 *  the same way, past the tree, to the nurse. And the lead's attacks against the
 *  gym's three parties.
 *
 */

#include <algorithm>
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
#include "PokemonFRLG_AutoStoryMember.h"
#include "PokemonFRLG_AutoStoryRowSim.h"
#include "PokemonFRLG_AutoStoryRows_Tests.h"
#include "PokemonFRLG_AutoStoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

const std::string GYM = "CeladonCity_Gym";
const std::string CELADON = "CeladonCity";

struct AutoStoryCeladonGymInput{
    std::string name;
    std::function<UnitTestResult()> body;
};

struct AutoStoryCeladonGymExpectation{
    std::string_view description;
};

struct AutoStoryCeladonGymCase{
    AutoStoryCeladonGymInput input;
    AutoStoryCeladonGymExpectation expectation;
};

class Test_AutoStoryCeladonGym : public UnitTest{
public:
    explicit Test_AutoStoryCeladonGym(AutoStoryCeladonGymCase test_case)
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
    AutoStoryCeladonGymCase m_case;
};

const StoryCheckpoint& checkpoint(std::string_view id){
    const std::vector<StoryCheckpoint>& rows = story_checkpoints();
    const auto row = std::find_if(rows.begin(), rows.end(), [&](const StoryCheckpoint& data){ return data.id == id; });
    if (row == rows.end()){
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("No checkpoint {}.", id));
    }
    return *row;
}

//  A step of a row on the sim: row_step_on_sim(), but Erika's battle,
//  which a talk starts, leaves the player where the table says. The
//  Center's way in and the nurse, which the sim hasn't got, pass.
std::string gym_step_on_sim(const StoryStep& step, const std::string& id, StoryPose& at, OverworldSim& sim){
    if (step.kind == StoryStepKind::BATTLE && step.name == "erika"){
        at = *story_pose(Pokemon::pose_of(*step.pose_after));
        return "";
    }
    return row_step_on_sim(sim, at, step, id);
}

//  Row `id`'s steps on `sim` from `at`: the failure, or empty.
std::string row_on_sim(std::string_view id, StoryPose& at, OverworldSim& sim){
    const StoryCheckpoint& row = checkpoint(id);
    for (const StoryStep& step : row.steps){
        if (const std::string failure = gym_step_on_sim(step, row.id, at, sim); !failure.empty()){
            return std::format("{}: ended on {}; {}", id, to_string(sim.state().position), failure);
        }
    }
    if (!sim.state().violations.empty()){
        return std::format("{}: a violation on the way: {}", id, sim.state().violations.back());
    }
    return "";
}

std::string fought(const OverworldSim& sim){
    std::string names;
    for (const std::string& name : sim.state().trainers_beaten){
        names += std::format(" {}", name);
    }
    return names.empty() ? " no one" : names;
}

//  12.03 on a sim from Fuchsia's nurse: out of the Center, the Fly, and
//  the landing on Celadon's heal spot (48, 12) facing south, where 14.03
//  starts. No one is fought on the way.
UnitTestResult test_fly_to_celadon(){
    StoryPose at = *story_pose(Pokemon::pose_of(*checkpoint("12.03").start));
    OverworldSim sim(story_maps(), MapPosition{at.map, at.tile}, *at.facing);
    if (const std::string failure = row_on_sim("12.03", at, sim); !failure.empty()){
        return failure;
    }
    const MapPosition landing{CELADON, {48, 12}};
    if (!(sim.state().position == landing) || sim.state().facing != Direction::SOUTH){
        return std::format("12.03: ended on {}, not Celadon's heal spot {} facing south.", to_string(sim.state().position), to_string(landing));
    }
    if (!sim.state().trainers_beaten.empty()){
        return std::format("12.03: fought{}.", fought(sim));
    }
    const StoryPose next = *story_pose(Pokemon::pose_of(*checkpoint("14.03").start));
    if (next.map != CELADON || !(next.tile == landing.point) || next.facing != Direction::SOUTH){
        return "14.03 doesn't start where 12.03 ends.";
    }
    return true;
}

//  14.03 on `sim` from its start, Fly's landing (48, 12): the yard's tree
//  cut, into the gym and up the west aisle; Tamia walks down to (2, 9)
//  and is fought, the walk goes round her by column 1, the inner tree
//  (3, 5) is cut, and Lori is fought from (5, 5). Ends below Erika, where
//  14.04 starts. The failure, or empty.
std::string to_erika(StoryPose& at, OverworldSim& sim){
    if (const std::string failure = row_on_sim("14.03", at, sim); !failure.empty()){
        return failure;
    }
    const MapPosition below_erika{GYM, {6, 5}};
    if (!(sim.state().position == below_erika) || sim.state().facing != Direction::NORTH){
        return std::format("14.03: ended on {}, not below Erika {} facing north.", to_string(sim.state().position), to_string(below_erika));
    }
    if (sim.state().trainers_beaten != std::vector<std::string>{"beauty", "beauty"}){
        return std::format("14.03: fought{}, not Tamia and Lori.", fought(sim));
    }
    const StoryPose next = *story_pose(Pokemon::pose_of(*checkpoint("14.04").start));
    if (next.map != GYM || !(next.tile == below_erika.point)){
        return "14.04 doesn't start where 14.03 ends.";
    }
    return "";
}

//  14.03 on the sim (to_erika()), then 14.04 on the same sim, the gym as
//  14.03 leaves it (Tamia on (2, 9), the inner tree cut): out along row
//  5, a step onto the cut tree's tile, down column 1 past Tamia and
//  column 2 to the wall, out of the gym, the tree again and back below
//  the Center's door (the Center's way in and the nurse need the
//  console), meeting no one else.
UnitTestResult test_gym_rows(){
    StoryPose at = *story_pose(Pokemon::pose_of(*checkpoint("14.03").start));
    OverworldSim sim(story_maps(), MapPosition{at.map, at.tile}, *at.facing);
    if (const std::string failure = to_erika(at, sim); !failure.empty()){
        return failure;
    }
    if (const std::string failure = row_on_sim("14.04", at, sim); !failure.empty()){
        return failure;
    }
    if (sim.state().trainers_beaten.size() != 2){
        return std::format("14.04: fought{} after Tamia and Lori.", fought(sim));
    }
    const MapPosition below_center{CELADON, {48, 12}};
    if (!(sim.state().position == below_center)){
        return std::format("14.04: ended on {}, not below the Center's door {}.", to_string(sim.state().position), to_string(below_center));
    }
    return true;
}

//  The lead as pokeguy brings it to Erika, without the Blizzard he
//  teaches there (left out: no TM spent without the user): Surf, Water
//  Pulse, Bite, Mega Kick. Against Tamia's, Lori's and Erika's Grass
//  types it never picks a Water move, which they resist; against Lori's
//  Exeggcute (Grass, Psychic) it picks Bite.
UnitTestResult test_lead_moves_against_grass(){
    PartyMember lead;
    lead.species = "blastoise";
    lead.moves_read = true;
    lead.level = 42;
    lead.moves = {{"surf", 15}, {"water-pulse", 20}, {"bite", 25}, {"mega-kick", 5}};
    constexpr size_t SURF = 0;
    constexpr size_t WATER_PULSE = 1;
    constexpr size_t BITE = 2;
    for (std::string_view foe : {"bellsprout", "exeggcute", "victreebel", "tangela", "vileplume"}){
        const std::optional<size_t> slot = attack_slot_against(lead, {std::string(foe)});
        if (!slot || *slot == SURF || *slot == WATER_PULSE){
            return std::format("Against {}: picked a Water move or none.", foe);
        }
    }
    if (attack_slot_against(lead, {"exeggcute"}) != BITE){
        return "Expected Bite against Lori's Exeggcute.";
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

//  Erika's rows are PokemonSimulation's planning_risk() for each foe leading
//  against the entry state of its erika case (35/137): speed, the 80th
//  percentile loss, the mean, the worst case, the worst normal hit, then
//  whether it lowers accuracy and whether it explodes.
UnitTestResult test_erika_foe_threats_are_the_generators(){
    for (const StoryStep& step : checkpoint("14.04").steps){
        if (step.name != "erika"){
            continue;
        }
        const auto found = step.args.find("foe_threats");
        const auto* rows = found == step.args.end() ? nullptr : std::get_if<std::vector<std::vector<int64_t>>>(&found->second);
        const std::vector<std::vector<int64_t>> expected = {
            {45, 0, 1, 60, 30, 0, 0},       //  Victreebel
            {33, 18, 8, 42, 20, 0, 0},      //  Tangela
            {34, 0, 2, 60, 30, 0, 0},       //  Vileplume
        };
        if (rows == nullptr || *rows != expected){
            return "14.04 erika: foe_threats are not the generator's rows for the erika case.";
        }
        return true;
    }
    return "14.04 has no erika battle step.";
}

const AutoStoryCeladonGymCase AUTO_STORY_CELADON_GYM_CASES[] = {
    {
        .input = {.name = "12.03 out of Fuchsia's Center and Fly to Celadon", .body = test_fly_to_celadon},
        .expectation = {.description = "lands on Celadon's heal spot facing south, where 14.03 starts"},
    },
    {
        .input = {.name = "14.03 to below Erika and 14.04 back to the nurse", .body = test_gym_rows},
        .expectation = {.description = "Tamia and Lori fought where the table steps into their lines, no one else"},
    },
    {
        .input = {.name = "the lead's attacks against Celadon Gym", .body = test_lead_moves_against_grass},
        .expectation = {.description = "no Water move against Grass; Bite against Exeggcute"},
    },
    {
        .input = {.name = "14.04 stops before Erika unless the lead is at least pokeguy's Lv44", .body = []{
            QuietLogger logger;
            return check_lead_level_precondition(logger, "14.04", "erika", 44);
        }},
        .expectation = {.description = "a Lv44 lead passes the LEVEL step, a Lv43 one stops there"},
    },
    {
        .input = {.name = "14.04 Erika's foe rows are the generator's", .body = test_erika_foe_threats_are_the_generators},
        .expectation = {.description = "PokemonSimulation planning_risk() rows"},
    },
};

} // namespace


void add_tests_AutoStoryCeladonGym(UnitTestDatabase& database){
    for (const auto& item : AUTO_STORY_CELADON_GYM_CASES){
        database.add<Test_AutoStoryCeladonGym>(item);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
