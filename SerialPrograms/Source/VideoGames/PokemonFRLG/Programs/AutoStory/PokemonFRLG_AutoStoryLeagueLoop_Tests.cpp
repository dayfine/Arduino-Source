/*  AutoStory League Loop Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The League loop (PokemonFRLG_AutoStoryLeagueLoop.h) on a fake runner:
 *  each try's battle is scripted as how it ended and the items it spent,
 *  and the top-ups between fights are planned_between()'s. The cases check
 *  the phases, the saves, the entry contract and the top-ups.
 *
 */

#include <algorithm>
#include <format>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonFRLG_AutoStoryLeagueFightData.h"
#include "PokemonFRLG_AutoStoryLeagueLoop.h"
#include "PokemonFRLG_AutoStoryLeagueLoop_Tests.h"
#include "PokemonFRLG_AutoStoryPlannedBetween.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

constexpr int OUR_MAX_HP = 171;
const SlugCounts FULL_PP = {{"bite", 25}, {"surf", 15}, {"blizzard", 5}, {"strength", 15}};

//  The bag the League starts with (PokemonSimulation BAG_NOW).
SlugCounts league_bag(){
    return {
        {"full-restore", 8}, {"x-special", 15}, {"x-speed", 5}, {"x-accuracy", 3}, {"guard-spec", 2},
        {"max-elixir", 1}, {"elixir", 1}, {"super-potion", 6}, {"full-heal", 2}, {"parlyz-heal", 2}, {"antidote", 1},
    };
}

//  A try's battle: how it ended and the items it spent.
struct ScriptedBattle{
    LeagueBattleEnd end = LeagueBattleEnd::LOST;
    SlugCounts used = {};
};

//  A win on no items.
ScriptedBattle win(){
    return ScriptedBattle{.end = LeagueBattleEnd::WON};
}
//  A win on two Full Restores, over Lorelei's cap of one.
ScriptedBattle win_on_two_full_restores(){
    return ScriptedBattle{.end = LeagueBattleEnd::WON, .used = {{"full-restore", 2}}};
}
ScriptedBattle lost(){
    return ScriptedBattle{.end = LeagueBattleEnd::LOST};
}
ScriptedBattle roared(){
    return ScriptedBattle{.end = LeagueBattleEnd::ROARED, .used = {{"x-speed", 1}}};
}
ScriptedBattle early_reset(){
    return ScriptedBattle{.end = LeagueBattleEnd::EARLY_RESET, .used = {{"x-speed", 3}}};
}

std::vector<ScriptedBattle> repeated(size_t count, const ScriptedBattle& battle){
    return std::vector<ScriptedBattle>(count, battle);
}
std::vector<ScriptedBattle> joined(std::vector<ScriptedBattle> first, const std::vector<ScriptedBattle>& then){
    first.insert(first.end(), then.begin(), then.end());
    return first;
}

//  The fight's start save: full HP and PP, `bag`.
LeagueSide full_side(SlugCounts bag){
    return LeagueSide{.hp = OUR_MAX_HP, .max_hp = OUR_MAX_HP, .pp = FULL_PP, .bag = std::move(bag)};
}

SlugCounts changed(SlugCounts counts, const SlugCounts& set){
    for (const auto& [name, count] : set){
        counts[name] = count;
    }
    return counts;
}

std::string items_text(const std::vector<std::string>& items){
    std::string text;
    for (const std::string& item : items){
        text += text.empty() ? item : std::format(", {}", item);
    }
    return text;
}

//  The fight's start save, and each try's battle in order (the last one
//  repeats).
class FakeLeagueRunner{
public:
    FakeLeagueRunner(const LeagueFightData& fight, LeagueSide save, std::vector<ScriptedBattle> battles)
        : m_fight(fight)
        , m_save(std::move(save))
        , m_battles(std::move(battles))
    {}

    LeagueFightRunner runner(){
        return LeagueFightRunner{
            .load_start = [this]{ m_now = m_save; },
            .top_up = [this]{ return top_up(); },
            .read_side = [this]{ return m_now; },
            .fight = [this]{ return fight(); },
            .save = [this]{ m_saves++; },
            .log = [](std::string_view){},
        };
    }
    int saves() const{
        return m_saves;
    }
    int fights() const{
        return m_fights;
    }

private:
    std::vector<std::string> top_up(){
        PlannedCarry carry{.hp = m_now.hp, .max_hp = m_now.max_hp, .pp = m_now.pp, .full_pp = FULL_PP, .bag = m_now.bag};
        std::vector<std::string> used = planned_between(carry, m_fight);
        m_now.hp = carry.hp;
        m_now.pp = carry.pp;
        m_now.bag = carry.bag;
        return used;
    }
    LeagueBattleEnd fight(){
        const ScriptedBattle& battle = m_battles[std::min<size_t>(m_fights, m_battles.size() - 1)];
        m_fights++;
        for (const auto& [item, count] : battle.used){
            m_now.bag[item] -= count;
        }
        return battle.end;
    }

