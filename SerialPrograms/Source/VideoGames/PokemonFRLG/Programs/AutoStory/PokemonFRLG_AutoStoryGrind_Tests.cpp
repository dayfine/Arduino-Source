/*  AutoStory Grind Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The GRIND step against a fake patch: wild battles that each give EXP,
 *  a lead that gets hurt after a few, a nurse to walk to, and the bounds
 *  and stops (a shiny, a blackout, a level that can't be read); and
 *  grinding until the sim's win chance against the next boss is enough,
 *  with a level cap.
 *
 */

#include <format>
#include <functional>
#include <string>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonFRLG_AutoStoryBattleSim.h"
#include "PokemonFRLG_AutoStoryGameArgs.h"
#include "PokemonFRLG_AutoStoryGrind.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

constexpr std::string_view RIVAL = "TRAINER_RIVAL_CERULEAN_BULBASAUR";

class QuietLogger : public Logger{
public:
    using Logger::log;
    virtual void log(const std::string&, Color) override{}
};

class FakePatch : public GrindActions{
public:
    unsigned level = 16;
    bool level_readable = true;
    //  Wins for each level gained.
    size_t wins_per_level = 2;
    size_t wins = 0;
    //  The lead is hurt after this many battles since the nurse.
    size_t hurt_after = 3;
    //  Encounters the patch has; then none start.
    size_t encounters_available = 100;
    //  The encounter (from 1) that is shiny, or that is lost; 0: none.
    size_t shiny_at = 0;
    size_t lost_at = 0;
    bool heal_fails = false;
    //  The sim's chance at each level; none: no estimate.
    std::function<std::optional<double>(unsigned level)> chance_at = {};

    size_t encounters = 0;
    size_t fights = 0;
    size_t heals = 0;
    size_t chances = 0;
    std::string error = {};

    std::optional<MemberInfo> read_member(size_t slot){
        if (m_in_battle){
            error = "read the level in a battle";
        }
        if (!level_readable || slot != 0){
            return std::nullopt;
        }
        return MemberInfo{.species = "squirtle", .level = level, .moves = {}};
    }
    virtual bool find_encounter(Pokemon::Direction) override{
        if (m_in_battle){
            error = "looked for a battle in a battle";
        }
        if (encounters >= encounters_available){
            return false;
        }
        encounters++;
        m_in_battle = true;
        return true;
    }
    virtual EncounterSeen read_encounter() override{
        return {.species = "pidgey", .shiny = encounters == shiny_at};
    }
    virtual GrindFight fight() override{
        if (!m_in_battle){
            error = "fought off a battle";
        }
        m_in_battle = false;
        fights++;
        m_since_heal++;
        if (encounters == lost_at){
            return GrindFight::LOST;
        }
        wins++;
        if (wins % wins_per_level == 0){
            level++;
        }
        return GrindFight::OVER;
    }
    virtual bool needs_heal() override{
        return m_since_heal >= hurt_after;
    }
    virtual bool heal_trip() override{
        if (m_in_battle){
            error = "walked to the nurse in a battle";
        }
        heals++;
        m_since_heal = 0;
        return !heal_fails;
    }
    virtual std::optional<WinEstimate> win_chance(const MemberInfo& member, std::string_view trainer) override{
        if (m_in_battle){
            error = "asked the sim in a battle";
        }
        if (trainer != RIVAL){
            error = "asked the sim about " + std::string(trainer);
        }
        chances++;
        const std::optional<double> chance = chance_at && member.level ? chance_at(*member.level) : std::nullopt;
        if (!chance){
            return std::nullopt;
        }
        return WinEstimate{.battles = 100, .wins = size_t(*chance * 100), .chance = *chance, .inputs = "a fake lead"};
    }

private:
    bool m_in_battle = false;
    size_t m_since_heal = 0;
};


struct AutoStoryGrindInput{
    std::string name;
    std::function<UnitTestResult()> body;
};

struct AutoStoryGrindExpectation{
};

struct AutoStoryGrindCase{
    AutoStoryGrindInput input;
    AutoStoryGrindExpectation expectation;
};

class Test_AutoStoryGrind : public UnitTest{
public:
    explicit Test_AutoStoryGrind(AutoStoryGrindCase test_case)
        : UnitTest("PokemonFRLG::AutoStoryGrind - " + test_case.input.name)
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
    AutoStoryGrindCase m_case;
};


struct GrindCase{
    std::string name = {};
    GrindPlan plan = {};
    std::function<void(FakePatch&)> setup = {};
    GrindEnd end = GrindEnd::REACHED;
    size_t encounters = 0;
    size_t fights = 0;
    size_t heals = 0;
    //  The level it ended at, if it matters.
    std::optional<unsigned> level = std::nullopt;
};

