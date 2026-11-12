/*  AutoStory Story Run Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <string>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "Pokemon_StoryRun.h"
#include "Pokemon_StoryRun_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{

namespace{

//  A game that records what the story run asked of it.
struct FakeGame{
    StoryPreflight preflight{.video_alive = true, .controller_ready = true, .game_running = true};
    std::string place_error;
    size_t loads = 0;
    size_t rereads = 0;
    size_t saves = 0;
    std::vector<std::string> lines;

    StoryGameHooks hooks(){
        StoryGameHooks game;
        game.preflight = [this]{
            return preflight;
        };
        game.load_save = [this](RunState&){
            loads++;
            return outcome_ok();
        };
        game.check_save_place = [this]{
            return place_error;
        };
        game.after_load = [this](RunState&){
            rereads++;
            return outcome_ok();
        };
        game.save = [this](RunState&){
            saves++;
            return outcome_ok();
        };
        game.log = [this](const std::string& line){
            lines.push_back(line);
        };
        return game;
    }
    bool logged(std::string_view line) const{
        for (const std::string& logged_line : lines){
            if (logged_line == line){
                return true;
            }
        }
        return false;
    }
};

//  A start as the program asks for it, and what the game must have done.
struct StartCase{
    struct Input{
        std::string name;
        StoryStart start;
        std::string place_error;
        StoryPreflight preflight;
    };
    struct Expectation{
        bool succeeds;
        size_t loads;
        size_t rereads;
        bool save_loaded;
    };
    Input input;
    Expectation expectation;
};

constexpr StoryPreflight READY{.video_alive = true, .controller_ready = true, .game_running = true};
constexpr StoryPreflight ON_HOME{.video_alive = true, .controller_ready = true, .game_running = false};
constexpr StoryPreflight NO_VIDEO{.video_alive = false, .controller_ready = true, .game_running = true};
constexpr StoryPreflight NO_CONTROLLER{.video_alive = true, .controller_ready = false, .game_running = true};

std::string_view start_name(StoryStart start){
    switch (start){
    case StoryStart::NEW_GAME:
        return "a new game";
    case StoryStart::LIVE:
        return "the game as it is";
    case StoryStart::FROM_SAVE:
        return "its save";
    }
    return "?";
}

class Test_StoryStart : public UnitTest{
public:
    Test_StoryStart(StartCase test_case)
        : UnitTest(std::format("Pokemon::StoryRun - start: {}", test_case.input.name))
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        FakeGame fake;
        fake.place_error = m_case.input.place_error;
        fake.preflight = m_case.input.preflight;
        StoryGameHooks game = fake.hooks();
        RunState state;
        const Outcome outcome = prepare_story_start("00.04b", m_case.input.start, game, state);
        const bool save_loaded = state.flags.contains("save_loaded");
        if (outcome.succeeded() != m_case.expectation.succeeds || fake.loads != m_case.expectation.loads
            || fake.rereads != m_case.expectation.rereads || save_loaded != m_case.expectation.save_loaded
        ){
            return std::format(
                "Expected ok={} loads={} rereads={} save_loaded={}, got {} {} {} {} ({})",
                m_case.expectation.succeeds, m_case.expectation.loads, m_case.expectation.rereads, m_case.expectation.save_loaded,
                outcome.succeeded(), fake.loads, fake.rereads, save_loaded, to_string(outcome)
            );
        }
        if (!fake.logged(std::format("[Story] start: checkpoint 00.04b, from {}", start_name(m_case.input.start)))){
            return "Expected the line naming the checkpoint that starts";
        }
        if (!m_case.input.preflight.game_running && outcome.failure != StoryPreflight::FAILURE){
            return std::format("Expected {}, got {}", StoryPreflight::FAILURE, to_string(outcome));
        }
        return true;
    }

private:
    const StartCase m_case;
};

const StartCase START_CASES[] = {
    //  Console B, 2026-11-11: a BDSP run started at 00.04b with the game
    //  left at 00.04d walked from 00.04d's position.
    {
        .input = {.name = "a checkpoint with a save loads it first", .start = StoryStart::FROM_SAVE, .place_error = "", .preflight = READY},
        .expectation = {.succeeds = true, .loads = 1, .rereads = 1, .save_loaded = true},
    },
    {
        .input = {.name = "a save in the wrong place fails before rereading it", .start = StoryStart::FROM_SAVE, .place_error = "The save is in Pewter City.", .preflight = READY},
        .expectation = {.succeeds = false, .loads = 1, .rereads = 0, .save_loaded = false},
    },
    {
        .input = {.name = "a new game loads nothing", .start = StoryStart::NEW_GAME, .place_error = "", .preflight = READY},
        .expectation = {.succeeds = true, .loads = 0, .rereads = 0, .save_loaded = false},
    },
    {
        .input = {.name = "a live start loads nothing", .start = StoryStart::LIVE, .place_error = "", .preflight = READY},
        .expectation = {.succeeds = true, .loads = 0, .rereads = 0, .save_loaded = false},
    },
    //  Console B, 2026-11-11 13:32: the game was on HOME; the run saved,
    //  pressed X/R/ZL on HOME, then "loaded" nothing. The preflight stops it
    //  before any press.
    {
        .input = {.name = "the HOME menu stops the run before the load", .start = StoryStart::FROM_SAVE, .place_error = "", .preflight = ON_HOME},
        .expectation = {.succeeds = false, .loads = 0, .rereads = 0, .save_loaded = false},
    },
    {
        .input = {.name = "the HOME menu stops a live start", .start = StoryStart::LIVE, .place_error = "", .preflight = ON_HOME},
        .expectation = {.succeeds = false, .loads = 0, .rereads = 0, .save_loaded = false},
    },
    {
        .input = {.name = "dead video stops the run before the load", .start = StoryStart::FROM_SAVE, .place_error = "", .preflight = NO_VIDEO},
        .expectation = {.succeeds = false, .loads = 0, .rereads = 0, .save_loaded = false},
    },
    {
        .input = {.name = "a controller not ready stops the run before the load", .start = StoryStart::FROM_SAVE, .place_error = "", .preflight = NO_CONTROLLER},
        .expectation = {.succeeds = false, .loads = 0, .rereads = 0, .save_loaded = false},
    },
};


//  A preflight's checks, and the failure it reads as: the setup's, not
//  the story's, naming every check that failed.
struct PreflightCase{
    struct Input{
        std::string name;
        StoryPreflight preflight;
    };
    struct Expectation{
        std::string outcome;
    };
    Input input;
    Expectation expectation;
};

class Test_StoryPreflight : public UnitTest{
public:
    Test_StoryPreflight(PreflightCase test_case)
        : UnitTest(std::format("Pokemon::StoryRun - preflight: {}", test_case.input.name))
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::string got = to_string(m_case.input.preflight.check());
        if (got != m_case.expectation.outcome){
            return std::format("Expected \"{}\", got \"{}\"", m_case.expectation.outcome, got);
        }
        return true;
    }

private:
    const PreflightCase m_case;
};

const PreflightCase PREFLIGHT_CASES[] = {
    {.input = {.name = "a ready console passes", .preflight = READY}, .expectation = {.outcome = ""}},
    {
        .input = {.name = "the HOME menu", .preflight = ON_HOME},
        .expectation = {.outcome = "precondition_failed: the game is not running"},
    },
    {
        .input = {.name = "every check failed", .preflight = StoryPreflight{}},
        .expectation = {.outcome = "precondition_failed: no live video; the controller is not ready; the game is not running"},
    },
};


//  Whether the first checkpoint saves at its start: only a new game's. After
//  a load the save is already there; from the game as it is the place is
//  unconfirmed (console B, 2026-11-11: "save" ran first on HOME).
struct FirstSaveCase{
    struct Input{
        StoryStart start;
    };
    struct Expectation{
        bool saves;
    };
    Input input;
    Expectation expectation;
};

class Test_StoryFirstSave : public UnitTest{
public:
    Test_StoryFirstSave(FirstSaveCase test_case)
        : UnitTest(std::format("Pokemon::StoryRun - first save: {}", start_name(test_case.input.start)))
        , m_case(test_case)
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        if (story_first_save_allowed(m_case.input.start) != m_case.expectation.saves){
            return std::format("Expected saves={} from {}", m_case.expectation.saves, start_name(m_case.input.start));
        }
        return true;
    }

private:
    const FirstSaveCase m_case;
};

const FirstSaveCase FIRST_SAVE_CASES[] = {
    {.input = {.start = StoryStart::NEW_GAME}, .expectation = {.saves = true}},
    {.input = {.start = StoryStart::FROM_SAVE}, .expectation = {.saves = false}},
    {.input = {.start = StoryStart::LIVE}, .expectation = {.saves = false}},
};


//  A failed attempt's reload loads the save through the game.
struct ReloadCase{
    struct Input{
        std::string name;
    };
    struct Expectation{
        size_t loads;
        size_t rereads;
        std::string log_line;
    };
    Input input;
    Expectation expectation;
};

class Test_StoryReload : public UnitTest{
public:
    Test_StoryReload(const ReloadCase& test_case)
        : UnitTest(std::format("Pokemon::StoryRun - {}", test_case.input.name))
        , m_case(test_case)
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        FakeGame fake;
        StoryGameHooks game = fake.hooks();
        SegmentHooks hooks = story_segment_hooks(game);
        RunState state;
        const Outcome outcome = hooks.reload(state);
        const ReloadExpectation& expected = m_case.expectation;
        if (!outcome.succeeded() || fake.loads != expected.loads || fake.rereads != expected.rereads || !fake.logged(expected.log_line)){
            return std::format("Expected {} load and {} reread, got {} and {} ({})", expected.loads, expected.rereads, fake.loads, fake.rereads, to_string(outcome));
        }
        return true;
    }

private:
    const ReloadCase m_case;
};

const ReloadCase RELOAD_CASES[] = {
    {
        .input = {.name = "a retry reloads the save"},
        .expectation = {.loads = 1, .rereads = 1, .log_line = "[Story] retry: loading the save"},
    },
};


//  A run that stopped short throws, logs the failure line and touches
//  nothing in the game: no reload, no save, no reset to HOME.
struct FailureCase{
    struct Input{
        std::string name;
        std::string checkpoint;
        std::string event;
        std::string stopped_after;
    };
    struct Expectation{
        std::string message;
    };
    Input input;
    Expectation expectation;
};

class Test_StoryFailureLeavesGame : public UnitTest{
public:
    Test_StoryFailureLeavesGame(const FailureCase& test_case)
        : UnitTest(std::format("Pokemon::StoryRun - {}", test_case.input.name))
        , m_case(test_case)
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        FakeGame fake;
        StoryGameHooks game = fake.hooks();
        SegmentReport report;
        report.completed = false;
        report.failed_checkpoint = m_case.input.checkpoint;
        report.failed_event = m_case.input.event;
        report.failure = outcome_failed("expect_failed", "dialogue opened");
        AutoStoryStats stats;
        std::string message;
        try{
            finish_story_run(report, m_case.input.stopped_after, stats, game);
            return "Expected an OperationFailedException";
        }catch (OperationFailedException& e){
            message = e.message();
        }
        if (message != m_case.expectation.message){
            return std::format("Unexpected message: {}", message);
        }
        if (!fake.logged(STORY_LEFT_WHERE_FAILED)){
            return "Expected the failure line";
        }
        if (fake.loads != 0 || fake.saves != 0){
            return std::format("Expected the game untouched, got {} loads and {} saves", fake.loads, fake.saves);
        }
        return true;
    }

private:
    const FailureCase m_case;
};

const FailureCase FAILURE_CASES[] = {
    {
        .input = {.name = "a failed run leaves the game where it failed", .checkpoint = "00.04b", .event = "talk_mom", .stopped_after = "00.05"},
        .expectation = {.message = "AutoStory stopped at 00.04b talk_mom: expect_failed: dialogue opened"},
    },
};


//  A completed run logs and returns the line the finish notification sends.
struct FinishedCase{
    struct Input{
        std::string name;
        std::string last_checkpoint;
    };
    struct Expectation{
        std::string line;
    };
    Input input;
    Expectation expectation;
};

class Test_StoryFinished : public UnitTest{
public:
    Test_StoryFinished(const FinishedCase& test_case)
        : UnitTest(std::format("Pokemon::StoryRun - {}", test_case.input.name))
        , m_case(test_case)
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        FakeGame fake;
        StoryGameHooks game = fake.hooks();
        SegmentReport report;
        report.completed = true;
        AutoStoryStats stats;
        const std::string done = finish_story_run(report, m_case.input.last_checkpoint, stats, game);
        if (done != m_case.expectation.line || !fake.logged(done)){
            return std::format("Unexpected finish line: {}", done);
        }
        return true;
    }

private:
    const FinishedCase m_case;
};

const FinishedCase FINISHED_CASES[] = {
    {
        .input = {.name = "a completed run is done through its last checkpoint", .last_checkpoint = "00.05"},
        .expectation = {.line = "AutoStory: done through 00.05."},
    },
};


//  A verify run short of its passes throws with the summary.
struct VerifyCase{
    struct Input{
        std::string name;
        std::string checkpoint;
        size_t passes;
    };
    struct Expectation{
        std::string message;
    };
    Input input;
    Expectation expectation;
};

class Test_StoryVerifyFailed : public UnitTest{
public:
    Test_StoryVerifyFailed(const VerifyCase& test_case)
        : UnitTest(std::format("Pokemon::StoryRun - {}", test_case.input.name))
        , m_case(test_case)
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        FakeGame fake;
        StoryGameHooks game = fake.hooks();
        VerifyPolicy policy;
        VerifyReport report;
        report.passes = m_case.input.passes;
        report.verified = false;
        std::string message;
        try{
            finish_story_verify(m_case.input.checkpoint, policy, report, game);
            return "Expected an OperationFailedException";
        }catch (OperationFailedException& e){
            message = e.message();
        }
        if (message != m_case.expectation.message || !fake.logged(STORY_LEFT_WHERE_FAILED)){
            return std::format("Unexpected message: {}", message);
        }
        return true;
    }

private:
    const VerifyCase m_case;
};

const VerifyCase VERIFY_CASES[] = {
    {
        .input = {.name = "an unverified checkpoint fails with its summary", .checkpoint = "00.15b", .passes = 1},
        .expectation = {.message = "AutoStory verify: 00.15b 1/3 passed, 2 needed."},
    },
};

} // namespace


void add_tests_StoryRun(UnitTestDatabase& database){
    for (const StartCase& test_case : START_CASES){
        database.add<Test_StoryStart>(test_case);
    }
    for (const PreflightCase& test_case : PREFLIGHT_CASES){
        database.add<Test_StoryPreflight>(test_case);
    }
    for (const FirstSaveCase& test_case : FIRST_SAVE_CASES){
        database.add<Test_StoryFirstSave>(test_case);
    }
    for (const ReloadCase& test_case : RELOAD_CASES){
        database.add<Test_StoryReload>(test_case);
    }
    for (const FailureCase& test_case : FAILURE_CASES){
        database.add<Test_StoryFailureLeavesGame>(test_case);
    }
    for (const FinishedCase& test_case : FINISHED_CASES){
        database.add<Test_StoryFinished>(test_case);
    }
    for (const VerifyCase& test_case : VERIFY_CASES){
        database.add<Test_StoryVerifyFailed>(test_case);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
