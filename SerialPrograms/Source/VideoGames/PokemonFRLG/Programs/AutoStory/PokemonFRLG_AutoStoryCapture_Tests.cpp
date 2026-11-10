/*  AutoStory Capture Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The capture step's decisions against a fake game: which encounters are
 *  run from, when balls or encounters run out, where a catch goes, what a
 *  shiny stops; its args; and a capture and a PC step run by the
 *  shared segment runner, as the program runs them, to show which failures
 *  reload the save and which stop the program.
 *
 */

#include <algorithm>
#include <format>
#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/Exceptions/ProgramFinishedException.h"
#include "Pokemon/AutoStory/Pokemon_StorySegments.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepExecutor.h"
#include "PokemonFRLG_AutoStoryCapture.h"
#include "PokemonFRLG_AutoStoryStepTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

using Pokemon::Direction;
using Pokemon::StoryStep;

//  Keeps the lines logged, for the messages a test checks.
class LineLogger : public Logger{
public:
    using Logger::log;
    virtual void log(const std::string& msg, Color) override{
        lines.push_back(msg);
    }
    std::vector<std::string> lines;
};

std::string joined(const std::vector<std::string>& items){
    std::string text;
    for (const std::string& item : items){
        text += (text.empty() ? "" : ", ") + item;
    }
    return text;
}


//  A wild Pokemon the fake game sends, in order.
struct FakeEncounter{
    std::string species = {};
    bool shiny = false;
    //  Throws it breaks free from before one catches it.
    size_t breaks = 0;
    //  Its HP bar after each attack, the last repeated; GREEN before any.
    std::vector<FoeHp> hp_after = {};
    bool faints_on_attack = false;
    bool flee_fails = false;
};

//  The game a capture plays, as a trace of what it did.
class FakeCaptureGame : public CaptureActions{
public:
    size_t party = 2;
    size_t bag_balls = 10;
    std::vector<FakeEncounter> encounters;
    std::vector<std::string> trace;
    //  Set by a step taken where the game wouldn't take it.
    std::string error;

    virtual bool find_encounter(Direction pace) override{
        if (pace != Direction::EAST && pace != Direction::NORTH){
            error = "paced " + std::string(direction_name(pace));
        }
        if (m_next >= encounters.size()){
            trace.push_back("none");
            return false;
        }
        m_current = encounters[m_next++];
        m_hp = FoeHp::GREEN;
        m_attacks = 0;
        m_in_battle = true;
        trace.push_back((m_current.shiny ? "shiny " : "") + (m_current.species.empty() ? "?" : m_current.species));
        return true;
    }
    //  A shiny's species isn't read: the console hears it before the menu.
    virtual EncounterSeen read_encounter() override{
        return {m_current.shiny ? "" : m_current.species, m_current.shiny};
    }
    virtual bool flee() override{
        check_in_battle("run");
        trace.push_back("run");
        if (m_current.flee_fails){
            return false;
        }
        m_in_battle = false;
        return true;
    }
    virtual FoeHp read_foe_hp() override{
        return m_hp;
    }
    virtual TurnEnd attack(size_t move) override{
        check_in_battle("attack");
        if (m_hp != FoeHp::GREEN){
            error = "attacked at a HP bar that wasn't green";
        }
        trace.push_back(std::format("move {}", move + 1));
        if (m_current.faints_on_attack){
            m_in_battle = false;
            return TurnEnd::FOE_FAINTED;
        }
        if (!m_current.hp_after.empty()){
            m_hp = m_current.hp_after[std::min(m_attacks, m_current.hp_after.size() - 1)];
        }
        m_attacks++;
        return TurnEnd::MENU;
    }
    virtual ThrowEnd throw_ball() override{
        check_in_battle("throw");
        if (bag_balls == 0){
            trace.push_back("no ball");
            return ThrowEnd::NO_BALLS;
        }
        bag_balls--;
        trace.push_back("ball");
        if (m_current.breaks > 0){
            m_current.breaks--;
            return ThrowEnd::BROKE_FREE;
        }
        m_caught = true;
        return ThrowEnd::CAUGHT;
    }
    virtual bool finish_catch(CatchDestination destination) override{
        if (!m_caught){
            error = "finished a catch that wasn't made";
        }
        const CatchDestination where = party < 6 ? CatchDestination::PARTY : CatchDestination::PC;
        if (destination != where){
            error = std::string("expected the catch to go to the ") + std::string(destination_name(where));
        }
        if (where == CatchDestination::PARTY){
            party++;
        }
        trace.push_back(std::string("to ") + std::string(destination_name(where)));
        m_caught = false;
        m_in_battle = false;
        return true;
    }

private:
    void check_in_battle(std::string_view step){
        if (!m_in_battle){
            error = std::string(step) + " outside a battle";
        }
    }