GrindPlan squirtle_to(unsigned level){
    return {.member = 0, .level = level, .max_encounters = 20, .pace = Pokemon::Direction::EAST, .heal_every = 0};
}

//  Until the rival is 70%, Lv`cap` at most.
GrindPlan beat_rival(unsigned cap){
    GrindPlan plan = squirtle_to(cap);
    plan.trainer = std::string(RIVAL);
    return plan;
}

//  40% at Lv16, 5% more a level, 72% from Lv19.
void winnable_from_19(FakePatch& patch){
    patch.chance_at = [](unsigned level) -> std::optional<double>{
        return level >= 19 ? 0.72 : 0.40 + 0.05 * (level - 16);
    };
}
void never_winnable(FakePatch& patch){
    patch.chance_at = [](unsigned) -> std::optional<double>{ return 0.50; };
}

std::vector<GrindCase> grind_cases_1(){
    return {
        {
            .name = "16 to 18: a heal when hurt, and one at the end",
            .plan = squirtle_to(18),
            .encounters = 4, .fights = 4, .heals = 2,
        },
        {
            .name = "already there: nothing fought",
            .plan = squirtle_to(16),
        },
        {
            .name = "a heal every two battles",
            .plan = [](){ GrindPlan plan = squirtle_to(18); plan.heal_every = 2; return plan; }(),
            .encounters = 4, .fights = 4, .heals = 2,
        },
        {
            .name = "not the lead",
            .plan = [](){ GrindPlan plan = squirtle_to(18); plan.member = 1; return plan; }(),
            .end = GrindEnd::REFUSED,
        },
        {
            .name = "a level that can't be read: nothing fought",
            .plan = squirtle_to(18),
            .setup = [](FakePatch& patch){ patch.level_readable = false; },
            .end = GrindEnd::UNREAD,
        },
    };
}

std::vector<GrindCase> grind_cases_2(){
    return {
        {
            .name = "out of encounters",
            .plan = [](){ GrindPlan plan = squirtle_to(30); plan.max_encounters = 3; return plan; }(),
            .end = GrindEnd::OUT_OF_ENCOUNTERS,
            .encounters = 3, .fights = 3, .heals = 0,
        },
        {
            .name = "a shiny stops it before its battle",
            .plan = squirtle_to(18),
            .setup = [](FakePatch& patch){ patch.shiny_at = 2; },
            .end = GrindEnd::SHINY,
            .encounters = 2, .fights = 1, .heals = 0,
        },
        {
            .name = "a blackout",
            .plan = squirtle_to(18),
            .setup = [](FakePatch& patch){ patch.lost_at = 2; },
            .end = GrindEnd::PLAYER_FAINTED,
            .encounters = 2, .fights = 2, .heals = 0,
        },
        {
            .name = "the trip to the nurse fails",
            .plan = squirtle_to(18),
            .setup = [](FakePatch& patch){ patch.heal_fails = true; },
            .end = GrindEnd::FAILED,
            .encounters = 3, .fights = 3, .heals = 1,
        },
        {
            .name = "no battle starts",
            .plan = squirtle_to(18),
            .setup = [](FakePatch& patch){ patch.encounters_available = 1; },
            .end = GrindEnd::NO_ENCOUNTER,
            .encounters = 1, .fights = 1, .heals = 0,
        },
    };
}

//  Grinding until the rival is winnable: the chance is asked before the
//  first battle and after each.
std::vector<GrindCase> grind_cases_3(){
    return {
        {
            .name = "until 70% against the rival: the first level that is",
            .plan = beat_rival(30),
            .setup = winnable_from_19,
            .encounters = 6, .fights = 6, .heals = 2,
            .level = 19,
        },
        {
            .name = "winnable already: nothing fought",
            .plan = beat_rival(30),
            .setup = [](FakePatch& patch){ patch.chance_at = [](unsigned){ return std::optional<double>(0.80); }; },
            .level = 16,
        },
        {
            .name = "winnable past the level cap: the chance decides",
            .plan = beat_rival(16),
            .setup = [](FakePatch& patch){ patch.chance_at = [](unsigned){ return std::optional<double>(0.80); }; },
            .level = 16,
        },
        {
            .name = "the level cap before the chance: stopped, no last heal",
            .plan = beat_rival(18),
            .setup = never_winnable,
            .end = GrindEnd::LEVEL_CAP,
            .encounters = 4, .fights = 4, .heals = 1,
            .level = 18,
        },
        {
            .name = "out of encounters before the chance",
            .plan = [](){ GrindPlan plan = beat_rival(30); plan.max_encounters = 3; return plan; }(),
            .setup = never_winnable,
            .end = GrindEnd::OUT_OF_ENCOUNTERS,
            .encounters = 3, .fights = 3, .heals = 0,
        },
        {
            .name = "no chance from the sim: nothing fought",
            .plan = beat_rival(30),
            .end = GrindEnd::NO_ESTIMATE,
        },
    };
}