    const LeagueFightData& m_fight;
    LeagueSide m_save;
    LeagueSide m_now;
    std::vector<ScriptedBattle> m_battles;
    int m_saves = 0;
    int m_fights = 0;
};

std::string_view verdict_name(LeagueFightVerdict verdict){
    switch (verdict){
    case LeagueFightVerdict::SAVED:
        return "saved";
    case LeagueFightVerdict::BELOW_MIN_PASS:
        return "below min_pass";
    case LeagueFightVerdict::OUT_OF_BUDGET:
        return "out of budget";
    case LeagueFightVerdict::PRECONDITION_FAILED:
        return "precondition failed";
    case LeagueFightVerdict::NO_FIGURES:
        return "no figures";
    }
    return "";
}

struct LoopCase{
    struct Input{
        std::string fight = {};
        LeagueSide save = {};
        std::vector<ScriptedBattle> battles = {};
    };
    struct Expectation{
        LeagueFightVerdict verdict = LeagueFightVerdict::SAVED;
        int phase1_passes = 0;
        int fights = 0;
        int saves = 0;
        //  The first try's top-ups, when checked.
        std::optional<std::vector<std::string>> top_up = std::nullopt;
        //  The last try's end, when checked.
        std::optional<LeagueBattleEnd> last_end = std::nullopt;
    };

    std::string name = {};
    Input input = {};
    Expectation expectation = {};
};