    size_t m_next = 0;
    FakeEncounter m_current;
    FoeHp m_hp = FoeHp::GREEN;
    size_t m_attacks = 0;
    bool m_in_battle = false;
    bool m_caught = false;
};


struct AutoStoryCaptureInput{
    std::string name;
    std::function<UnitTestResult()> body;
};

struct AutoStoryCaptureExpectation{
};

struct AutoStoryCaptureCase{
    AutoStoryCaptureInput input;
    AutoStoryCaptureExpectation expectation;
};

class Test_AutoStoryCapture : public UnitTest{
public:
    explicit Test_AutoStoryCapture(AutoStoryCaptureCase test_case)
        : UnitTest("PokemonFRLG::AutoStoryCapture - " + test_case.input.name)
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
    AutoStoryCaptureCase m_case;
};


const CapturePlan RATTATA{.species = "rattata", .max_encounters = 6, .max_balls = 4};


//  The capture loop's decisions, by what the game sends.
struct CaptureCase{
    std::string name = {};
    CapturePlan plan = RATTATA;
    size_t party = 2;
    size_t bag_balls = 10;
    std::vector<FakeEncounter> encounters = {};
    CaptureEnd end = CaptureEnd::FAILED;
    std::string trace = {};
    size_t caught = 0;
};
const std::vector<CaptureCase> CAPTURE_CASES = {
    {
        .name = "the target on the first encounter, caught to the party",
        .encounters = {{.species = "rattata"}},
        .end = CaptureEnd::CAUGHT,
        .trace = "rattata, ball, to party",
        .caught = 1,
    },
    {
        .name = "other species and an unread one run from",
        .encounters = {{.species = "spearow"}, {.species = ""}, {.species = "rattata", .breaks = 1}},
        .end = CaptureEnd::CAUGHT,
        .trace = "spearow, run, ?, run, rattata, ball, ball, to party",
        .caught = 1,
    },
    {
        .name = "a full party sends the catch to the PC",
        .party = 6,
        .encounters = {{.species = "rattata"}},
        .end = CaptureEnd::CAUGHT,
        .trace = "rattata, ball, to PC",
        .caught = 1,
    },
    {
        .name = "two catches with a party of 5: the first to the party, the second to the PC",
        .plan = {.species = "rattata", .catches = 2, .max_encounters = 6, .max_balls = 4},
        .party = 5,
        .encounters = {{.species = "rattata"}, {.species = "rattata"}},
        .end = CaptureEnd::CAUGHT,
        .trace = "rattata, ball, to party, rattata, ball, to PC",
        .caught = 2,
    },
    {
        .name = "the step's ball cap: run once it is reached",
        .encounters = {{.species = "rattata", .breaks = 10}},
        .end = CaptureEnd::OUT_OF_BALLS,
        .trace = "rattata, ball, ball, ball, ball, run",
    },
    {
        .name = "the bag runs out first",
        .bag_balls = 2,
        .encounters = {{.species = "rattata", .breaks = 10}},
        .end = CaptureEnd::OUT_OF_BALLS,
        .trace = "rattata, ball, ball, no ball, run",
    },
    {
        .name = "the encounter cap",
        .plan = {.species = "rattata", .max_encounters = 3},
        .encounters = {{.species = "spearow"}, {.species = "ekans"}, {.species = "spearow"}, {.species = "rattata"}},
        .end = CaptureEnd::OUT_OF_ENCOUNTERS,
        .trace = "spearow, run, ekans, run, spearow, run",
    },
    {
        .name = "no wild battle starts",
        .encounters = {{.species = "spearow"}},
        .end = CaptureEnd::NO_ENCOUNTER,
        .trace = "spearow, run, none",
    },
    {
        .name = "a shiny of another species: nothing pressed after it",
        .encounters = {{.species = "spearow", .shiny = true}, {.species = "rattata"}},
        .end = CaptureEnd::SHINY,
        .trace = "shiny spearow",
    },
    {
        .name = "a shiny target: no ball thrown either",
        .encounters = {{.species = "rattata", .shiny = true}},
        .end = CaptureEnd::SHINY,
        .trace = "shiny rattata",
    },
    {
        .name = "a run that fails",
        .encounters = {{.species = "spearow", .flee_fails = true}},
        .end = CaptureEnd::FAILED,
        .trace = "spearow, run",
    },
    {
        .name = "weakened from green only, then balls",
        .plan = {.species = "rattata", .max_balls = 4, .weaken_move = 0, .max_weaken_turns = 3},
        .encounters = {{.species = "rattata", .breaks = 1, .hp_after = {FoeHp::GREEN, FoeHp::YELLOW}}},
        .end = CaptureEnd::CAUGHT,
        .trace = "rattata, move 1, move 1, ball, ball, to party",
        .caught = 1,
    },
    {
        .name = "weakening stops at its turn cap",
        .plan = {.species = "rattata", .max_balls = 4, .weaken_move = 2, .max_weaken_turns = 1},
        .encounters = {{.species = "rattata", .hp_after = {FoeHp::GREEN}}},
        .end = CaptureEnd::CAUGHT,
        .trace = "rattata, move 3, ball, to party",
        .caught = 1,
    },
    {
        .name = "a target knocked out: on to the next encounter",
        .plan = {.species = "rattata", .max_balls = 4, .weaken_move = 0},
        .encounters = {{.species = "rattata", .faints_on_attack = true}, {.species = "rattata", .hp_after = {FoeHp::RED}}},
        .end = CaptureEnd::CAUGHT,
        .trace = "rattata, move 1, rattata, move 1, ball, to party",
        .caught = 1,
    },
};

