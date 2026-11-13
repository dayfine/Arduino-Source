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
#include "PokemonFRLG_AutoStoryGameArgs.h"
#include "PokemonFRLG_AutoStoryHealing.h"
#include "PokemonFRLG_AutoStoryHealingSteps.h"
#include "PokemonFRLG_AutoStoryParty.h"
#include "PokemonFRLG_AutoStoryRooms.h"
#include "PokemonFRLG_AutoStoryMember.h"
#include "PokemonFRLG_AutoStoryMartPlan.h"
#include "PokemonFRLG_AutoStoryMoveData.h"
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

//  The lead of the 2026-11-12 console failure (12.02): Bite, Water Pulse,
//  Surf, Dig (slot 4). Dig was picked 8 turns against Koffing, whose
//  Levitate makes Ground do nothing. Surf instead, over Koga's party.
UnitTestResult test_dig_not_picked_against_levitate(){
    PartyMember lead;
    lead.species = "blastoise";
    lead.moves_read = true;
    lead.level = 39;
    lead.moves = {{"bite", 25}, {"water-pulse", 20}, {"surf", 15}, {"dig", 10}};
    constexpr size_t SURF = 2;
    //  Koga's four, in order; the user won by hand with Surf every turn.
    for (std::string_view foe : {"koffing", "muk", "koffing", "weezing"}){
        if (attack_slot_against(lead, {std::string(foe)}) != SURF){
            return std::format("Against {}: expected Surf.", foe);
        }
    }
    if (attack_slot_against(lead, {"koffing", "muk", "weezing"}) != SURF){
        return "Against Koga's whole party: expected Surf.";
    }
    return true;
}