std::vector<GrindCase> grind_cases(){
    std::vector<GrindCase> cases;
    for (std::vector<GrindCase> part : {grind_cases_1(), grind_cases_2(), grind_cases_3()}){
        cases.insert(cases.end(), part.begin(), part.end());
    }
    return cases;
}

UnitTestResult check_grind_case(const GrindCase& item){
    QuietLogger logger;
    FakePatch patch;
    if (item.setup){
        item.setup(patch);
    }
    const GrindResult result = run_grind(logger, patch, [&patch](size_t slot){
        return patch.read_member(slot);
    }, item.plan);
    if (!patch.error.empty()){
        return item.name + ": " + patch.error;
    }
    if (result.end != item.end){
        return std::format("{}: ended {}, expected {} ({}).", item.name,
            grind_end_name(result.end), grind_end_name(item.end), result.detail);
    }
    if (patch.encounters != item.encounters || patch.fights != item.fights || patch.heals != item.heals){
        return std::format("{}: {} encounters, {} fights, {} heals; expected {}, {}, {}.", item.name,
            patch.encounters, patch.fights, patch.heals, item.encounters, item.fights, item.heals);
    }
    if (item.level && result.level != item.level){
        return std::format("{}: ended at level {}, expected {}.", item.name, result.level.value_or(0), *item.level);
    }
    const size_t chances = item.plan.trainer.empty() ? 0 : patch.fights + 1;
    if (patch.chances != chances){
        return std::format("{}: the sim asked {} times, expected {}.", item.name, patch.chances, chances);
    }
    return true;
}

UnitTestResult test_grind(){
    for (const GrindCase& item : grind_cases()){
        const UnitTestResult result = check_grind_case(item);
        if (result.result != UnitTestResult::PASSED){
            return result;
        }
    }
    return true;
}


//  The cap's stop says what was short: the level, the chance and the
//  trainer.
UnitTestResult test_grind_cap_message(){
    QuietLogger logger;
    FakePatch patch;
    never_winnable(patch);
    const GrindPlan plan = beat_rival(18);
    const GrindResult result = run_grind(logger, patch, [&patch](size_t slot){
        return patch.read_member(slot);
    }, plan);
    const std::string message = grind_message(plan, result);
    const std::string expected =
        "Grind slot 1: level_cap: Lv18 is the cap, short of the win chance: level 18 of 18, win chance 50% of 70%"
        " against TRAINER_RIVAL_CERULEAN_BULBASAUR, 4 encounters, 1 trips to the nurse.";
    if (message != expected){
        return "Expected \"" + expected + "\", got \"" + message + "\".";
    }
    return true;
}


//  The lead the sim plays is the one read: its species, level, moves and
//  stats; the PP the battles left in a move it still knows, and a new
//  one's full.
UnitTestResult test_lead_as_read(){
    PartyMember tracked = new_member("wartortle", 22);
    tracked.moves = {{.move = "bite", .pp = 10}, {.move = "tail-whip", .pp = 30}, {.move = "bubble", .pp = 5}, {.move = "withdraw", .pp = 40}};
    const Stats stats{.hp = 68, .attack = 40, .defense = 50, .sp_attack = 41, .sp_defense = 49, .speed = 38};
    const MemberInfo read{.species = "wartortle", .level = 23, .moves = {"bite", "water-pulse", "bubble", "withdraw"}, .stats = stats};
    const PartyMember lead = lead_as_read(read, tracked);
    std::string moves;
    for (const KnownMove& move : lead.moves){
        moves += std::format("{} {}, ", move.move, move.pp);
    }
    if (moves != "bite 10, water-pulse 20, bubble 5, withdraw 40, "){
        return "Unexpected moves: " + moves;
    }
    if (lead.species != "wartortle" || lead.level != 23 || battle_level(lead) != 23 || !lead.moves_read
        || !lead.stats || lead.stats->hp != 68 || lead.stats->speed != 38
    ){
        return "Expected the species, level and stats read.";
    }
    return true;
}


