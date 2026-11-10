/*  AutoStory Vermilion Gym Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Rows 00.23 to 00.25 (StoryData/frlg-00.23..25.plan.json) on the
 *  overworld sim: the tree cut on the way in and out, the gym's three
 *  trainers fought where the table says, the trash can search walking
 *  between the cans clear of where the trainers stand after their battles
 *  (the switches found at once, and after a wrong can's reset), and Lt.
 *  Surge reached through the beams. And the sim's chance against Surge.
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
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMaps.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldRoute.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldSim.h"
#include "PokemonFRLG_AutoStoryDamage.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryGrind.h"
#include "PokemonFRLG_AutoStoryParty.h"
#include "PokemonFRLG_AutoStoryRowSim.h"
#include "PokemonFRLG_AutoStoryTable.h"
#include "PokemonFRLG_AutoStoryTrashCans.h"
#include "PokemonFRLG_AutoStoryWinChance.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

class QuietLogger : public Logger{
public:
    using Logger::log;
    virtual void log(const std::string&, Color) override{}
};

struct AutoStoryVermilionGymInput{
    std::string name;
    std::function<UnitTestResult()> body;
};

struct AutoStoryVermilionGymExpectation{
};

struct AutoStoryVermilionGymCase{
    AutoStoryVermilionGymInput input;
    AutoStoryVermilionGymExpectation expectation;
};

class Test_AutoStoryVermilionGym : public UnitTest{
public:
    explicit Test_AutoStoryVermilionGym(AutoStoryVermilionGymCase test_case)
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
    AutoStoryVermilionGymCase m_case;
};

const std::string GYM = "VermilionCity_Gym";
const std::string ROUTE6 = "Route6";
const std::string VERMILION = "VermilionCity";
const std::string VERMILION_CENTER = "VermilionCity_PokemonCenter_1F";

const StoryCheckpoint& checkpoint(std::string_view id){
    const std::vector<StoryCheckpoint>& rows = story_checkpoints();
    const auto row = std::find_if(rows.begin(), rows.end(), [&](const StoryCheckpoint& data){ return data.id == id; });
    if (row == rows.end()){
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("No checkpoint {}.", id));
    }
    return *row;
}

size_t step_index(const StoryCheckpoint& row, const std::string& name){
    const auto step = std::find_if(row.steps.begin(), row.steps.end(), [&](const StoryStep& data){ return data.name == name; });
    if (step == row.steps.end()){
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "No step " + row.id + "/" + name + ".");
    }
    return step - row.steps.begin();
}

std::string beaten_list(const OverworldSim& sim){
    std::string text;
    for (const std::string& name : sim.state().trainers_beaten){
        text += " " + name;
    }
    return text.empty() ? " none" : text;
}

MapData& map_named(std::vector<MapData>& maps, const std::string& name){
    for (MapData& map : maps){
        if (map.name == name){
            return map;
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "No map " + name + ".");
}

//  `maps` with the trainers beaten that rows before 00.24 fought: Route
//  6's Jeff (00.21) and Vermilion Gym's three (00.23).
std::vector<MapData> maps_after_gym(std::vector<MapData> maps = story_maps()){
    for (MapObject& object : map_named(maps, GYM).objects){
        object.trainer.reset();
    }
    for (MapObject& object : map_named(maps, ROUTE6).objects){
        if (object.name == "camper" && object.trainer){
            object.trainer.reset();
        }
    }
    return maps;
}

//  maps_after_gym() with Oak's aide on Vermilion City's (25, 7): Lt.
//  Surge's win shows him (VermilionCity_Gym_EventScript_ShowOaksAide),
//  and he stays until talked to (VermilionCity_EventScript_HideOaksAide).
std::vector<MapData> maps_after_surge(){
    const std::vector<MapData> all = overworld_maps();
    std::set<std::string> flags = map_flag_set(all, "after_hall_of_fame");
    flags.merge(map_flag_set(all, "new_game"));
    flags.erase("FLAG_HIDE_VERMILION_CITY_OAKS_AIDE");
    return maps_after_gym(overworld_maps(flags));
}

//  The walk to `can`'s stand on `sim` as the trash can step plans it, on
//  trash_can_maps(): ends there facing the can, or says why not.
std::string walk_to_stand(OverworldSim& sim, StoryPose& at, int can){
    static const std::vector<MapData> MAPS = trash_can_maps(story_maps());
    const TrashCanStand stand = trash_can_stand(can);
    const MapPosition start{at.map, at.tile};
    const std::optional<std::vector<RouteStep>> route = plan_overworld_route(
        MAPS, start, RouteGoal{{GYM, stand.tile}, stand.facing}, story_route_costs(), at.facing
    );
    if (!route){
        return std::format("no route from {} to can {}.", to_string(start), can);
    }
    if (!route->empty()){
        const RoutePlanning planning{.maps = &MAPS, .costs = story_route_costs(), .start = start, .label = "can"};
        if (const std::string failure = OverworldSimWalker(sim).run(*route, planning); !failure.empty()){
            return std::format("to can {}: ", can) + failure;
        }
    }
    at = StoryPose{GYM, stand.tile, stand.facing};
    if (!(sim.state().position == MapPosition{GYM, stand.tile}) || sim.state().facing != stand.facing){
        return std::format("not at can {}'s stand facing it: on {}.", can, to_string(sim.state().position));
    }
    if (!sim.state().violations.empty()){
        return std::format("to can {}: {}", can, sim.state().violations.back());
    }
    return "";
}

//  A step of a row on the sim: row_step_on_sim(), but Surge's battle,
//  which a talk starts, leaves the player where the table says. The TEACH,
//  the GRIND and the talks the sim hasn't got pass.
std::string gym_step_on_sim(OverworldSim& sim, StoryPose& at, const StoryStep& step, const std::string& id){
    if (step.kind == StoryStepKind::BATTLE && step.name == "lt_surge"){
        at = *story_pose(Pokemon::pose_of(*step.pose_after));
        return "";
    }
    return row_step_on_sim(sim, at, step, id);
}

using Switches = std::pair<int, int>;

//  The gym's cans as its scripts answer them, each checked from its stand,
//  walked to on the sim as the program plans it.
class SimGym{
public:
    SimGym(OverworldSim& sim, StoryPose& at, std::vector<Switches> draws)
        : m_sim(sim), m_at(at), m_draws(std::move(draws))
    {
        next_draw();
    }

    CanPages check(int can){
        error = walk_to_stand(m_sim, m_at, can);
        if (!error.empty()){
            return CanPages::FAILED;
        }
        if (open){
            return CanPages::ONE;
        }
        if (!m_first_found){
            m_first_found = can == m_switches.first;
            return m_first_found ? CanPages::TWO : CanPages::ONE;
        }
        if (can == m_switches.second){
            open = true;
            return CanPages::ONE;
        }
        m_first_found = false;
        next_draw();
        return CanPages::TWO;
    }

    std::string error;
    bool open = false;

private:
    void next_draw(){
        //  Past the script: one the search's guess finds.
        m_switches = m_drawn < m_draws.size() ? m_draws[m_drawn++] : Switches{8, 7};
    }

    OverworldSim& m_sim;
    StoryPose& m_at;
    std::vector<Switches> m_draws;
    size_t m_drawn = 0;
    Switches m_switches;
    bool m_first_found = false;
};

//  Row 00.23 from steps `first` to `last` (exclusive) on `sim`, the trash
//  cans drawn as `draws` say.
std::string run_gym_steps(OverworldSim& sim, StoryPose& at, size_t first, size_t last, const std::vector<Switches>& draws){
    const StoryCheckpoint& row = checkpoint("00.23");
    for (size_t c = first; c < last; c++){
        const StoryStep& step = row.steps[c];
        if (step.action == TRASH_CANS_VARIANT){
            QuietLogger logger;
            SimGym gym(sim, at, draws);
            const TrashCanResult result = run_trash_cans(logger, [&](int can){ return gym.check(can); });
            if (result.end != TrashCanEnd::OPENED || !gym.open){
                return "00.23/trash_cans: " + std::string(trash_can_end_name(result.end)) + " " + gym.error;
            }
            continue;
        }
        if (const std::string failure = gym_step_on_sim(sim, at, step, row.id); !failure.empty()){
            return failure;
        }
    }
    return "";
}

//  00.23 on the sim from the nurse to the nurse: the tree cut, Dwayne,
//  Baily and Tucker fought in that order where the table says and no one
//  else, the cans searched, out past the tree grown back. The switches
//  found at once (under 3 and 4), and after a wrong guess's reset (under
//  3 and 8, then 13 and 14).
UnitTestResult test_gym_row(){
    const StoryCheckpoint& row = checkpoint("00.23");
    for (const std::vector<Switches>& draws : {std::vector<Switches>{{3, 4}}, std::vector<Switches>{{3, 8}, {13, 14}}}){
        StoryPose at = *story_pose(Pokemon::pose_of(*row.start));
        OverworldSim sim(story_maps(), MapPosition{at.map, at.tile}, *at.facing);
        if (const std::string failure = run_gym_steps(sim, at, 0, row.steps.size(), draws); !failure.empty()){
            return failure + " Beat" + beaten_list(sim) + ".";
        }
        const std::vector<std::string> expected{"sailor", "balding_man", "gentleman"};
        if (sim.state().trainers_beaten != expected || !sim.state().violations.empty()){
            return "00.23: expected Dwayne, Baily and Tucker beaten and no violation; beat" + beaten_list(sim) + ".";
        }
        if (!(sim.state().position == MapPosition{VERMILION_CENTER, {7, 4}})){
            return "00.23: expected to end at Vermilion's nurse, not " + to_string(sim.state().position) + ".";
        }
    }
    return true;
}

//  From where Tucker's battle leaves the player, with the three where
//  their battles leave them: every can's stand reached from (5, 8) and from
//  every other stand, and the way out to the nurse from every stand.
UnitTestResult test_cans_from_everywhere(){
    const StoryCheckpoint& row = checkpoint("00.23");
    StoryPose at = *story_pose(Pokemon::pose_of(*row.start));
    OverworldSim sim(story_maps(), MapPosition{at.map, at.tile}, *at.facing);
    const size_t cans = step_index(row, "trash_cans");
    if (const std::string failure = run_gym_steps(sim, at, 0, cans, {}); !failure.empty()){
        return failure;
    }
    for (int from = 1; from <= TRASH_CAN_COUNT; from++){
        OverworldSim there = sim;
        StoryPose here = at;
        if (const std::string failure = walk_to_stand(there, here, from); !failure.empty()){
            return "From Tucker's battle: " + failure;
        }
        for (int to = 1; to <= TRASH_CAN_COUNT; to++){
            OverworldSim walked = there;
            StoryPose pose = here;
            if (const std::string failure = walk_to_stand(walked, pose, to); !failure.empty()){
                return std::format("From can {}: ", from) + failure;
            }
            if (walked.state().trainers_beaten.size() != 3){
                return std::format("Can {} to can {}: a battle on the way.", from, to);
            }
        }
        OverworldSim out = there;
        if (const std::string failure = run_gym_steps(out, here, cans + 1, row.steps.size(), {}); !failure.empty()){
            return std::format("Out from can {}: ", from) + failure;
        }
        if (!(out.state().position == MapPosition{VERMILION_CENTER, {7, 4}}) || !out.state().violations.empty()){
            return std::format("Out from can {}: expected the nurse and no violation, on {}.", from, to_string(out.state().position));
        }
    }
    return true;
}

//  The tree shuts the gym's yard: no way from above it to the door.
UnitTestResult test_tree_shuts_the_yard(){
    const RouteGoal door{{GYM, {5, 19}}, std::nullopt};
    if (plan_overworld_route(story_maps(), MapPosition{VERMILION, {19, 23}}, door, story_route_costs(), Direction::SOUTH)){
        return "Planned a way into the gym past the uncut tree (19, 24).";
    }
    if (!plan_overworld_route(story_maps(), MapPosition{VERMILION, {19, 24}}, door, story_route_costs(), Direction::SOUTH)){
        return "No way from the tree's tile to the gym.";
    }
    //  The sim: no step onto the tree; cut, it's gone; a warp grows it back.
    OverworldSim sim(story_maps(), MapPosition{VERMILION, {19, 23}}, Direction::SOUTH);
    sim.tap(Direction::SOUTH);
    if (!(sim.state().position == MapPosition{VERMILION, {19, 23}}) || !sim.cut_tree()){
        return "Expected the tree to stop the player, then be cut.";
    }
    sim.tap(Direction::SOUTH);
    if (!(sim.state().position == MapPosition{VERMILION, {19, 24}})){
        return "Expected a step onto the cut tree's tile.";
    }
    return true;
}

//  00.24 and 00.25 on the sim, the gym as 00.23 leaves it: to Route 6's
//  grass round Isabelle's line and back, to below Surge through the beams,
//  and from him back out past the tree to the nurse, meeting no one.
UnitTestResult test_to_surge_and_out(){
    for (const char* id : {"00.24", "00.25"}){
        const StoryCheckpoint& row = checkpoint(id);
        StoryPose at = *story_pose(Pokemon::pose_of(*row.start));
        OverworldSim sim(maps_after_gym(), MapPosition{at.map, at.tile}, *at.facing);
        for (const StoryStep& step : row.steps){
            if (const std::string failure = gym_step_on_sim(sim, at, step, row.id); !failure.empty()){
                return failure;
            }
        }
        if (!sim.state().trainers_beaten.empty() || !sim.state().violations.empty()){
            return std::string(id) + ": a battle or a violation on the way.";
        }
        const MapPosition end = std::string(id) == "00.24" ? MapPosition{GYM, {5, 3}} : MapPosition{VERMILION_CENTER, {7, 4}};
        if (!(sim.state().position == end)){
            return std::string(id) + ": ended on " + to_string(sim.state().position) + ", not " + to_string(end) + ".";
        }
    }
    const StoryPose next = *story_pose(Pokemon::pose_of(*checkpoint("00.25").start));
    if (!(next.tile == TilePos{5, 3}) || next.map != GYM){
        return "00.25 doesn't start where 00.24 ends.";
    }
    return true;
}

//  Console B, 00.25 (2026-11-03 18:17): the walk to the nurse went up
//  column 27 to row 7 and west along it, into Oak's aide on (25, 7), there
//  since Surge's win; the leg was taken as walked and the step into the
//  Center's door went into a house's wall. 00.25 from Surge to the nurse
//  with the aide there, meeting no one.
UnitTestResult test_out_past_oaks_aide(){
    const StoryCheckpoint& row = checkpoint("00.25");
    const std::vector<MapData> maps = maps_after_surge();
    const MapData& city = *std::find_if(maps.begin(), maps.end(), [](const MapData& map){ return map.name == VERMILION; });
    if (object_at(city, {25, 7}) == nullptr){
        return "Expected Oak's aide on (25, 7) after Lt. Surge.";
    }
    StoryPose at = *story_pose(Pokemon::pose_of(*row.start));
    OverworldSim sim(maps, MapPosition{at.map, at.tile}, *at.facing);
    for (const StoryStep& step : row.steps){
        if (const std::string failure = gym_step_on_sim(sim, at, step, row.id); !failure.empty()){
            return failure;
        }
    }
    const MapPosition nurse{VERMILION_CENTER, {7, 4}};
    if (!sim.state().violations.empty() || !(sim.state().position == nurse)){
        return "Expected the nurse reached past Oak's aide; on " + to_string(sim.state().position) + (sim.state().violations.empty() ? "." : ", " + sim.state().violations.back());
    }
    return true;
}

//  The story's lead before Surge: Wartortle at `level` with the S.S.
//  Anne's moves (00.22), stats at OWN_GENES.
PartyMember surge_lead(uint8_t level){
    const Stats stats = combatant("wartortle", level, OWN_GENES)->stats;
    std::vector<SummaryMove> moves;
    for (const char* move : {"bite", "water-pulse", "bubble", "withdraw"}){
        moves.push_back(SummaryMove{.move = move, .pp = max_pp(move), .max_pp = max_pp(move)});
    }
    return member_from_summary("wartortle", level, moves, stats);
}

//  Lt. Surge against the lead with 14 Potions: Raichu's Shock Wave,
//  Thunder Wave and Double Team beat Water Pulse and Bite (3% at Lv28);
//  TM28 Dig in place of Withdraw takes it to 27% at Lv28 and 61% at Lv30,
//  past the GRIND's gate (00.23, 00.24). The battles still lost are to the
//  Raichu. The gym's three trainers are no risk.
UnitTestResult test_surge_chance(){
    const SimTrainer surge = *sim_trainer("TRAINER_LEADER_LT_SURGE");
    const SimItems potions{.potions = 14};
    const auto chance = [&](uint8_t level, bool dig){
        SimBattler own = *own_battler(surge_lead(level));
        if (dig){
            own.moves[3] = SimMove{.move = "dig", .pp = 10};
        }
        return estimate_win_chance(own, potions, surge);
    };
    const WinEstimate plain = chance(28, false);
    const WinEstimate dug = chance(28, true);
    const WinEstimate dug30 = chance(30, true);
    if (plain.chance > 0.15 || dug.chance < 0.2 || dug.chance > 0.4 || dug30.chance < 0.5){
        return std::format(
            "Expected under 15% at Lv28, 20-40% with Dig, 50% or more at Lv30; got {:.1f}%, {:.1f}% ({}), {:.1f}%.",
            100 * plain.chance, 100 * dug.chance, loss_text(dug), 100 * dug30.chance
        );
    }
    const auto& by_foe = dug.losses.by_foe;
    if (!by_foe.contains("raichu") || 10 * by_foe.at("raichu") < 9 * (dug.battles - dug.wins)){
        return "Expected the battles lost with Dig lost to the Raichu, got \"" + loss_text(dug) + "\".";
    }
    for (const char* trainer : {"TRAINER_SAILOR_DWAYNE", "TRAINER_ENGINEER_BAILY", "TRAINER_GENTLEMAN_TUCKER"}){
        if (estimate_win_chance(*own_battler(surge_lead(28)), potions, *sim_trainer(trainer)).chance < 0.9){
            return std::string("Expected the lead to beat ") + trainer + " 90% of the time or more.";
        }
    }
    return true;
}

//  Console B, m73 (2026-11-03): the Raichu stayed at full HP for six
//  turns while the Dig missed. pret AI_CV_EvasionUp gives Double Team +3
//  at 90% HP or more, 156 times in 256, over Shock Wave and Thunder Wave
//  (100 each); AI_CBM_EvasionUp takes 10 off at +6 evasion.
UnitTestResult test_surge_raichu_double_team(){
    const SimTrainer surge = *sim_trainer("TRAINER_LEADER_LT_SURGE");
    const SimBattler& raichu = surge.party[2];
    const SimBattler lead = *own_battler(surge_lead(29));
    if (raichu.moves[2].move != "double-team"){
        return "Expected the Raichu's third move to be Double Team, got " + raichu.moves[2].move + ".";
    }
    const double full = ai_move_chances(surge, raichu, lead)[2];
    SimBattler dodging = raichu;
    dodging.body.stages[static_cast<size_t>(Stage::EVASION)] = 6;
    const double maxed = ai_move_chances(surge, dodging, lead)[2];
    if (full < 0.65 || maxed > 0){
        return std::format("Expected Double Team 65% of the time or more at full HP, never at +6; got {:.0f}% and {:.0f}%.", 100 * full, 100 * maxed);
    }
    return true;
}

//  m73's lead as its summary showed it before Lt. Surge, `levels` more.
PartyMember m73_lead(uint8_t levels = 0){
    std::vector<SummaryMove> moves;
    for (const char* move : {"bite", "water-pulse", "bubble", "dig"}){
        moves.push_back(SummaryMove{.move = move, .pp = max_pp(move), .max_pp = max_pp(move)});
    }
    PartyMember lead = member_from_summary("wartortle", 29, moves, Stats{83, 44, 59, 51, 64, 49});
    lead.moves_through_level = static_cast<uint8_t>(29 + levels);
    return lead;
}

//  m73: each Dig took two turns (18 s from menu to menu, Bite 10 s), and
//  the Raichu's Double Team lands while the lead is under ground. The sim
//  played Dig as one turn and gave Double Team no CHECK_VIABILITY: 79% for
//  m73's lead and 14 Potions, lost on the console. Now 58%, all lost to
//  the Raichu; at Lv31 the lead outspeeds it (82%).
UnitTestResult test_surge_dig_two_turns(){
    const SimItems potions{.potions = 14};
    const WinEstimate m73 = *story_win_chance(m73_lead(), potions, 1.0, "TRAINER_LEADER_LT_SURGE");
    const WinEstimate lv31 = *story_win_chance(m73_lead(2), potions, 1.0, "TRAINER_LEADER_LT_SURGE");
    const auto& by_foe = m73.losses.by_foe;
    const bool raichu = by_foe.contains("raichu") && by_foe.at("raichu") == m73.battles - m73.wins;
    if (m73.chance < 0.45 || m73.chance > 0.7 || !raichu || lv31.chance < 0.75){
        return std::format(
            "Expected m73's lead at 45-70%, every loss to the Raichu, 75% or more at Lv31; got {:.1f}% ({}), {:.1f}%.",
            100 * m73.chance, loss_text(m73), 100 * lv31.chance
        );
    }
    return true;
}

//  00.24 with a lead that beats Lt. Surge already: the check in the Center
//  comes first, and with its flag set the walk to Route 6's grass, the
//  grind and the way back are skipped; the rest goes from the nurse to
//  below Surge on the sim. Console B walked out and back at Lv29, 79%
//  (2026-11-03).
UnitTestResult test_surge_winnable_skips_grind(){
    const StoryCheckpoint& row = checkpoint("00.24");
    const StoryStep& check = row.steps.front();
    if (check.action != GRIND_CHECK_ACTION || !grind_plan_of(check.args)){
        return "Expected 00.24 to start with the grind's check, not " + check.name + ".";
    }
    StoryPose at = *story_pose(Pokemon::pose_of(*row.start));
    OverworldSim sim(maps_after_gym(), MapPosition{at.map, at.tile}, *at.facing);
    std::vector<std::string> skipped;
    for (const StoryStep& step : row.steps){
        if (step.skip_if == std::vector<std::string>{check.name}){
            skipped.push_back(step.name);
            continue;
        }
        if (const std::string failure = gym_step_on_sim(sim, at, step, row.id); !failure.empty()){
            return failure;
        }
    }
    const std::vector<std::string> expected{
        "route6_south_path", "round_isabelle_line", "grind_for_surge", "west_on_row_31", "down_to_jeff_row", "vermilion_nurse", "heal_vermilion",
    };
    if (skipped != expected || !(sim.state().position == MapPosition{GYM, {5, 3}})){
        return std::format("Expected the trip to Route 6 skipped and below Surge reached; {} skipped, on {}.", skipped.size(), to_string(sim.state().position));
    }
    return true;
}
} // namespace


const AutoStoryVermilionGymCase AUTO_STORY_VERMILION_GYM_CASES[] = {
    {
        .input = {"00.23 the tree shuts the gym's yard until cut", test_tree_shuts_the_yard},
        .expectation = {},
    },
    {
        .input = {"00.23 Vermilion Gym: Dwayne, Baily and Tucker, the cans at once or after a reset", test_gym_row},
        .expectation = {},
    },
    {
        .input = {"00.23 every can's stand from Tucker's tile and from every other, and out", test_cans_from_everywhere},
        .expectation = {},
    },
    {
        .input = {"00.24 and 00.25 through the beams to Lt. Surge and out", test_to_surge_and_out},
        .expectation = {},
    },
    {
        .input = {"00.25 out to the nurse past Oak's aide on (25, 7)", test_out_past_oaks_aide},
        .expectation = {},
    },
    {
        .input = {"Lt. Surge: Dig over Withdraw, the sim's chance", test_surge_chance},
        .expectation = {},
    },
    {
        .input = {"Lt. Surge: the Raichu's Double Team at full HP", test_surge_raichu_double_team},
        .expectation = {},
    },
    {
        .input = {"Lt. Surge: Dig takes two turns, m73's lead under 79%", test_surge_dig_two_turns},
        .expectation = {},
    },
    {
        .input = {"00.24 with Lt. Surge winnable already: no trip to Route 6", test_surge_winnable_skips_grind},
        .expectation = {},
    },
};

void add_tests_AutoStoryVermilionGym(UnitTestDatabase& database){
    for (const auto& item : AUTO_STORY_VERMILION_GYM_CASES){
        database.add<Test_AutoStoryVermilionGym>(item);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
