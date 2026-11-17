/*  AutoStory League Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Rows 20.01a and 20.01 (StoryData/frlg-20.01*.plan.json) on the
 *  overworld sim: Indigo Plateau's Mart, the Elite Four's rooms one way
 *  north, the Champion's door, all in 20.01 (one checkpoint, no save
 *  between the fights); the battles' foe rows against their generator; and
 *  the League plan's steps around the fights.
 *
 */

#include <cstdint>
#include <format>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Pokemon_BuiltinStoryActions.h"
#include "Pokemon/AutoStory/Pokemon_StoryPostconditionCheck.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMaps.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldSim.h"
#include "PokemonFRLG_AutoStoryBattlePolicy.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryItemsPocket.h"
#include "PokemonFRLG_AutoStoryLeaguePlan.h"
#include "PokemonFRLG_AutoStoryPoses.h"
#include "PokemonFRLG_AutoStoryRowSim.h"
#include "PokemonFRLG_AutoStoryRows20_Tests.h"
#include "PokemonFRLG_AutoStoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

struct AutoStoryLeagueInput{
    std::function<UnitTestResult()> body;
};

struct AutoStoryLeagueExpectation{
    std::string_view description;
};

struct AutoStoryLeagueCase{
    std::string name;
    AutoStoryLeagueInput input;
    AutoStoryLeagueExpectation expectation;
};

class Test_AutoStoryLeague : public UnitTest{
public:
    explicit Test_AutoStoryLeague(AutoStoryLeagueCase test_case)
        : UnitTest(std::format("PokemonFRLG::AutoStory - Simulation: {}", test_case.name))
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
    AutoStoryLeagueCase m_case;
};