//  Console B at the end of 00.15 (2026-11-03): Wartortle Lv22, 65 HP,
//  with Water Pulse taught over Tail Whip, and 10 Potions. The sim, the
//  levels since the summary added to its stats, wins against the
//  Cerulean rival about 42% at Lv22, 62% at Lv23, 66% at Lv24 and 76% at
//  Lv25: a GRIND to 70% stops at Lv25.
UnitTestResult test_console_party(){
    Stats stats = combatant("wartortle", 22, OWN_GENES)->stats;
    stats.hp = 65;
    const MemberInfo read{.species = "wartortle", .level = 22, .moves = {"bite", "water-pulse", "bubble", "withdraw"}, .stats = stats};
    const PartyMember at22 = lead_as_read(read, PartyMember{});
    for (uint8_t level = 22; level <= 25; level++){
        PartyMember lead = at22;
        lead.moves_through_level = level;
        const std::optional<WinEstimate> estimate = healed_win_chance(lead, 10, RIVAL);
        if (!estimate || (estimate->chance >= MIN_WIN_CHANCE) != (level == 25)){
            return std::format(
                "Lv{}: {:.1f}%; expected the first at {:.0f}% or more at Lv25.",
                level, estimate ? 100 * estimate->chance : 0.0, 100 * MIN_WIN_CHANCE
            );
        }
    }
    return true;
}


//  A GridPose for the nurse the args carry.
const Pokemon::GridPose NURSE{"Route4_PokemonCenter_1F", {{7, 4}}, Pokemon::Direction::NORTH};

UnitTestResult test_grind_args(){
    GrindPlan plan = squirtle_to(18);
    plan.max_encounters = 60;
    plan.pace = Pokemon::Direction::NORTH;
    plan.heal_every = 8;
    const Pokemon::StoryActionArgs args = grind_args(plan, NURSE);
    const std::optional<GrindPlan> back = grind_plan_of(args);
    if (!back || back->level != 18 || back->max_encounters != 60 || back->pace != Pokemon::Direction::NORTH
        || back->heal_every != 8 || back->member != 0 || !back->trainer.empty() || args.contains("chance")
    ){
        return "The level-18 plan didn't read back from its args.";
    }
    GrindPlan rival = beat_rival(30);
    rival.max_encounters = 100;
    rival.pace = Pokemon::Direction::WEST;
    Pokemon::StoryActionArgs rival_args = grind_args(rival, NURSE);
    const int64_t* chance = game_arg<int64_t>(rival_args, "chance");
    const std::string* beat = game_arg<std::string>(rival_args, "beat");
    if (chance == nullptr || *chance != 70 || beat == nullptr || *beat != RIVAL){
        return "Expected the rival's plan to carry beat and chance=70.";
    }
    rival_args["chance"] = int64_t(75);
    const std::optional<GrindPlan> rival_back = grind_plan_of(rival_args);
    if (!rival_back || rival_back->trainer != RIVAL || rival_back->level != 30 || rival_back->min_chance != 0.75){
        return "The rival's plan with chance=75 didn't read back.";
    }
    const std::vector<std::pair<std::string, Pokemon::StoryArgValue>> bad_args{
        {"level", int64_t(101)},
        {"slot", int64_t(7)},
        {"encounters", int64_t(0)},
        {"beat", std::string("")},
        {"chance", int64_t(0)},
        {"chance", int64_t(101)},
    };
    for (const auto& [key, value] : bad_args){
        Pokemon::StoryActionArgs bad = rival_args;
        bad[key] = value;
        if (grind_plan_of(bad)){
            return "Expected a bad " + key + " refused.";
        }
    }
    Pokemon::StoryActionArgs no_trainer = rival_args;
    no_trainer.erase("beat");
    if (grind_plan_of(no_trainer)){
        return "Expected a chance with no trainer refused.";
    }
    return true;
}

} // namespace


const AutoStoryGrindCase AUTO_STORY_GRIND_CASES[] = {
    {
        .input = {"grinding on a fake patch", test_grind},
        .expectation = {},
    },
    {
        .input = {"the step's args", test_grind_args},
        .expectation = {},
    },
    {
        .input = {"the level cap's message", test_grind_cap_message},
        .expectation = {},
    },
    {
        .input = {"the lead the sim plays is the one read", test_lead_as_read},
        .expectation = {},
    },
    {
        .input = {"console B's party stops at Lv25 against the Cerulean rival", test_console_party},
        .expectation = {},
    },
};

void add_tests_AutoStoryGrind(UnitTestDatabase& database){
    for (const auto& item : AUTO_STORY_GRIND_CASES){
        database.add<Test_AutoStoryGrind>(item);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