//  Each decision of the capture loop, by what the game sends.
UnitTestResult test_capture_decisions(){
    for (const CaptureCase& item : CAPTURE_CASES){
        LineLogger logger;
        FakeCaptureGame game;
        game.party = item.party;
        game.bag_balls = item.bag_balls;
        game.encounters = item.encounters;
        const CaptureResult result = run_capture(logger, game, item.plan, item.party);
        if (!game.error.empty()){
            return item.name + ": " + game.error + ". Trace: " + joined(game.trace);
        }
        if (result.end != item.end || joined(game.trace) != item.trace || result.caught.size() != item.caught){
            return std::format(
                "{}: expected {} with {} caught after \"{}\"; got {} with {} caught after \"{}\".",
                item.name, capture_end_name(item.end), item.caught, item.trace,
                capture_end_name(result.end), result.caught.size(), joined(game.trace)
            );
        }
    }
    return true;
}


//  The step's args, both ways, and the args it refuses.
UnitTestResult test_capture_args(){
    const std::vector<CapturePlan> plans = {
        RATTATA,
        {.species = "pidgey", .catches = 2, .max_encounters = 30, .max_balls = 8, .pace = Direction::NORTH,
            .weaken_move = 3, .max_weaken_turns = 1},
    };
    for (const CapturePlan& plan : plans){
        const std::optional<CapturePlan> back = capture_plan_of(capture_args(plan));
        if (!back || back->species != plan.species || back->catches != plan.catches
            || back->max_encounters != plan.max_encounters || back->max_balls != plan.max_balls
            || back->pace != plan.pace || back->weaken_move != plan.weaken_move
            || back->max_weaken_turns != plan.max_weaken_turns
        ){
            return "The " + plan.species + " plan didn't read back from its args.";
        }
    }
    const Pokemon::StoryActionSpec* spec = Pokemon::find_story_action(story_action_specs(), Pokemon::StoryStepKind::GAME, CAPTURE_ACTION);
    if (spec == nullptr){
        return "No capture action among FRLG's actions.";
    }
    const Pokemon::StoryActionArgs short_form = Pokemon::with_fallbacks(*spec, {{"species", std::string("rattata")}});
    const std::optional<CapturePlan> defaults = capture_plan_of(short_form);
    if (!defaults || defaults->species != "rattata" || defaults->max_balls != 5 || defaults->weaken_move){
        return "Expected a capture of only a species to read with the defaults.";
    }
    const std::vector<std::pair<std::string, Pokemon::StoryArgValue>> bad_args{
        {"species", std::string("")},
        {"balls", int64_t(0)},
        {"move", int64_t(5)},
        {"pace", std::string("up")},
    };
    for (const auto& [key, value] : bad_args){
        Pokemon::StoryActionArgs args = short_form;
        args[key] = value;
        if (capture_plan_of(args)){
            return "Expected a bad " + key + " refused.";
        }
    }
    const Pokemon::StoryStep step = capture_step("catch_rattata", "Cut user", RATTATA);
    if (step.kind != Pokemon::StoryStepKind::GAME || step.action != CAPTURE_ACTION || !capture_plan_of(step.args)){
        return "Expected capture_step() to be a GAME capture step with its plan's args.";
    }
    return true;
}


