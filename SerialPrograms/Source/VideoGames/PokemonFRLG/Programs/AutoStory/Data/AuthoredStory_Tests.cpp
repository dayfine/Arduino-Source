/*  FRLG Authored Story Tests
 *  From: https://github.com/PokemonAutomation/
 *
 *  The story table as loaded from the StoryData plans against the C++
 *  rows it was exported from: the same events, and the same trace of the
 *  whole story on the story maps (StoryMapsSim).
 *
 */

#include <format>
#include <string>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/Filesystem/FileIO.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Pokemon_StorySegments.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryRowSim.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryTable.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_StoryReplay.h"
#include "JsonParser.h"
#include "JsonWriter.h"
#include "AuthoredStory_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

//  "[Event] 00.28/12 [b1f_east] winston_line: start" without its section,
//  as the table logged it before it had sections.
std::string without_section(const std::string& line){
    const size_t open = line.find(" [", 1);
    const size_t close = open == std::string::npos ? open : line.find("] ", open);
    if (!line.starts_with("[Event] ") || close == std::string::npos){
        return line;
    }
    return line.substr(0, open) + line.substr(close + 1);
}

//  The story on the story maps, each step's end pose traced.
class TraceGame : public StoryMapsSim{
public:
    std::vector<std::string> trace;

    virtual void begin_attempt(const Pokemon::StoryCheckpoint& data) override{
        StoryMapsSim::begin_attempt(data);
        m_row = data.id;
        trace.push_back("begin " + data.id);
    }
    virtual Pokemon::Outcome run(Pokemon::RunState& state, const StoryStep& step) override{
        Pokemon::Outcome outcome = StoryMapsSim::run(state, step);
        trace.push_back(std::format("{}/{}: {} at {}", m_row, step.name, outcome.succeeded() ? "ok" : "failed",
            outcome.pose ? Pokemon::to_string(*outcome.pose) : "-"));
        return outcome;
    }

private:
    std::string m_row;
};

} // namespace

std::vector<std::string> story_sim_trace(const std::vector<StoryCheckpoint>& table){
    const std::vector<StoryCheckpoint> rows = plan_story_run(table, 0, table.size() - 1, StoryHandoffs{}).rows;
    TraceGame game;
    game.set_language(Language::Japanese);
    Pokemon::Segment segment;
    segment.id = "00";
    for (const StoryCheckpoint& data : rows){
        segment.checkpoints.push_back(Pokemon::CheckpointSpec{
            .id = data.id, .title = data.title, .saves = true,
            .build = [&game, &data]{
                return Pokemon::build_story_checkpoint(data, game);
            },
        });
    }
    Pokemon::SegmentHooks hooks;
    hooks.save = [&game](Pokemon::RunState& state){
        game.trace.push_back("save at " + Pokemon::to_string(state.pose));
        return Pokemon::outcome_ok();
    };
    hooks.reload = [&game](Pokemon::RunState&){
        game.trace.push_back("reload");
        return Pokemon::outcome_ok();
    };
    hooks.log = [&game](const std::string& line){
        game.trace.push_back(without_section(line));
    };
    Pokemon::RunState state;
    const Pokemon::SegmentReport report = Pokemon::run_segment(state, segment, {}, Pokemon::SegmentPolicy{}, hooks);
    game.trace.push_back(report.completed ? "completed" : "not completed");
    game.trace.push_back(stub_report(game.stubs().rows()));
    return game.trace;
}


