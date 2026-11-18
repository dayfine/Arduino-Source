/*  AutoStory Grind Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The GRIND step against a fake patch: wild battles that each give EXP,
 *  a lead that gets hurt after a few, a nurse to walk to, and the bounds
 *  and stops (a shiny, a blackout, a level that can't be read).
 *
 */

#include <format>
#include <functional>
#include <string>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonFRLG_AutoStoryGameArgs.h"
#include "PokemonFRLG_AutoStoryGrind.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{


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

    size_t encounters = 0;
    size_t fights = 0;
    size_t heals = 0;
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

std::vector<GrindCase> grind_cases(){
    std::vector<GrindCase> cases;
    for (std::vector<GrindCase> part : {grind_cases_1(), grind_cases_2()}){
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
        || back->heal_every != 8 || back->member != 0
    ){
        return "The level-18 plan didn't read back from its args.";
    }
    const std::vector<std::pair<std::string, Pokemon::StoryArgValue>> bad_args{
        {"level", int64_t(101)},
        {"slot", int64_t(7)},
        {"encounters", int64_t(0)},
    };
    for (const auto& [key, value] : bad_args){
        Pokemon::StoryActionArgs bad = args;
        bad[key] = value;
        if (grind_plan_of(bad)){
            return "Expected a bad " + key + " refused.";
        }
    }
    return true;
}

//  The check in the Center before the walk out: a lead already at the
//  plan's level needs no trip (06.03 walked to Route 6's grass and back at
//  Lv29, console B, 2026-11-03); one short of it does. Nothing is fought
//  either way.
UnitTestResult test_grind_reached_already(){
    QuietLogger logger;
    for (const unsigned level : {16u, 19u}){
        FakePatch patch;
        patch.level = level;
        const bool reached = grind_reached_already(logger, patch, [&patch](size_t slot){
            return patch.read_member(slot);
        }, squirtle_to(19));
        if (reached != (level >= 19) || patch.encounters != 0 || patch.heals != 0 || !patch.error.empty()){
            return std::format("Lv{}: reached {}, {} encounters, {} heals {}.", level, reached, patch.encounters, patch.heals, patch.error);
        }
    }
    const Pokemon::StoryStep check = grind_check_step("lead_ready", "", squirtle_to(30));
    const std::optional<GrindPlan> back = grind_plan_of(check.args);
    if (check.action != GRIND_CHECK_ACTION || !back || back->level != 30 || check.args.contains("nurse")){
        return "The check's args didn't read back as its plan, or had a nurse.";
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
        .input = {"reached already: no walk out to the patch", test_grind_reached_already},
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