//  What the run says at each end.
UnitTestResult test_capture_messages(){
    struct Case{
        CaptureResult result;
        std::string expected;
    };
    const std::vector<Case> cases = {
        {
            .result = {.end = CaptureEnd::CAUGHT, .encounters = 3, .balls_thrown = 2, .caught = {CatchDestination::PC}},
            .expected = "Capture: caught 1 rattata (PC): 3 encounters, 2 balls thrown, 1 of 1 caught.",
        },
        {
            .result = {.end = CaptureEnd::SHINY, .encounters = 4, .detail = ""},
            .expected = "Capture: a shiny Pokemon in encounter 4. Stopped in its battle without running, for you to "
                "decide: catch it or run by hand, then save.",
        },
        {
            .result = {.end = CaptureEnd::OUT_OF_BALLS, .encounters = 1, .balls_thrown = 2, .detail = "the bag has no Poke Ball"},
            .expected = "Capture of rattata: out of balls (the bag has no Poke Ball): 1 encounters, 2 balls thrown, 0 of 1 caught.",
        },
        {
            .result = {.end = CaptureEnd::OUT_OF_ENCOUNTERS, .encounters = 6},
            .expected = "Capture of rattata: out of encounters: 6 encounters, 0 balls thrown, 0 of 1 caught.",
        },
    };
    for (const Case& item : cases){
        const std::string got = capture_message(RATTATA, item.result);
        if (got != item.expected){
            return "Expected \"" + item.expected + "\", got \"" + got + "\".";
        }
    }
    return true;
}


//  A PC that does nothing: the segment tests below only use its party.
class IdlePc : public PcStorageActions{
public:
    std::vector<PcPartyMember> party = {{.species = "squirtle"}, {.species = "magikarp"}};
    size_t views = 0;
    virtual std::vector<PcPartyMember> read_party() override{
        return party;
    }
    virtual PcView read_view() override{
        views++;
        return {};
    }
    virtual void press(PcPress) override{}
};

//  Runs capture and PC GAME steps as the program's executor does.
class StorageExecutor : public Pokemon::StoryStepExecutor{
public:
    LineLogger logger;
    FakeCaptureGame capture;
    IdlePc pc;