//  A foe's ability blocks only the type it names, and a single ability only.
UnitTestResult test_ability_immunities(){
    if (ability_immunity("koffing") != PokemonType::GROUND || ability_immunity("weezing") != PokemonType::GROUND){
        return "Expected Koffing and Weezing: Levitate, Ground.";
    }
    if (ability_immunity("vulpix") != PokemonType::FIRE || ability_immunity("jolteon") != PokemonType::ELECTRIC ||
        ability_immunity("vaporeon") != PokemonType::WATER
    ){
        return "Expected Flash Fire, Volt Absorb and Water Absorb immunities.";
    }
    if (ability_immunity("muk") != PokemonType::NONE || ability_immunity("growlithe") != PokemonType::NONE){
        return "Expected no immunity for Muk, or for Growlithe (two possible abilities).";
    }
    if (!takes_two_turns("dig") || !takes_two_turns("fly") || takes_two_turns("surf")){
        return "Expected Dig and Fly to take two turns, and Surf not.";
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

//  pokeguy's bag at Koga holds 5 Super Potions (PokemonSimulation
//  docs/battles/pokeguy-conditions.md, ### koga): 12.01's Mart step buys up
//  to 5, whatever the Potions cover.
UnitTestResult test_mart_fills_super_potions(){
    const std::optional<Pokemon::StoryCheckpoint> row = checkpoint("12.01");
    if (!row){
        return "12.01 is missing.";
    }
    const auto step_it = std::find_if(row->steps.begin(), row->steps.end(), [](const Pokemon::StoryStep& step){
        return step.name == "buy_super_potions";
    });
    if (step_it == row->steps.end()){
        return "12.01 has no buy_super_potions step.";
    }
    struct Case{
        int potions;
        int super_potions;
        size_t expected;
    };
    const Case cases[] = {{6, 0, 5}, {0, 0, 5}, {6, 2, 3}, {6, 5, 0}, {0, 7, 0}};
    QuietLogger logger;
    for (const Case& test : cases){
        Supplies supplies;
        supplies.potions = test.potions;
        supplies.super_potions = test.super_potions;
        Pokemon::StoryStep step = *step_it;
        const std::optional<Pokemon::Outcome> done = fill_mart_to(logger, supplies, step);
        const std::vector<Pokemon::StoryItem>* items = game_arg<std::vector<Pokemon::StoryItem>>(step.args, "items");
        const size_t buys = done ? 0 : (items == nullptr || items->empty() ? 0 : items->at(0).quantity);
        if (buys != test.expected){
            return std::format("{} Potions, {} Super Potions: buys {}, expected {}.", test.potions, test.super_potions, buys, test.expected);
        }
    }
    return true;
}

const StoryStep& step_named(const StoryCheckpoint& row, std::string_view name){
    for (const StoryStep& step : row.steps){
        if (step.name == name){
            return step;
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("No step {} in {}.", name, row.id));
}

//  Whether a BAG_CHECK_ACTION step's flag is set for `supplies`.
bool bag_flag(const Supplies& supplies, const StoryStep& step){
    const std::vector<Pokemon::StoryItem>* items = game_arg<std::vector<Pokemon::StoryItem>>(step.args, "items");
    const bool* any = game_arg<bool>(step.args, "any");
    return items != nullptr && bag_holds(supplies, *items, any != nullptr && *any);
}

Supplies koga_supplies(int super_potions, int antidotes, int full_heals){
    Supplies supplies;
    supplies.potions = 6;
    supplies.super_potions = super_potions;
    supplies.antidotes = antidotes;
    supplies.full_heals = full_heals;
    return supplies;
}

//  The 2026-11-12 loss to Koga: a save in front of him with 6 Potions, 1 Super
//  Potion and no Antidote. 12.02 counts the bag first; the walk out to the
//  Fuchsia Mart and back is skipped only when the bag holds PokeGuy's 5 Super
//  Potions and a cure for poison.
UnitTestResult test_koga_bag_detour_flags(){
    const StoryCheckpoint& row = checkpoint("12.02");
    const StoryStep& supers = step_named(row, "bag_super_potions");
    const StoryStep& cure = step_named(row, "bag_status_cure");
    struct Case{
        int super_potions;
        int antidotes;
        int full_heals;
        bool supers_flag;
        bool cure_flag;
    };
    const Case cases[] = {
        {1, 0, 0, false, false},    //  the loss's bag
        {5, 1, 0, true, true},      //  PokeGuy's: no detour
        {7, 0, 1, true, true},      //  a Full Heal cures poison too
        {5, 0, 0, true, false},     //  only the cure is missing
        {4, 1, 0, false, true},     //  only a Super Potion is missing
        {0, 0, 0, false, false},
    };
    for (const Case& test : cases){
        const Supplies supplies = koga_supplies(test.super_potions, test.antidotes, test.full_heals);
        if (bag_flag(supplies, supers) != test.supers_flag || bag_flag(supplies, cure) != test.cure_flag){
            return std::format(
                "{} Super Potions, {} Antidotes, {} Full Heals: flags {}/{}, expected {}/{}.",
                test.super_potions, test.antidotes, test.full_heals,
                bag_flag(supplies, supers), bag_flag(supplies, cure), test.supers_flag, test.cure_flag
            );
        }
    }
    return true;
}

//  The detour's steps: every walk and cut is skipped when both flags hold; each
//  buy when its own does; the buys are not once-per-save (a skip_if and
//  once_per_save are refused together); it starts and ends where 12.02 does.
UnitTestResult test_koga_bag_detour_steps(){
    const StoryCheckpoint& row = checkpoint("12.02");
    const std::vector<std::string> both = {"bag_super_potions", "bag_status_cure"};
    bool inside = false;
    size_t skipped = 0;
    for (const StoryStep& step : row.steps){
        if (step.name == "challenge_koga"){
            break;
        }
        if (step.name == "leave_gym_by_top_edge"){
            inside = true;
        }
        if (!inside){
            continue;
        }
        const bool buy = step.action == MART_ACTION;
        const std::vector<std::string> expected = !buy ? both
            : step.name == "buy_super_potions_again" ? std::vector<std::string>{"bag_super_potions"}
            : std::vector<std::string>{"bag_status_cure"};
        if (step.skip_if != expected){
            return std::format("12.02/{}: the wrong skip_if.", step.name);
        }
        if (step.once_per_save){
            return std::format("12.02/{}: once per save with a skip_if.", step.name);
        }
        skipped++;
    }
    if (skipped != 14){
        return std::format("12.02: {} detour steps before Koga, expected 14.", skipped);
    }
    const StoryStep& last = step_named(row, "west_of_koga_again");
    const StoryStep& first = step_named(checkpoint("12.01"), "west_of_koga");
    const Pokemon::GridPose* back = game_arg<Pokemon::GridPose>(last.args, "target");
    const Pokemon::GridPose* west = game_arg<Pokemon::GridPose>(first.args, "target");
    if (back == nullptr || west == nullptr || !(*back == *west)){
        return "The detour doesn't end where 12.01 does: west of Koga facing east.";
    }
    return true;
}

//  The Full Heal buy is one when the bag has none, whatever the Potions;
//  the Super Potion buy fills to 5.
UnitTestResult test_koga_bag_detour_buys(){
    const StoryCheckpoint& row = checkpoint("12.02");
    struct Case{
        std::string_view name;
        int super_potions;
        int full_heals;
        size_t expected;
    };
    const Case cases[] = {
        {"buy_super_potions_again", 1, 0, 4}, {"buy_super_potions_again", 0, 0, 5}, {"buy_super_potions_again", 5, 0, 0},
        {"buy_full_heal", 1, 0, 1}, {"buy_full_heal", 1, 1, 0}, {"buy_full_heal", 1, 3, 0},
    };
    QuietLogger logger;
    for (const Case& test : cases){
        const Supplies supplies = koga_supplies(test.super_potions, 0, test.full_heals);
        Pokemon::StoryStep step = step_named(row, test.name);
        const std::optional<Pokemon::Outcome> done = fill_mart_to(logger, supplies, step);
        const std::vector<Pokemon::StoryItem>* items = game_arg<std::vector<Pokemon::StoryItem>>(step.args, "items");
        const size_t buys = done ? 0 : (items == nullptr || items->empty() ? 0 : items->at(0).quantity);
        if (buys != test.expected){
            return std::format("{} with {} Super Potions, {} Full Heals: buys {}, expected {}.", test.name, test.super_potions, test.full_heals, buys, test.expected);
        }
    }
    return true;
}

const AutoStoryFuchsiaGymCase AUTO_STORY_FUCHSIA_GYM_CASES[] = {
    {
        .input = {.name = "12.01's Mart fills the bag to pokeguy's 5 Super Potions", .body = test_mart_fills_super_potions},
        .expectation = {.description = "5 bought with 6 Potions and none, fewer or none when the bag holds some"},
    },
    {
        .input = {.name = "12.02 counts the bag and walks to the Fuchsia Mart only when it lacks PokeGuy's Koga bag", .body = test_koga_bag_detour_flags},
        .expectation = {.description = "flags set for 5 Super Potions and an Antidote or Full Heal; the loss's bag sets neither"},
    },
    {
        .input = {.name = "12.02's detour steps skip together and end west of Koga", .body = test_koga_bag_detour_steps},
        .expectation = {.description = "14 steps skipped by the flags, ending where 12.01 does"},
    },
    {
        .input = {.name = "12.02's Mart buys fill to 5 Super Potions and one Full Heal", .body = test_koga_bag_detour_buys},
        .expectation = {.description = "the shortfall, or nothing when held"},
    },
    {
        .input = {.name = "12.01 past Kayden and Kirk to west of Koga and 12.02 to the Warden and the nurse", .body = test_gym_rows},
        .expectation = {.description = "Kayden and Kirk fought where the table steps into their lines, no one else; ends at Fuchsia's nurse"},
    },
    {
        .input = {.name = "the lead's attacks in Fuchsia Gym", .body = test_lead_moves_in_gym},
        .expectation = {.description = "Surf against Koga's Pokemon, Surf or Bite against the Jugglers'"},
    },
    {
        .input = {.name = "Dig is not picked against Levitate", .body = test_dig_not_picked_against_levitate},
        .expectation = {.description = "Surf, not Dig, against all four of Koga's Pokemon"},
    },
    {
        .input = {.name = "foe abilities that grant immunity", .body = test_ability_immunities},
        .expectation = {.description = "Levitate, Flash Fire, Volt Absorb and Water Absorb species; Dig and Fly take two turns"},
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