class Test_LeagueLoop : public UnitTest{
public:
    explicit Test_LeagueLoop(LoopCase test_case)
        : UnitTest(std::format("PokemonFRLG::AutoStoryLeagueLoop - fake runner: {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const LeagueFightData* fight = league_fight_data(league_plan_data(), m_case.input.fight);
        if (fight == nullptr){
            return std::format("no fight {}.", m_case.input.fight);
        }
        FakeLeagueRunner fake(*fight, m_case.input.save, m_case.input.battles);
        const LeagueFightReport report = run_league_fight(*fight, fake.runner());
        const LoopCase::Expectation& want = m_case.expectation;
        if (report.verdict != want.verdict || report.phase1_passes != want.phase1_passes
            || fake.fights() != want.fights || fake.saves() != want.saves
        ){
            return std::format("{}, phase 1 passes {}, fights {}, saves {}; expected {}, {}, {}, {}.",
                verdict_name(report.verdict), report.phase1_passes, fake.fights(), fake.saves(),
                verdict_name(want.verdict), want.phase1_passes, want.fights, want.saves);
        }
        if (want.top_up && (report.tries.empty() || report.tries.front().top_up != *want.top_up)){
            return std::format("top-up [{}]; expected [{}].",
                report.tries.empty() ? std::string() : items_text(report.tries.front().top_up), items_text(*want.top_up));
        }
        if (want.last_end && (report.tries.empty() || report.tries.back().end != *want.last_end)){
            return "the last try ended otherwise.";
        }
        return true;
    }

private:
    LoopCase m_case;
};

//  Bruno's start save: Lorelei won on her caps (a Full Restore, six X Specials).
SlugCounts bruno_bag(){
    return changed(league_bag(), {{"full-restore", 7}, {"x-special", 9}});
}

const LoopCase LOOP_CASES[] = {
    {
        .name = "Lorelei: ten passes, saved on the first phase 2 try",
        .input = {.fight = "lorelei", .save = full_side(league_bag()), .battles = {win()}},
        .expectation = {.verdict = LeagueFightVerdict::SAVED, .phase1_passes = 10, .fights = 11, .saves = 1},
    },
    {
        .name = "Lorelei: four passes in phase 1, under min_pass 5: stop",
        .input = {.fight = "lorelei", .save = full_side(league_bag()), .battles = joined(repeated(4, win()), {lost()})},
        .expectation = {.verdict = LeagueFightVerdict::BELOW_MIN_PASS, .phase1_passes = 4, .fights = 10, .saves = 0},
    },
    {
        .name = "Lorelei: phase 2 passes on its third try: saved",
        .input = {
            .fight = "lorelei",
            .save = full_side(league_bag()),
            .battles = joined(joined(repeated(5, win()), repeated(7, lost())), {win()}),
        },
        .expectation = {.verdict = LeagueFightVerdict::SAVED, .phase1_passes = 5, .fights = 13, .saves = 1},
    },
    {
        .name = "Lorelei: no pass in the reset budget of 5: stop",
        .input = {.fight = "lorelei", .save = full_side(league_bag()), .battles = joined(repeated(5, win()), {lost()})},
        .expectation = {.verdict = LeagueFightVerdict::OUT_OF_BUDGET, .phase1_passes = 5, .fights = 15, .saves = 0},
    },
    {
        .name = "Lorelei: phase 2 wins over the Full Restore cap: never saved",
        .input = {
            .fight = "lorelei",
            .save = full_side(league_bag()),
            .battles = joined(repeated(5, win()), {win_on_two_full_restores()}),
        },
        .expectation = {
            .verdict = LeagueFightVerdict::OUT_OF_BUDGET, .phase1_passes = 5, .fights = 15, .saves = 0,
            .last_end = LeagueBattleEnd::WON,
        },
    },
    {
        .name = "Bruno: a Roar, then early resets after Onix: tries that don't pass",
        .input = {.fight = "bruno", .save = full_side(bruno_bag()), .battles = {roared(), early_reset()}},
        .expectation = {
            .verdict = LeagueFightVerdict::BELOW_MIN_PASS, .phase1_passes = 0, .fights = 10, .saves = 0,
            .last_end = LeagueBattleEnd::EARLY_RESET,
        },
    },
    {
        .name = "Lorelei's save is a Full Restore short of the entry contract: stop before fighting",
        .input = {.fight = "lorelei", .save = full_side(changed(league_bag(), {{"full-restore", 7}})), .battles = {win()}},
        .expectation = {.verdict = LeagueFightVerdict::PRECONDITION_FAILED, .phase1_passes = 0, .fights = 0, .saves = 0},
    },
    {
        .name = "before Bruno, 40 HP missing: one Super Potion",
        .input = {
            .fight = "bruno",
            .save = LeagueSide{.hp = OUR_MAX_HP - 40, .max_hp = OUR_MAX_HP, .pp = FULL_PP, .bag = bruno_bag()},
            .battles = {lost()},
        },
        .expectation = {
            .verdict = LeagueFightVerdict::BELOW_MIN_PASS, .phase1_passes = 0, .fights = 10, .saves = 0,
            .top_up = std::vector<std::string>{"super-potion"},
        },
    },
    {
        .name = "before Bruno, 90 HP missing and two Super Potions spare: two",
        .input = {
            .fight = "bruno",
            .save = LeagueSide{
                .hp = OUR_MAX_HP - 90, .max_hp = OUR_MAX_HP, .pp = FULL_PP, .bag = changed(bruno_bag(), {{"super-potion", 7}}),
            },
            .battles = {lost()},
        },
        .expectation = {
            .verdict = LeagueFightVerdict::BELOW_MIN_PASS, .phase1_passes = 0, .fights = 10, .saves = 0,
            .top_up = std::vector<std::string>{"super-potion", "super-potion"},
        },
    },
    {
        .name = "before Bruno, 120 HP missing: a Full Restore",
        .input = {
            .fight = "bruno",
            .save = LeagueSide{.hp = OUR_MAX_HP - 120, .max_hp = OUR_MAX_HP, .pp = FULL_PP, .bag = bruno_bag()},
            .battles = {lost()},
        },
        .expectation = {
            .verdict = LeagueFightVerdict::BELOW_MIN_PASS, .phase1_passes = 0, .fights = 10, .saves = 0,
            .top_up = std::vector<std::string>{"full-restore"},
        },
    },
    {
        .name = "before the Champion, Surf 5 and Blizzard 2: the Elixir",
        .input = {
            .fight = "champion",
            .save = LeagueSide{
                .hp = OUR_MAX_HP, .max_hp = OUR_MAX_HP,
                .pp = changed(FULL_PP, {{"surf", 5}, {"blizzard", 2}}),
                .bag = {
                    {"full-restore", 2}, {"x-special", 2}, {"x-speed", 1}, {"x-accuracy", 3}, {"guard-spec", 2},
                    {"elixir", 1}, {"max-elixir", 1},
                },
            },
            .battles = {lost()},
        },
        .expectation = {
            .verdict = LeagueFightVerdict::BELOW_MIN_PASS, .phase1_passes = 0, .fights = 10, .saves = 0,
            .top_up = std::vector<std::string>{"elixir"},
        },
    },
    {
        .name = "before Lance, Surf 3: no Elixir",
        .input = {
            .fight = "lance",
            .save = LeagueSide{
                .hp = OUR_MAX_HP, .max_hp = OUR_MAX_HP,
                .pp = changed(FULL_PP, {{"surf", 3}}),
                .bag = {
                    {"full-restore", 4}, {"x-special", 4}, {"x-speed", 1}, {"x-accuracy", 3}, {"guard-spec", 2},
                    {"super-potion", 2}, {"elixir", 1}, {"max-elixir", 1},
                },
            },
            .battles = {lost()},
        },
        .expectation = {
            .verdict = LeagueFightVerdict::BELOW_MIN_PASS, .phase1_passes = 0, .fights = 10, .saves = 0,
            .top_up = std::vector<std::string>{},
        },
    },
};

} // namespace


void add_tests_AutoStoryLeagueLoop(UnitTestDatabase& database){
    for (const LoopCase& test_case : LOOP_CASES){
        database.add<Test_LeagueLoop>(test_case);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
