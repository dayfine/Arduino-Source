/*  AutoStory League Rows Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The League's fight rows 20.01 to 20.05, with their verify blocks, run
 *  through the shared engine (run_segment()) against a fake game: each
 *  try's battle is scripted (won or lost, the items spent), the reload
 *  goes back to the bag of the last save, and a try passes by
 *  league_row_pass(): the fight's item caps, and the side it leaves able
 *  to start the next fight after its top-up. And the top-up before a
 *  fight (planned_between(), the console's league_top_up): one Super
 *  Potion, two, or a Full Restore, and the Elixir only before the
 *  Champion.
 *
 */

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <random>
#include <format>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Pokemon_StoryEvents.h"
#include "Pokemon/AutoStory/Pokemon_StorySegments.h"
#include "PokemonFRLG_AutoStoryLeagueFightData.h"
#include "PokemonFRLG_AutoStoryLeagueRows.h"
#include "PokemonFRLG_AutoStoryPlannedBetween.h"
#include "PokemonFRLG_AutoStoryRows20_Tests.h"
#include "PokemonFRLG_AutoStoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

constexpr std::string_view FIGHT_ROWS[] = {"20.01", "20.02", "20.03", "20.04", "20.05"};

const StoryCheckpoint& fight_row(std::string_view id){
    for (const StoryCheckpoint& row : story_checkpoints()){
        if (row.id == id){
            return row;
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("No checkpoint {}.", id));
}

constexpr int MAX_HP = 165;

//  One try's battle: won or lost, the items it spent, the HP and Surf PP
//  it left.
struct ScriptedTry{
    bool won = true;
    SlugCounts used = {};
    int hp_left = MAX_HP;
    int surf_spent = 0;
};

ScriptedTry win(SlugCounts used = {}, int hp_left = MAX_HP, int surf_spent = 0){
    return ScriptedTry{.won = true, .used = std::move(used), .hp_left = hp_left, .surf_spent = surf_spent};
}
ScriptedTry lose(){
    return ScriptedTry{.won = false};
}

//  The bag of "Enter Lorelei" (her entry contract), which 20.01a buys.
SlugCounts lorelei_bag(){
    return {
        {"full-restore", 8}, {"x-special", 15}, {"x-speed", 5}, {"x-accuracy", 3}, {"guard-spec", 2},
        {"super-potion", 6}, {"elixir", 1}, {"max-elixir", 1},
    };
}

//  Our side at full HP and PP with `bag`.
LeagueSide full_side(SlugCounts bag){
    const SlugCounts full_pp = {{"bite", 25}, {"strength", 15}, {"surf", 15}, {"blizzard", 5}};
    return LeagueSide{.hp = MAX_HP, .max_hp = MAX_HP, .pp = full_pp, .full_pp = full_pp, .bag = std::move(bag)};
}

//  The fake game: our side now and at the last save, and each row's tries
//  in order (the last repeats; a row with none wins spending nothing). A
//  try first tops up as the console's league_top_up does and checks the
//  fight's entry contract (failing "precondition_failed"), then plays its
//  battle. Each save's side is checked against the next fight's entry
//  contract after its top-up, which no save may break.
class FakeLeague{
public:
    using Script = std::function<ScriptedTry(const std::string& row)>;

    FakeLeague(SlugCounts bag, Script script)
        : m_side(full_side(std::move(bag)))
        , m_saved(m_side)
        , m_script(std::move(script))
    {}

    Pokemon::Segment segment(){
        Pokemon::Segment segment;
        segment.id = "20";
        for (std::string_view id : FIGHT_ROWS){
            const StoryCheckpoint& row = fight_row(id);
            Pokemon::CheckpointSpec spec;
            spec.id = row.id;
            spec.title = row.title;
            spec.verify = row.verify;
            spec.build = [this, id = row.id]{
                return checkpoint(id);
            };
            segment.checkpoints.push_back(std::move(spec));
        }
        return segment;
    }

    Pokemon::SegmentHooks hooks(){
        Pokemon::SegmentHooks hooks;
        hooks.reload = [this](Pokemon::RunState&){
            m_side = m_saved;
            return Pokemon::outcome_ok();
        };
        hooks.verify_pass = [this](const std::string& id, Pokemon::RunState&){
            return league_row_pass(fight_row(id), m_saved.bag, m_side);
        };
        hooks.log = [this](const std::string& line){
            log += std::format("\n    {}", line);
        };
        return hooks;
    }

    int full_restores_saved() const{
        return count_of(m_saved.bag, "full-restore");
    }

    int fights = 0;
    int saves = 0;
    //  Saves after which the next fight's entry contract failed.
    std::vector<std::string> broken_saves;
    std::string log;

private:
    Pokemon::Checkpoint checkpoint(const std::string& id){
        Pokemon::Checkpoint checkpoint;
        checkpoint.id = id;
        Pokemon::Event fight;
        fight.name = "fight";
        fight.act = [this, id](Pokemon::RunState&){
            const LeagueFightData& data = *league_row_fight(fight_row(id));
            const LeagueEntryCheck entry = top_up(data);
            if (!entry.short_of.empty()){
                return Pokemon::outcome_failed("precondition_failed", entry.short_of.front());
            }
            const ScriptedTry one = m_script(id);
            fights++;
            if (!one.won){
                return Pokemon::outcome_failed("blackout");
            }
            for (const auto& [item, count] : one.used){
                m_side.bag[item] -= count;
            }
            m_side.hp = std::clamp(one.hp_left, 1, MAX_HP);
            m_side.pp["surf"] = std::max(0, m_side.pp["surf"] - one.surf_spent);
            return Pokemon::outcome_ok();
        };
        checkpoint.events.push_back(std::move(fight));
        checkpoint.save = [this, id](Pokemon::RunState&){
            saves++;
            m_saved = m_side;
            if (const LeagueFightData* next = next_league_fight(*league_row_fight(fight_row(id)))){
                const LeagueEntryCheck check = league_entry_check(*next, m_saved);
                if (!check.short_of.empty()){
                    broken_saves.push_back(std::format("{}: {}", id, check.short_of.front()));
                }
            }
            return Pokemon::outcome_ok();
        };
        return checkpoint;
    }

    //  The heal rule's top-up before `fight`, on our side.
    LeagueEntryCheck top_up(const LeagueFightData& fight){
        PlannedCarry carry{.hp = m_side.hp, .max_hp = MAX_HP, .pp = m_side.pp, .full_pp = m_side.full_pp, .bag = m_side.bag};
        planned_between(carry, fight);
        m_side.hp = carry.hp;
        m_side.pp = carry.pp;
        m_side.bag = carry.bag;
        return league_entry_check(fight, m_side);
    }

    LeagueSide m_side;
    LeagueSide m_saved;
    Script m_script;
};

//  Each row's tries in order, the last repeating; a row with none wins.
FakeLeague::Script scripted(std::map<std::string, std::vector<ScriptedTry>, std::less<>> tries){
    auto played = std::make_shared<std::map<std::string, size_t>>();
    return [tries = std::move(tries), played](const std::string& row){
        const auto found = tries.find(row);
        if (found == tries.end() || found->second.empty()){
            return win();
        }
        size_t& count = (*played)[row];
        const ScriptedTry& one = found->second[std::min(count, found->second.size() - 1)];
        count++;
        return one;
    };
}

std::vector<ScriptedTry> repeated(size_t count, const ScriptedTry& one){
    return std::vector<ScriptedTry>(count, one);
}
std::vector<ScriptedTry> joined(std::vector<ScriptedTry> first, const std::vector<ScriptedTry>& then){
    first.insert(first.end(), then.begin(), then.end());
    return first;
}


struct LeagueRunCase{
    struct Input{
        //  The row the run starts at, and the bag of its start save.
        std::string first = "20.01";
        SlugCounts bag = lorelei_bag();
        //  Each row's tries, by row id.
        std::map<std::string, std::vector<ScriptedTry>, std::less<>> tries = {};
    };
    struct Expectation{
        bool completed = false;
        std::string failure = {};
        std::string failed_row = {};
        int fights = 0;
        int saves = 0;
        //  The Full Restores the last save holds.
        int full_restores_saved = 0;
    };

    std::string name = {};
    Input input = {};
    Expectation expectation = {};
};

class Test_LeagueRows : public UnitTest{
public:
    explicit Test_LeagueRows(LeagueRunCase test_case)
        : UnitTest(std::format("PokemonFRLG::AutoStoryLeagueRows - {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        FakeLeague game(m_case.input.bag, scripted(m_case.input.tries));
        Pokemon::Segment segment = game.segment();
        Pokemon::SegmentHooks hooks = game.hooks();
        Pokemon::RunState state;
        const Pokemon::SegmentReport report = Pokemon::run_segment(
            Pokemon::SegmentRange{m_case.input.first, "20.05"}, Pokemon::SegmentPolicy{}, segment, hooks, state
        );
        const LeagueRunCase::Expectation& want = m_case.expectation;
        const std::string failure = report.completed ? "" : report.failure.failure;
        if (report.completed != want.completed || failure != want.failure || report.failed_checkpoint != want.failed_row
            || game.fights != want.fights || game.saves != want.saves || game.full_restores_saved() != want.full_restores_saved
            || !game.broken_saves.empty()
        ){
            return std::format(
                "{} \"{}\" at \"{}\", fights {}, saves {} ({} broken), Full Restores saved {}; expected {} \"{}\" at \"{}\", {}, {}, {}:{}",
                report.completed ? "completed" : "failed", failure, report.failed_checkpoint,
                game.fights, game.saves, game.broken_saves.size(), game.full_restores_saved(),
                want.completed ? "completed" : "failed", want.failure, want.failed_row,
                want.fights, want.saves, want.full_restores_saved, game.log
            );
        }
        return true;
    }

private:
    LeagueRunCase m_case;
};

//  Each fight row plays its 10 phase 1 tries and one phase 2 try when
//  every try passes.
constexpr int TRIES_ALL_PASS = 11;

const LeagueRunCase LEAGUE_RUN_CASES[] = {
    {
        .name = "every try a win within the caps: 20.01 to 20.05 each saved once",
        .input = {},
        .expectation = {.completed = true, .fights = 5 * TRIES_ALL_PASS, .saves = 5, .full_restores_saved = 8},
    },
    {
        .name = "Bruno's first phase 2 win spends 3 Full Restores, over his cap of 2: not saved; the next win is",
        .input = {.tries = {
            {"20.02", joined(joined(repeated(10, win()), {win({{"full-restore", 3}})}), {win({{"full-restore", 1}})})},
        }},
        .expectation = {.completed = true, .fights = 5 * TRIES_ALL_PASS + 1, .saves = 5, .full_restores_saved = 7},
    },
    {
        .name = "Lorelei passes 4 of phase 1's 10, under min_pass 5: the run stops before Bruno, nothing saved",
        .input = {.tries = {{"20.01", joined(repeated(4, win()), {lose()})}}},
        .expectation = {.failure = "verify_below_min_pass", .failed_row = "20.01", .fights = 10, .saves = 0, .full_restores_saved = 8},
    },
    {
        .name = "Lorelei wins with 2 Full Restores, over her cap of 1, every time: phase 1 passes none, the run stops",
        .input = {.tries = {{"20.01", {win({{"full-restore", 2}})}}}},
        .expectation = {.failure = "verify_below_min_pass", .failed_row = "20.01", .fights = 10, .saves = 0, .full_restores_saved = 8},
    },
    {
        .name = "the bag at the Indigo Plateau Center short of a Full Restore: 20.01 stops before the League's door",
        .input = {.bag = {
            {"full-restore", 7}, {"x-special", 15}, {"x-speed", 5}, {"x-accuracy", 3}, {"guard-spec", 2},
            {"super-potion", 6}, {"elixir", 1}, {"max-elixir", 1},
        }},
        .expectation = {.failure = "precondition_failed", .failed_row = "20.01", .fights = 0, .saves = 0, .full_restores_saved = 7},
    },
    {
        .name = "from Lance's entry bag, a win within his cap of 2 Full Restores at 45 HP: the Champion's heal takes a third, leaving 1 of his 2: never saved",
        .input = {
            .first = "20.04",
            .bag = {
                {"full-restore", 4}, {"x-special", 4}, {"x-speed", 1}, {"x-accuracy", 3}, {"guard-spec", 2},
                {"super-potion", 2}, {"elixir", 1}, {"max-elixir", 1},
            },
            .tries = {{"20.04", {win({{"full-restore", 2}}, /*hp_left=*/45)}}},
        },
        .expectation = {.failure = "verify_below_min_pass", .failed_row = "20.04", .fights = 10, .saves = 0, .full_restores_saved = 4},
    },
    {
        .name = "Agatha wins leaving Surf at 2, under Lance's 3: never saved",
        .input = {.tries = {{"20.03", {win({}, MAX_HP, /*surf_spent=*/13)}}}},
        .expectation = {.failure = "verify_below_min_pass", .failed_row = "20.03", .fights = 2 * TRIES_ALL_PASS + 10, .saves = 2, .full_restores_saved = 8},
    },
};


//  Random fights within the caps: whatever they leave, no save breaks the
//  next fight's entry contract.
class Test_LeagueRowsRandom : public UnitTest{
public:
    Test_LeagueRowsRandom()
        : UnitTest("PokemonFRLG::AutoStoryLeagueRows - random wins within the caps: every save meets the next fight's entry contract")
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        constexpr uint32_t RUNS = 200;
        int saves = 0;
        for (uint32_t seed = 1; seed <= RUNS; seed++){
            auto random = std::make_shared<std::mt19937>(seed);
            FakeLeague game(lorelei_bag(), [random](const std::string& row){
                std::mt19937& next = *random;
                const LeagueFightData& fight = *league_row_fight(fight_row(row));
                if (next() % 10 < 3){
                    return lose();
                }
                SlugCounts used;
                for (const auto& [item, cap] : fight.caps){
                    if (cap > 0){
                        used[item] = static_cast<int>(next() % (cap + 1));
                    }
                }
                return win(std::move(used), /*hp_left=*/1 + static_cast<int>(next() % MAX_HP), /*surf_spent=*/static_cast<int>(next() % 16));
            });
            Pokemon::Segment segment = game.segment();
            Pokemon::SegmentHooks hooks = game.hooks();
            Pokemon::RunState state;
            Pokemon::run_segment({}, Pokemon::SegmentPolicy{}, segment, hooks, state);
            if (!game.broken_saves.empty()){
                return std::format("Seed {}: saved {}.", seed, game.broken_saves.front());
            }
            saves += game.saves;
        }
        if (saves == 0){
            return "No run saved anything: the check proves nothing.";
        }
        return true;
    }
};


//  The top-up before a fight (planned_between()) from our side at the
//  fight's start: Blastoise at HP 165, Surf and Blizzard as given.
struct TopUpCase{
    struct Input{
        std::string_view fight = "bruno";
        int missing_hp = 0;
        int super_potions = 7;
        int full_restores = 7;
        int surf_pp = 15;
    };
    struct Expectation{
        std::vector<std::string> items = {};
    };

    std::string name = {};
    Input input = {};
    Expectation expectation = {};
};

class Test_LeagueTopUp : public UnitTest{
public:
    explicit Test_LeagueTopUp(TopUpCase test_case)
        : UnitTest(std::format("PokemonFRLG::AutoStoryLeagueRows - top-up: {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const TopUpCase::Input& input = m_case.input;
        const LeagueFightData* fight = league_fight_data(league_plan_data(), input.fight);
        if (fight == nullptr){
            return std::format("No League fight {}.", input.fight);
        }
        PlannedCarry carry{
            .hp = MAX_HP - input.missing_hp, .max_hp = MAX_HP,
            .pp = {{"surf", input.surf_pp}, {"blizzard", 5}},
            .full_pp = {{"surf", 15}, {"blizzard", 5}},
            .bag = {{"super-potion", input.super_potions}, {"full-restore", input.full_restores}, {"elixir", 1}},
        };
        const std::vector<std::string> items = planned_between(carry, *fight);
        if (items != m_case.expectation.items){
            std::string text;
            for (const std::string& item : items){
                text += std::format(" {}", item);
            }
            return std::format("Used [{} ].", text);
        }
        return true;
    }

private:
    TopUpCase m_case;
};

//  Bruno's reserve is 5 Super Potions and 5 Full Restores: 7 Super
//  Potions leave two spare, 6 one.
const TopUpCase TOP_UP_CASES[] = {
    {
        .name = "40 HP missing: one Super Potion",
        .input = {.missing_hp = 40},
        .expectation = {.items = {"super-potion"}},
    },
    {
        .name = "90 HP missing, two Super Potions spare: two",
        .input = {.missing_hp = 90},
        .expectation = {.items = {"super-potion", "super-potion"}},
    },
    {
        .name = "90 HP missing, one Super Potion spare: a Full Restore",
        .input = {.missing_hp = 90, .super_potions = 6},
        .expectation = {.items = {"full-restore"}},
    },
    {
        .name = "120 HP missing: a Full Restore",
        .input = {.missing_hp = 120},
        .expectation = {.items = {"full-restore"}},
    },
    {
        .name = "full HP: nothing",
        .input = {},
        .expectation = {.items = {}},
    },
    {
        .name = "Surf at 3 before Lance: no Elixir",
        .input = {.fight = "lance", .surf_pp = 3},
        .expectation = {.items = {}},
    },
    {
        .name = "Surf at 3 before the Champion, under his floor of 8: the Elixir",
        .input = {.fight = "champion", .surf_pp = 3},
        .expectation = {.items = {"elixir"}},
    },
};

} // namespace


void add_tests_AutoStoryLeagueRows(UnitTestDatabase& database){
    for (const LeagueRunCase& test_case : LEAGUE_RUN_CASES){
        database.add<Test_LeagueRows>(test_case);
    }
    database.add<Test_LeagueRowsRandom>();
    for (const TopUpCase& test_case : TOP_UP_CASES){
        database.add<Test_LeagueTopUp>(test_case);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
