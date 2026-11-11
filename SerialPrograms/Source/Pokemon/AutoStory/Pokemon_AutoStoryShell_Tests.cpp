/*  AutoStory Program Shell Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <string>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "Pokemon_AutoStoryShell.h"
#include "Pokemon_AutoStoryShell_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{

namespace{

//  A game that records what the shell asked of it.
struct FakeGame{
    std::string place_error;
    size_t loads = 0;
    size_t rereads = 0;
    size_t saves = 0;
    std::vector<std::string> lines;

    StoryGameHooks hooks(){
        StoryGameHooks game;
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
    std::string name;
    StoryStart start;
    std::string place_error;
    bool succeeds;
    size_t loads;
    size_t rereads;
    bool save_loaded;
};

class Test_StoryStart : public UnitTest{
public:
    Test_StoryStart(StartCase test_case)
        : UnitTest(std::format("Pokemon::AutoStoryShell - start: {}", test_case.name))
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        FakeGame fake;
        fake.place_error = m_case.place_error;
        StoryGameHooks game = fake.hooks();
        RunState state;
        const Outcome outcome = prepare_story_start("00.04b", m_case.start, game, state);
        const bool save_loaded = state.flags.contains("save_loaded");
        if (outcome.succeeded() != m_case.succeeds || fake.loads != m_case.loads
            || fake.rereads != m_case.rereads || save_loaded != m_case.save_loaded
        ){
            return std::format(
                "Expected ok={} loads={} rereads={} save_loaded={}, got {} {} {} {} ({})",
                m_case.succeeds, m_case.loads, m_case.rereads, m_case.save_loaded,
                outcome.succeeded(), fake.loads, fake.rereads, save_loaded, to_string(outcome)
            );
        }
        return true;
    }

private:
    const StartCase m_case;
};

const StartCase START_CASES[] = {
    //  Console B, 2026-11-11: a BDSP run started at 00.04b with the game
    //  left at 00.04d walked from 00.04d's position.
    {"a checkpoint with a save loads it first", StoryStart::FROM_SAVE, "", true, 1, 1, true},
    {"a save in the wrong place fails before rereading it", StoryStart::FROM_SAVE, "The save is in Pewter City.", false, 1, 0, false},
    {"a new game loads nothing", StoryStart::NEW_GAME, "", true, 0, 0, false},
    {"a live start loads nothing", StoryStart::LIVE, "", true, 0, 0, false},
};


//  A failed attempt's reload loads the save through the game.
class Test_StoryReload : public UnitTest{
public:
    Test_StoryReload()
        : UnitTest("Pokemon::AutoStoryShell - a retry reloads the save")
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        FakeGame fake;
        StoryGameHooks game = fake.hooks();
        SegmentHooks hooks = story_segment_hooks(game);
        RunState state;
        const Outcome outcome = hooks.reload(state);
        if (!outcome.succeeded() || fake.loads != 1 || fake.rereads != 1 || !fake.logged("[Story] retry: loading the save")){
            return std::format("Expected one load and reread, got {} and {} ({})", fake.loads, fake.rereads, to_string(outcome));
        }
        return true;
    }
};


//  A run that stopped short throws, logs the failure line and touches
//  nothing in the game: no reload, no save, no reset to HOME.
class Test_StoryFailureLeavesGame : public UnitTest{
public:
    Test_StoryFailureLeavesGame()
        : UnitTest("Pokemon::AutoStoryShell - a failed run leaves the game where it failed")
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        FakeGame fake;
        StoryGameHooks game = fake.hooks();
        SegmentReport report;
        report.completed = false;
        report.failed_checkpoint = "00.04b";
        report.failed_event = "talk_mom";
        report.failure = outcome_failed("expect_failed", "dialogue opened");
        AutoStoryStats stats;
        std::string message;
        try{
            finish_story_run(report, "00.05", stats, game);
            return "Expected an OperationFailedException";
        }catch (OperationFailedException& e){
            message = e.message();
        }
        if (message != "AutoStory stopped at 00.04b talk_mom: expect_failed: dialogue opened"){
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
};


//  A completed run logs and returns the line the finish notification sends.
class Test_StoryFinished : public UnitTest{
public:
    Test_StoryFinished()
        : UnitTest("Pokemon::AutoStoryShell - a completed run is done through its last checkpoint")
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        FakeGame fake;
        StoryGameHooks game = fake.hooks();
        SegmentReport report;
        report.completed = true;
        AutoStoryStats stats;
        const std::string done = finish_story_run(report, "00.05", stats, game);
        if (done != "AutoStory: done through 00.05." || !fake.logged(done)){
            return std::format("Unexpected finish line: {}", done);
        }
        return true;
    }
};


//  A verify run short of its passes throws with the summary.
class Test_StoryVerifyFailed : public UnitTest{
public:
    Test_StoryVerifyFailed()
        : UnitTest("Pokemon::AutoStoryShell - an unverified checkpoint fails with its summary")
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        FakeGame fake;
        StoryGameHooks game = fake.hooks();
        VerifyPolicy policy;
        VerifyReport report;
        report.passes = 1;
        report.verified = false;
        std::string message;
        try{
            finish_story_verify("00.15b", policy, report, game);
            return "Expected an OperationFailedException";
        }catch (OperationFailedException& e){
            message = e.message();
        }
        if (message != "AutoStory verify: 00.15b 1/3 passed, 2 needed." || !fake.logged(STORY_LEFT_WHERE_FAILED)){
            return std::format("Unexpected message: {}", message);
        }
        return true;
    }
};

} // namespace


void add_tests_AutoStoryShell(UnitTestDatabase& database){
    for (const StartCase& test_case : START_CASES){
        database.add<Test_StoryStart>(test_case);
    }
    database.add<Test_StoryReload>();
    database.add<Test_StoryFailureLeavesGame>();
    database.add<Test_StoryFinished>();
    database.add<Test_StoryVerifyFailed>();
}



} // namespace Pokemon
} // namespace PokemonAutomation
