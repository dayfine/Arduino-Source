/*  AutoStory Celadon Gym Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Rows 14.03 and 14.04 (StoryData/frlg-14.03.plan.json, 14.04) on the
 *  overworld sim: from Fly's landing in Celadon past the yard's tree, up
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
        : UnitTest("PokemonFRLG::AutoStory - Simulation: " + test_case.input.name)
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        try{
            return m_case.input.body();
        }catch (Exception& e){
            return std::string(e.name()) + ": " + e.message();
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
std::string gym_step_on_sim(OverworldSim& sim, StoryPose& at, const StoryStep& step, const std::string& id){
    if (step.kind == StoryStepKind::BATTLE && step.name == "erika"){
        at = *story_pose(Pokemon::pose_of(*step.pose_after));
        return "";
    }
    return row_step_on_sim(sim, at, step, id);
}

//  Row `id`'s steps from the one named `first` on, on `sim` from `at`:
//  the failure, or empty.
std::string row_from(OverworldSim& sim, StoryPose& at, std::string_view id, std::string_view first){
    const StoryCheckpoint& row = checkpoint(id);
    bool started = false;
    for (const StoryStep& step : row.steps){
        started = started || step.name == first;
        if (!started){
            continue;
        }
        if (const std::string failure = gym_step_on_sim(sim, at, step, row.id); !failure.empty()){
            return std::format("ended on {}; {}", to_string(sim.state().position), failure);
        }
    }
    if (!started){
        return std::format("{} has no step {}.", id, first);
    }
    if (!sim.state().violations.empty()){
        return std::format("{}: a violation on the way: {}", id, sim.state().violations.back());
    }
    return "";
}

std::string fought(const OverworldSim& sim){
    std::string names;
    for (const std::string& name : sim.state().trainers_beaten){
        names += " " + name;
    }
    return names.empty() ? " no one" : names;
}

//  14.03 on the sim, from Fly's landing (48, 12): the yard's tree cut,
//  into the gym and up the west aisle; Tamia walks down to (2, 9) and is fought, the walk goes
//  round her by column 1, the inner tree (3, 5) is cut, and Lori is
//  fought from (5, 5). Then 14.04 on the same sim, the gym as 14.03
//  leaves it (Tamia on (2, 9), the inner tree cut): out along row 5, a
//  step onto the cut tree's tile, down column 1 past Tamia and column 2
//  to the wall, out of the gym, the tree again and back below the
//  Center's door (the Center's way in and the nurse need the console),
//  meeting no one else.
UnitTestResult test_gym_rows(){
    StoryPose at = *story_pose(Pokemon::pose_of(*checkpoint("14.03").start));
    OverworldSim sim(story_maps(), MapPosition{at.map, at.tile}, *at.facing);
    if (const std::string failure = row_from(sim, at, "14.03", "above_gym_tree"); !failure.empty()){
        return "14.03: " + failure;
    }
    const MapPosition below_erika{GYM, {6, 5}};
    if (!(sim.state().position == below_erika) || sim.state().facing != Direction::NORTH){
        return std::format("14.03: ended on {}, not below Erika {} facing north.", to_string(sim.state().position), to_string(below_erika));
    }
    if (sim.state().trainers_beaten != std::vector<std::string>{"beauty", "beauty"}){
        return "14.03: fought" + fought(sim) + ", not Tamia and Lori.";
    }
    const StoryPose next = *story_pose(Pokemon::pose_of(*checkpoint("14.04").start));
    if (next.map != GYM || !(next.tile == below_erika.point)){
        return "14.04 doesn't start where 14.03 ends.";
    }
    if (const std::string failure = row_from(sim, at, "14.04", "challenge_erika"); !failure.empty()){
        return "14.04: " + failure;
    }
    if (sim.state().trainers_beaten.size() != 2){
        return "14.04: fought" + fought(sim) + " after Tamia and Lori.";
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

const AutoStoryCeladonGymCase AUTO_STORY_CELADON_GYM_CASES[] = {
    {
        .input = {.name = "14.03 to below Erika and 14.04 back to the nurse", .body = test_gym_rows},
        .expectation = {.description = "Tamia and Lori fought where the table steps into their lines, no one else"},
    },
    {
        .input = {.name = "the lead's attacks against Celadon Gym", .body = test_lead_moves_against_grass},
        .expectation = {.description = "no Water move against Grass; Bite against Exeggcute"},
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