    virtual Pokemon::Outcome run(Pokemon::RunState&, const StoryStep& step) override{
        if (step.kind != Pokemon::StoryStepKind::GAME){
            return Pokemon::unsupported_step(step);
        }
        const std::optional<Pokemon::Outcome> outcome = run_storage_game_step(logger, nullptr, step, capture, pc);
        return outcome ? *outcome : Pokemon::unsupported_step(step);
    }
};

struct SegmentRun{
    Pokemon::SegmentReport report;
    size_t reloads = 0;
    //  The program stopped: ProgramFinishedException.
    bool finished = false;
};

//  One checkpoint of `steps`, run by the shared runner with the program's
//  retry policy.
SegmentRun run_steps(StorageExecutor& executor, const std::vector<StoryStep>& steps){
    const Pokemon::StoryCheckpoint data{.id = "test.01", .title = "capture", .start = std::nullopt, .steps = steps};
    Pokemon::Segment segment;
    segment.id = "test";
    segment.checkpoints.push_back(Pokemon::CheckpointSpec{
        .id = data.id, .title = data.title, .saves = false,
        .build = [&]{ return Pokemon::build_story_checkpoint(data, executor); },
    });
    SegmentRun run;
    Pokemon::SegmentHooks hooks;
    hooks.save = [](Pokemon::RunState&){ return Pokemon::outcome_ok(); };
    hooks.reload = [&](Pokemon::RunState&){
        run.reloads++;
        return Pokemon::outcome_ok();
    };
    hooks.log = [](const std::string&){};
    Pokemon::RunState state;
    try{
        run.report = Pokemon::run_segment(state, segment, {}, Pokemon::SegmentPolicy{}, hooks);
    }catch (ProgramFinishedException&){
        run.finished = true;
    }
    return run;
}

//  A capture that runs out reloads the save and tries again; a shiny stops
//  the program where it is, without a reload; a refused deposit fails
//  the checkpoint before the PC is touched.
UnitTestResult test_capture_in_segment(){
    {
        StorageExecutor executor;
        executor.capture.encounters = {{.species = "spearow"}, {.species = "rattata"}};
        const SegmentRun run = run_steps(executor, {capture_step("catch_rattata", "", RATTATA)});
        if (!run.report.completed || run.reloads != 0 || joined(executor.capture.trace) != "spearow, run, rattata, ball, to party"){
            return "Expected the catch in one attempt; got: " + joined(executor.capture.trace);
        }
    }
    {
        StorageExecutor executor;
        executor.capture.bag_balls = 0;
        executor.capture.encounters = {{.species = "rattata"}, {.species = "rattata"}, {.species = "rattata"}};
        const SegmentRun run = run_steps(executor, {capture_step("catch_rattata", "", RATTATA)});
        if (run.report.completed || run.reloads != 2 || run.report.failure.failure != "capture_out_of_balls"){
            return std::format(
                "Expected 3 attempts out of balls, reloading between; got {} reloads, failure {}.",
                run.reloads, run.report.failure.failure
            );
        }
    }
    {
        StorageExecutor executor;
        executor.capture.encounters = {{.species = "pidgey", .shiny = true}, {.species = "rattata"}};
        const SegmentRun run = run_steps(executor, {capture_step("catch_rattata", "", RATTATA)});
        const bool told = std::find_if(executor.logger.lines.begin(), executor.logger.lines.end(), [](const std::string& line){
            return line.starts_with("Capture: a shiny Pokemon in encounter 1.");
        }) != executor.logger.lines.end();
        if (!run.finished || !told || run.reloads != 0 || joined(executor.capture.trace) != "shiny pidgey"){
            return "Expected the program stopped at the shiny with no reload; got: " + joined(executor.capture.trace)
                + std::format(" ({} reloads)", run.reloads);
        }
    }
    {
        StorageExecutor executor;
        executor.pc.party = {{.species = "squirtle", .fainted = true}, {.species = "magikarp"}};
        const PcTransfer deposit{.action = PcAction::DEPOSIT, .party_slot = 1};
        const SegmentRun run = run_steps(executor, {pc_step("deposit_magikarp", "", deposit)});
        if (run.report.completed || run.report.failure.failure != "pc_refused" || executor.pc.views != 0){
            return "Expected the deposit of the last Pokemon that can fight refused before the PC was read.";
        }
    }
    return true;
}