const StoryCheckpoint& league_row(std::string_view id){
    for (const StoryCheckpoint& row : story_checkpoints()){
        if (row.id == id){
            return row;
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("No checkpoint {}.", id));
}

//  The step `name` of row `id`.
const StoryStep& league_step(std::string_view id, std::string_view name){
    for (const StoryStep& step : league_row(id).steps){
        if (step.name == name){
            return step;
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("No step {} in {}.", name, id));
}

//  Runs the walking steps of `row` on `sim`, from `at`, after the step
//  named `from` (from the row's first if empty) up to (not including) the
//  step named `until`, or all of them if `until` is empty. The other steps
//  (the Mart, the level checks, the heals, the scenes and the battles) are
//  the console's.
std::string walk_league_row(
    const StoryCheckpoint& row, std::string_view until, OverworldSim& sim, StoryPose& at, std::string_view from = {}
){
    bool started = from.empty();
    for (const StoryStep& step : row.steps){
        if (!started){
            started = step.name == from;
            continue;
        }
        if (step.name == until){
            return "";
        }
        if (step.kind != StoryStepKind::ROUTE && step.kind != StoryStepKind::LEGS && step.kind != StoryStepKind::STEP){
            continue;
        }
        if (const std::string failure = row_step_on_sim(sim, at, step, row.id); !failure.empty()){
            return failure;
        }
    }
    return until.empty() ? "" : std::format("{}: no step {}.", row.id, until);
}

//  Walks row `id` from its start to the step `until` (the room's entry
//  scene) and checks the door before it took the player into `room`, with
//  no trainer met on the way.
UnitTestResult walk_to_room(std::string_view id, std::string_view until, std::string_view room){
    const StoryCheckpoint& row = league_row(id);
    StoryPose at = *story_pose(Pokemon::pose_of(*row.start));
    OverworldSim sim(story_maps(), MapPosition{at.map, at.tile}, *at.facing);
    if (const std::string failure = walk_league_row(row, until, sim, at); !failure.empty()){
        return failure;
    }
    if (sim.state().position.map != room){
        return std::format("{}: the door led to {}, not {}.", id, to_string(sim.state().position), room);
    }
    if (!sim.state().trainers_beaten.empty() || !sim.state().violations.empty()){
        return std::format("{}: met a trainer or broke a rule on the way.", id);
    }
    return true;
}

//  Walks 20.01 from in front of the trainer its battle step `battle` beat
//  (the step's declared pose) round them to below their door, up to the
//  step `door` into it. The map pack draws the door closed (a wall: the
//  game opens it with setmetatile when the trainer is beaten,
//  PokemonLeague_EventScript_OpenDoor), so the step through it and the
//  next room's walk are the console's.
UnitTestResult walk_to_door(std::string_view battle, std::string_view door, const MapPosition& below){
    const std::string_view id = "20.01";
    const StoryCheckpoint& row = league_row(id);
    const StoryStep& beaten = league_step(id, battle);
    if (!beaten.pose_after){
        return std::format("{} {}: no declared pose after the battle.", id, battle);
    }
    StoryPose at = *story_pose(Pokemon::pose_of(*beaten.pose_after));
    OverworldSim sim(story_maps(), MapPosition{at.map, at.tile}, *at.facing);
    if (const std::string failure = walk_league_row(row, door, sim, at, battle); !failure.empty()){
        return failure;
    }
    if (!(sim.state().position == below) || sim.state().facing != Direction::NORTH){
        return std::format("{}: ended on {}, not below the door {} facing north.", id, to_string(sim.state().position), to_string(below));
    }
    if (!sim.state().trainers_beaten.empty() || !sim.state().violations.empty()){
        return std::format("{}: met a trainer or broke a rule on the way.", id);
    }
    return true;
}

//  20.01a: the lobby to the Mart's counter, where 20.01 starts.
UnitTestResult test_indigo_mart_counter(){
    const StoryCheckpoint& shop = league_row("20.01a");
    StoryPose at = *story_pose(Pokemon::pose_of(*shop.start));
    OverworldSim sim(story_maps(), MapPosition{at.map, at.tile}, *at.facing);
    if (const std::string failure = walk_league_row(shop, "", sim, at); !failure.empty()){
        return failure;
    }
    const MapPosition counter{"IndigoPlateau_PokemonCenter_1F", {2, 7}};
    if (!(sim.state().position == counter)){
        return std::format("20.01a: ended on {}, not the Mart's counter {}.", to_string(sim.state().position), to_string(counter));
    }
    const StoryPose next = *story_pose(Pokemon::pose_of(*league_row("20.01").start));
    if (next.map != counter.map || !(next.tile == counter.point)){
        return "20.01 doesn't start where 20.01a ends.";
    }
    return true;
}

//  The battle's foe_threats are PokemonSimulation's planning_risk() for
//  each foe leading against the entry state of its pokeguy case
//  (src/pokesim/games/frlg/battle/player_strategy/planning.py, as 19.01's
//  rival): speed, the 80th percentile loss, the mean (rounded), the worst
//  case and the worst normal hit, then whether the foe lowers accuracy
//  (Sand Attack) and whether it explodes.
UnitTestResult check_foe_threats(std::string_view id, std::string_view battle, const std::vector<std::vector<int64_t>>& expected){
    const StoryStep& step = league_step(id, battle);
    const auto found = step.args.find("foe_threats");
    const auto* rows = found == step.args.end() ? nullptr : std::get_if<std::vector<std::vector<int64_t>>>(&found->second);
    if (rows == nullptr || *rows != expected){
        return std::format("{} {}: foe_threats are not the generator's rows.", id, battle);
    }
    return true;
}

//  A League battle and its generator's rows.
struct LeagueFight{
    std::string_view id;
    std::string_view battle;
    std::vector<std::vector<int64_t>> rows;
};

//  The generator's rows for the five League battles, in the doc's foe order,
//  against console B's Lv54 Blastoise (171 HP, Def 135, Sp. Def 150).
std::vector<LeagueFight> league_fights(){
    return {
        {"20.01", "lorelei", {
            {93, 17, 10, 37, 30, 0, 0},     //  Dewgong: worst normal hit 30 as seen on console B, not 18
            {91, 0, 0, 0, 12, 0, 0, 0, 1},  //  Cloyster (Dive charges a turn)
            {45, 21, 7, 49, 24, 0, 0},      //  Slowbro
            {110, 22, 10, 48, 24, 0, 0},    //  Jynx
            {94, 28, 12, 62, 31, 0, 0},     //  Lapras
        }},
        {"20.01", "bruno", {
            {81, 26, 16, 63, 31, 0, 0, 1},  //  Onix (Roar)
            {101, 43, 24, 108, 54, 0, 0},   //  Hitmonchan
            {113, 54, 44, 114, 57, 0, 0},   //  Hitmonlee
            {96, 33, 33, 69, 34, 0, 0},     //  Onix
            {83, 0, 4, 150, 75, 0, 0},      //  Machamp
        }},
        {"20.01", "agatha", {
            {126, 0, 3, 60, 30, 0, 0},      //  Gengar
            {129, 28, 17, 60, 30, 0, 0},    //  Golbat
            {108, 0, 0, 0, 41, 0, 0},       //  Haunter
            {111, 51, 31, 108, 54, 0, 0},   //  Arbok
            {150, 40, 24, 84, 42, 0, 0},    //  Gengar
        }},
        {"20.01", "lance", {
            {112, 40, 31, 40, 80, 0, 0},    //  Gyarados
            {96, 40, 16, 78, 53, 0, 0},     //  Dragonair
            {96, 0, 0, 0, 53, 0, 0},        //  Dragonair
            {173, 41, 41, 84, 67, 0, 0},    //  Aerodactyl
            {107, 56, 19, 126, 98, 0, 0},   //  Dragonite
        }},
        {"20.01", "champion", {
            {130, 35, 24, 75, 37, 1, 0, 1}, //  Pidgeot (Sand Attack, Whirlwind)
            {159, 70, 64, 144, 72, 0, 0},   //  Alakazam
            {70, 0, 8, 183, 91, 0, 0},      //  Rhydon
            {118, 44, 43, 96, 48, 0, 0},    //  Gyarados
            {139, 45, 45, 92, 46, 0, 0, 1}, //  Arcanine (Roar)
            {125, 194, 182, 396, 198, 0, 0, 0, 1},  //  Venusaur (Solar Beam charges a turn)
        }},
    };
}

UnitTestResult test_foe_threats_are_the_generators(){
    for (const LeagueFight& fight : league_fights()){
        UnitTestResult result = check_foe_threats(fight.id, fight.battle, fight.rows);
        if (result.result != UnitTestResult::PASSED){
            return result;
        }
    }
    return true;
}

//  The Champion's Venusaur: the battle texts read before the turn, and
//  the turns (attacks) taken after them before this one.
struct VenusaurTurnInput{
    std::vector<std::string> battle_texts = {};
    int turns_after_texts = 0;
};

//  The policy's item, "" for an attack.
struct VenusaurTurnExpectation{
    std::string_view item;
};

struct VenusaurTurnCase{
    std::string_view name;
    VenusaurTurnInput input;
    VenusaurTurnExpectation expectation;
};

//  Solar Beam without sun charges a turn first: no KO risk until Venusaur
//  is charging (PokemonSimulation #110: the policy only attacked, and the
//  Champion was 0%). Charged, its hit KOs from full: attack.
const VenusaurTurnCase VENUSAUR_TURN_CASES[] = {
    {"not charging: X Special", {{}, 0}, {"x-special"}},
    {"charging Solar Beam: attack", {{"foevenusaurtookinsunlight"}, 0}, {""}},
    {"the turn after its charge: X Special", {{"foevenusaurtookinsunlight"}, 1}, {"x-special"}},
    {"in Sunny Day's sun: attack", {{"thesunlightgotbright"}, 0}, {""}},
    {"after the sun faded: X Special", {{"thesunlightgotbright", "thesunlightfaded"}, 0}, {"x-special"}},
};

//  Console B's Blastoise (Lv54, 2026-11-16) at full HP against Venusaur.
UnitTestResult test_venusaur_charge(){
    const StoryStep& step = league_step("20.01", "champion");
    constexpr size_t VENUSAUR = 5;
    for (const VenusaurTurnCase& test : VENUSAUR_TURN_CASES){
        Supplies supplies;
        set_from_bag(supplies, {{"full-restore", 10}, {"x-special", 10}, {"x-speed", 4}, {"x-accuracy", 3}});
        supplies.lead = PartyMember{
            .species = "blastoise",
            .moves = {{"bite", 25}, {"strength", 15}, {"surf", 15}, {"blizzard", 5}},
            .moves_through_level = 54,
            .moves_read = true,
            .level = 54,
            .stats = Stats{.hp = 171, .attack = 101, .defense = 135, .sp_attack = 119, .sp_defense = 150, .speed = 117},
        };
        supplies.foes = Pokemon::story_arg<std::vector<std::string>>(step.args, "foes");
        supplies.policy = battle_policy_of(step.args);
        PolicyState state(supplies.policy);
        PolicyView view{
            .hp = 171, .max_hp = 171, .status = StatusCondition::NONE, .speed = 117,
            .foe_index = VENUSAUR, .foe_fill = 1.0, .battle_texts = test.input.battle_texts,
        };
        for (int turn = 0; turn < test.input.turns_after_texts; turn++){
            state.choose(supplies, view);
            view.battle_texts.clear();
        }
        const std::optional<PolicyItem> item = state.choose(supplies, view).item;
        const std::string_view chosen = item ? std::string_view(item->item) : std::string_view();
        if (chosen != test.expectation.item){
            return std::format("Venusaur {}: expected \"{}\", got \"{}\".", test.name, test.expectation.item, chosen);
        }
    }
    return true;
}

//  The index of the step `name` in `row`, or the step count if none.
size_t step_index(const StoryCheckpoint& row, std::string_view name){
    size_t index = 0;
    while (index < row.steps.size() && row.steps[index].name != name){
        index++;
    }
    return index;
}

//  The League plan's steps in 20.01: league_start before the League's
//  door, and before each fight after Lorelei's a league_between_fights
//  for that fight's trainer, after the fight before it.
UnitTestResult test_league_plan_steps(){
    const StoryCheckpoint& row = league_row("20.01");
    const size_t start = step_index(row, "league_start");
    if (start >= step_index(row, "league_door") || row.steps[start].action != "league_start"){
        return "20.01: expected league_start before the League's door.";
    }
    struct Between{
        std::string_view after;
        std::string_view step;
        std::string_view battle;
        std::string_view trainer;
    };
    const Between betweens[] = {
        {"lorelei", "items_before_bruno", "bruno", "TRAINER_ELITE_FOUR_BRUNO"},
        {"bruno", "items_before_agatha", "agatha", "TRAINER_ELITE_FOUR_AGATHA"},
        {"agatha", "items_before_lance", "lance", "TRAINER_ELITE_FOUR_LANCE"},
        {"lance", "items_before_champion", "champion", "TRAINER_CHAMPION_FIRST_BULBASAUR"},
    };
    for (const Between& between : betweens){
        const size_t at = step_index(row, between.step);
        if (at <= step_index(row, between.after) || at >= step_index(row, between.battle)
            || row.steps[at].action != "league_between_fights"
            || Pokemon::story_arg<std::string>(row.steps[at].args, "next") != between.trainer
        ){
            return std::format("20.01: expected {} (league_between_fights, next {}) between {} and {}.", between.step, between.trainer, between.after, between.battle);
        }
        if (Pokemon::story_arg<std::string>(league_step("20.01", between.battle).args, "trainer") != between.trainer){
            return std::format("20.01 {}: expected trainer {}.", between.battle, between.trainer);
        }
    }
    return true;
}

const AutoStoryLeagueCase AUTO_STORY_LEAGUE_CASES[] = {
    {
        .name = "20.01 the Champion's Venusaur: Solar Beam is a KO risk only charged or in sun",
        .input = {.body = test_venusaur_charge},
        .expectation = {.description = "X Special until it charges or the sun is out, then an attack"},
    },
    {
        .name = "20.01a Indigo Plateau's Mart counter",
        .input = {.body = test_indigo_mart_counter},
        .expectation = {.description = "the counter (2, 7), where 20.01 starts"},
    },
    {
        .name = "20.01 the League's door, into Lorelei's room",
        .input = {.body = []{
            return walk_to_room("20.01", "enter_loreleis_room", "PokemonLeague_LoreleisRoom");
        }},
        .expectation = {.description = "Lorelei's room, no trainer met"},
    },
    {
        .name = "20.01 around Lorelei to below her door",
        .input = {.body = []{
            return walk_to_door("lorelei", "loreleis_door", MapPosition{"PokemonLeague_LoreleisRoom", {6, 3}});
        }},
        .expectation = {.description = "Lorelei's room (6, 3) facing north"},
    },
    {
        .name = "20.01 around Bruno to below his door",
        .input = {.body = []{
            return walk_to_door("bruno", "brunos_door", MapPosition{"PokemonLeague_BrunosRoom", {6, 3}});
        }},
        .expectation = {.description = "Bruno's room (6, 3) facing north"},
    },
    {
        .name = "20.01 around Agatha to below her door",
        .input = {.body = []{
            return walk_to_door("agatha", "agathas_door", MapPosition{"PokemonLeague_AgathasRoom", {6, 3}});
        }},
        .expectation = {.description = "Agatha's room (6, 3) facing north"},
    },
    {
        .name = "20.01 around Lance to below the Champion's door",
        .input = {.body = []{
            return walk_to_door("lance", "champions_door", MapPosition{"PokemonLeague_LancesRoom", {6, 6}});
        }},
        .expectation = {.description = "Lance's room (6, 6) facing north"},
    },
    {
        .name = "20.01 the League's foe rows are the generator's",
        .input = {.body = test_foe_threats_are_the_generators},
        .expectation = {.description = "PokemonSimulation planning_risk() rows"},
    },
    {
        .name = "20.01 the League plan's steps: league_start, then the items before each fight",
        .input = {.body = test_league_plan_steps},
        .expectation = {.description = "league_start before the door; league_between_fights before Bruno, Agatha, Lance and the Champion"},
    },
};

} // namespace


void add_tests_AutoStoryLeague(UnitTestDatabase& database){
    for (const auto& item : AUTO_STORY_LEAGUE_CASES){
        database.add<Test_AutoStoryLeague>(item);
    }
    add_tests_AutoStoryLeaguePlan(database);
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
