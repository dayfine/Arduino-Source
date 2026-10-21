/*  Pokemon SV BBQ Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <filesystem>
#include <fstream>
#include "Common/Cpp/PrettyPrint.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "PokemonSV_BBQLogger.h"
#include "PokemonSV_BBQSnapshot.h"
#include "PokemonSV_BlueberryQuests.h"
#include "PokemonSV_BBQTests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{


namespace{

struct TeraEncounterScenario{
    std::string name;
    bool shiny = false;
    int encounter_watcher_result;       //  0: battle menu, 1: audio, -1: route finished first
    bool battle_menu_detected = false;  //  The route's own wait saw the battle menu.
    TeraEncounterAction expected;
};

class Test_TeraEncounterAction : public UnitTest{
public:
    Test_TeraEncounterAction(TeraEncounterScenario scenario)
        : UnitTest("PokemonSV::TeraEncounterAction - " + scenario.name)
        , m_scenario(std::move(scenario))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const TeraEncounterAction actual = tera_encounter_action(
            m_scenario.shiny, m_scenario.encounter_watcher_result, m_scenario.battle_menu_detected
        );
        if (actual != m_scenario.expected){
            return "Expected action " + std::to_string((int)m_scenario.expected)
                + ", got " + std::to_string((int)actual) + ".";
        }
        return true;
    }

private:
    const TeraEncounterScenario m_scenario;
};

}

static void add_tests_TeraEncounterAction(UnitTestDatabase& database){
    using enum TeraEncounterAction;
    const std::vector<TeraEncounterScenario> scenarios{
        {.name = "timeout recovers without battle", .encounter_watcher_result = -1, .expected = Recover},
        {.name = "audio alone is not entry", .encounter_watcher_result = 1, .expected = Recover},
        {.name = "unknown callback is not entry", .encounter_watcher_result = 2, .expected = Recover},
        {.name = "encounter watcher menu battles", .encounter_watcher_result = 0, .expected = Battle},
        {.name = "route wait menu battles", .encounter_watcher_result = -1, .battle_menu_detected = true, .expected = Battle},
        {.name = "audio then route wait menu battles", .encounter_watcher_result = 1, .battle_menu_detected = true, .expected = Battle},
        {.name = "both menus battle", .encounter_watcher_result = 0, .battle_menu_detected = true, .expected = Battle},
        {.name = "shiny stops without entry", .shiny = true, .encounter_watcher_result = -1, .expected = StopForShiny},
        {.name = "shiny stops before battle", .shiny = true, .encounter_watcher_result = 0, .battle_menu_detected = true, .expected = StopForShiny},
    };
    for (const TeraEncounterScenario& scenario : scenarios){
        database.add<Test_TeraEncounterAction>(scenario);
    }
}


namespace{

class BBQTestLogger : public Logger{
public:
    virtual void log(const std::string& message, Color) override{
        messages.push_back(message);
    }
    std::vector<std::string> messages;
};

class Test_BBQDiagnosticLogs : public UnitTest{
public:
    Test_BBQDiagnosticLogs(const std::string& scenario)
        : UnitTest("PokemonSV::BBQDiagnosticLogs - " + scenario), m_scenario(scenario)
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        BBQTestLogger logger;
        std::vector<std::string> expected;
        const BBQAttemptLog::Clock::time_point start{};
        if (m_scenario == "board"){
            BBQLogger(logger).board(true,
                {BBQuests::photo_swim, BBQuests::sneak_up, BBQuests::auto_10});
            BBQLogger(logger).board(false, {BBQuests::UnableToDetect});
            BBQLogger(logger).board(true, {});
            expected = {
                "BBQ event=board panel_detected=1 quests=photo-swim,sneak-up,auto-10",
                "BBQ event=board panel_detected=0 quests=unknown",
                "BBQ event=board panel_detected=1 quests=",
            };
        }else if (m_scenario == "success"){
            BBQAttemptLog attempt_log(logger, BBQuests::make_tm, 1, 2, start);
            attempt_log.route_return(start + std::chrono::milliseconds(2000));
            attempt_log.result(true, start + std::chrono::milliseconds(2500));
            expected = {
                "BBQ quest=make-tm attempt=1 limit=2 event=start",
                "BBQ quest=make-tm attempt=1 limit=2 event=route_return elapsed_ms=2000 elapsed=\"2.000 seconds\"",
                "BBQ quest=make-tm attempt=1 limit=2 event=result outcome=absent_from_board verification_ms=500 verification=\"0.500 seconds\" total_ms=2500 total=\"2.500 seconds\"",
            };
        }else if (m_scenario == "retry-exhausted"){
            BBQLogger(logger).battle_entry_timeout(BBQuests::catch_any);
            BBQAttemptLog attempt_log(logger, BBQuests::catch_any, 2, 2, start);
            attempt_log.route_return(start + std::chrono::milliseconds(3345));
            attempt_log.result(false, start + std::chrono::milliseconds(4110));
            BBQLogger(logger).retry_exhausted(BBQuests::catch_any);
            expected = {
                "BBQ quest=catch-any event=battle_entry outcome=timeout",
                "BBQ quest=catch-any attempt=2 limit=2 event=start",
                "BBQ quest=catch-any attempt=2 limit=2 event=route_return elapsed_ms=3345 elapsed=\"3.345 seconds\"",
                "BBQ quest=catch-any attempt=2 limit=2 event=result outcome=still_on_board verification_ms=765 verification=\"0.765 seconds\" total_ms=4110 total=\"4.110 seconds\"",
                "BBQ quest=catch-any event=retry_exhausted recovery=skip_current_batch",
            };
        }else if (m_scenario == "long-duration"){
            BBQAttemptLog attempt_log(logger, BBQuests::UnableToDetect, 256, 256, start);
            attempt_log.route_return(start + std::chrono::milliseconds(120000));
            attempt_log.result(false, start + std::chrono::milliseconds(120000));
            expected = {
                "BBQ quest=unknown attempt=256 limit=256 event=start",
                "BBQ quest=unknown attempt=256 limit=256 event=route_return elapsed_ms=120000 elapsed=\"2.000 minutes\"",
                "BBQ quest=unknown attempt=256 limit=256 event=result outcome=still_on_board verification_ms=0 verification=\"0.000 seconds\" total_ms=120000 total=\"2.000 minutes\"",
            };
        }else{
            return "Unknown BBQ log test scenario.";
        }
        if (logger.messages != expected){
            return "Unexpected BBQ diagnostic log sequence for " + m_scenario;
        }
        return true;
    }

private:
    const std::string m_scenario;
};

}

static void add_tests_BBQDiagnosticLogs(UnitTestDatabase& database){
    for (const char* scenario : {"board", "success", "retry-exhausted", "long-duration"}){
        database.add<Test_BBQDiagnosticLogs>(scenario);
    }
}


namespace{

// A fresh folder under the system temp folder, removed afterwards.
class TempFolder{
public:
    TempFolder()
        : path(std::filesystem::temp_directory_path() / ("bbq-snapshot-test-" + now_to_filestring()))
    {
        std::filesystem::create_directories(path);
    }
    ~TempFolder(){
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }
    const std::filesystem::path path;
};

ImageRGB32 small_frame(){
    ImageRGB32 frame(4, 4);
    frame.fill(0xff00ff00);
    return frame;
}

class Test_BBQSnapshotSaved : public UnitTest{
public:
    Test_BBQSnapshotSaved() : UnitTest("PokemonSV::BBQSnapshot - saves into a new folder") {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        TempFolder temp;
        const std::string directory = (temp.path / "BBQ").string() + "/";
        BBQTestLogger logger;

        const std::string path = save_bbq_snapshot(
            logger, small_frame(), directory, BBQuests::make_tm, "verification-failed"
        );

        const std::string suffix = "-make-tm-verification-failed.png";
        if (path.rfind(directory, 0) != 0 || path.size() < suffix.size()
            || path.compare(path.size() - suffix.size(), suffix.size(), suffix) != 0
        ){
            return "Unexpected snapshot path: " + path;
        }
        if (!std::filesystem::exists(path)){
            return "Snapshot was not written: " + path;
        }
        if (logger.messages != std::vector<std::string>{"BBQ event=snapshot saved=1 path=" + path}){
            return "Unexpected snapshot log.";
        }
        return true;
    }
};

class Test_BBQSnapshotFailed : public UnitTest{
public:
    Test_BBQSnapshotFailed() : UnitTest("PokemonSV::BBQSnapshot - reports a failed save") {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        // The folder cannot be created because a file already has its name.
        TempFolder temp;
        std::ofstream(temp.path / "BBQ") << "not a folder";
        const std::string directory = (temp.path / "BBQ").string() + "/";
        BBQTestLogger logger;

        const std::string path = save_bbq_snapshot(
            logger, small_frame(), directory, BBQuests::make_tm, "verification-failed"
        );

        if (!path.empty()){
            return "Expected no path for a failed save, got: " + path;
        }
        const std::string expected_prefix = "BBQ event=snapshot saved=0 path=" + directory;
        if (logger.messages.size() != 1 || logger.messages[0].rfind(expected_prefix, 0) != 0){
            return "Unexpected snapshot log.";
        }
        return true;
    }
};

}

static void add_tests_BBQSnapshot(UnitTestDatabase& database){
    database.add<Test_BBQSnapshotSaved>();
    database.add<Test_BBQSnapshotFailed>();
}


void add_tests_BBQ(UnitTestDatabase& database){
    add_tests_TeraEncounterAction(database);
    add_tests_BBQDiagnosticLogs(database);
    add_tests_BBQSnapshot(database);
}



}
}
}