//  A patch where the battle starts on step `battle_step` (from 1): the
//  flash shows unless missed; its screen shows from `screen_after` steps
//  later on (0: right after the step), or never.
struct FakePatch : public PatchSteps{
    size_t battle_step = 0;
    bool flash_missed = false;
    std::optional<size_t> screen_after = 0;
    //  Whether the overworld shows when no battle does.
    bool overworld = true;

    size_t steps = 0;
    size_t moved = 0;
    std::vector<std::string> dumps = {};

    bool battle_started() const{
        return battle_step != 0 && steps >= battle_step;
    }
    virtual bool step() override{
        if (battle_started()){
            //  The D-pad does nothing in a battle.
            steps++;
            return false;
        }
        steps++;
        moved++;
        return steps == battle_step && !flash_missed;
    }
    virtual bool battle_shows() override{
        return battle_started() && screen_after && steps >= battle_step + *screen_after;
    }
    virtual bool overworld_shows() override{
        return !battle_started() && overworld;
    }
    virtual void dump_screen(std::string_view label) override{
        dumps.emplace_back(label);
    }
};

struct PaceCase{
    std::string name;
    FakePatch patch;
    bool found;
    //  Steps the player took on the overworld: none in a battle.
    size_t moved;
    size_t dumps = 0;
};

UnitTestResult test_pace_until_battle(){
    const auto patch = [](size_t battle_step, bool flash_missed, std::optional<size_t> screen_after, bool overworld){
        FakePatch fake;
        fake.battle_step = battle_step;
        fake.flash_missed = flash_missed;
        fake.screen_after = screen_after;
        fake.overworld = overworld;
        return fake;
    };
    std::vector<PaceCase> cases{
        {"the flash on step 5", patch(5, false, 0, true), true, 5},
        //  Console B, 2026-11-03: "Wild RATTATA appeared!" waited for A
        //  while 193 more steps paced in vain.
        {"the flash missed, the text box right after", patch(5, true, 0, true), true, 5},
        {"the flash missed, the text box a step later", patch(5, true, 1, true), true, 5},
        {"the flash missed, the text box only after the last step", patch(PACE_MAX_STEPS, true, 0, true), true, PACE_MAX_STEPS},
        {"no battle: the overworld, no image", patch(0, false, 0, true), false, PACE_MAX_STEPS},
        {"no battle, not the overworld: an image", patch(0, false, 0, false), false, PACE_MAX_STEPS, 1},
    };
    for (PaceCase& item : cases){
        LineLogger logger;
        const bool found = pace_until_battle(logger, item.patch, PACE_MAX_STEPS);
        if (found != item.found || item.patch.moved != item.moved || item.patch.dumps.size() != item.dumps){
            return std::format(
                "{}: expected found {}, {} steps moved, {} images; got {}, {}, {} ({}).",
                item.name, item.found, item.moved, item.dumps,
                found, item.patch.moved, item.patch.dumps.size(), joined(logger.lines)
            );
        }
    }
    return true;
}

} // namespace


const AutoStoryCaptureCase AUTO_STORY_CAPTURE_CASES[] = {
    {
        .input = {"decisions by encounter", test_capture_decisions},
        .expectation = {},
    },
    {
        .input = {"the step's args", test_capture_args},
        .expectation = {},
    },
    {
        .input = {"messages", test_capture_messages},
        .expectation = {},
    },
    {
        .input = {"capture and PC steps in a segment: reloads and stops", test_capture_in_segment},
        .expectation = {},
    },
    {
        .input = {"pacing finds a battle whose flash was missed", test_pace_until_battle},
        .expectation = {},
    },
};

void add_tests_AutoStoryCapture(UnitTestDatabase& database){
    for (const auto& item : AUTO_STORY_CAPTURE_CASES){
        database.add<Test_AutoStoryCapture>(item);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