namespace{

//  The row as the C++ table built it: no notes or sections.
JsonValue plain_export(StoryCheckpoint row){
    row.notes.clear();
    row.sections.clear();
    for (StoryStep& step : row.steps){
        step.notes.clear();
        step.section.clear();
    }
    return story_checkpoint_export(row);
}

std::string compare_rows(const StoryCheckpoint& authored, const StoryCheckpoint& table){
    const JsonValue a = plain_export(authored);
    const JsonValue b = plain_export(table);
    if (!(a == b)){
        return std::format("{}: authored {}\nC++ {}", table.id, a.dump(-1), b.dump(-1));
    }
    if (authored.live_start != table.live_start || authored.handoff != table.handoff){
        return table.id + ": live start or handoff differ.";
    }
    return "";
}

UnitTestResult test_rows_match(){
    const std::vector<StoryCheckpoint> authored = load_authored_story();
    const std::vector<StoryCheckpoint>& table = story_checkpoints();
    if (authored.size() != table.size()){
        return std::format("Expected {} authored rows, read {}.", table.size(), authored.size());
    }
    for (size_t c = 0; c < table.size(); c++){
        const std::string error = compare_rows(authored[c], table[c]);
        if (!error.empty()){
            return error;
        }
    }
    return true;
}

UnitTestResult test_traces_match(){
    const std::vector<std::string> authored = story_sim_trace(load_authored_story());
    const std::vector<std::string> table = story_sim_trace(story_checkpoints());
    for (size_t c = 0; c < std::min(authored.size(), table.size()); c++){
        if (authored[c] != table[c]){
            return std::format("Trace line {}: authored \"{}\", C++ \"{}\".", c, authored[c], table[c]);
        }
    }
    if (authored.size() != table.size()){
        return std::format("Expected {} trace lines, read {}.", table.size(), authored.size());
    }
    return true;
}

//  The story table's trace against the golden one, taken from the C++
//  rows before they moved to StoryData (ReplayData/frlg-story-sim-trace.json).
UnitTestResult test_trace_golden(){
    const JsonValue fixture = parse_json(story_replay_fixture("frlg-story-sim-trace"));
    const JsonArray& golden = fixture.to_object_throw().get_array_throw("trace");
    const std::vector<std::string> trace = story_sim_trace(story_checkpoints());
    for (size_t c = 0; c < std::min(golden.size(), trace.size()); c++){
        if (golden[c].to_string_throw() != trace[c]){
            return std::format("Trace line {}: golden \"{}\", read \"{}\".", c, golden[c].to_string_throw(), trace[c]);
        }
    }
    if (golden.size() != trace.size()){
        return std::format("Expected {} trace lines, read {}.", golden.size(), trace.size());
    }
    return true;
}

//  Migration only: writes each C++ row as its authored plan, and the C++
//  table's trace, to frlg-story-export/ under the working directory.
UnitTestResult test_export(){
    const std::string folder = "frlg-story-export/";
    Filesystem::create_directories(Filesystem::Path(folder));
    for (const StoryCheckpoint& row : story_checkpoints()){
        string_to_file(folder + "frlg-" + row.id + ".plan.json", authored_checkpoint_json(row).dump(2));
    }
    JsonArray lines;
    for (const std::string& line : story_sim_trace(story_checkpoints())){
        lines.push_back(line);
    }
    string_to_file(folder + "frlg-story-sim-trace.json", JsonValue(std::move(lines)).dump(1));
    return true;
}

struct AuthoredCase{
    std::string name;
    UnitTestResult (*body)();
};

class AuthoredTest : public UnitTest{
public:
    explicit AuthoredTest(AuthoredCase test)
        : UnitTest("PokemonFRLG::AutoStory - Authored: " + test.name), m_test(std::move(test)){}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        try{
            return m_test.body();
        }catch (Exception& e){
            return std::string(e.name()) + ": " + e.message();
        }
    }
private:
    AuthoredCase m_test;
};

} // namespace


void add_tests_AuthoredStory(UnitTestDatabase& database){
    database.add<AuthoredTest>(AuthoredCase{"every row as authored is the C++ row", test_rows_match});
    database.add<AuthoredTest>(AuthoredCase{"the story sim's trace is the C++ table's", test_traces_match});
    database.add<AuthoredTest>(AuthoredCase{"export the C++ rows", test_export});
    database.add<AuthoredTest>(AuthoredCase{"the story sim's trace is the golden one", test_trace_golden});
}


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
