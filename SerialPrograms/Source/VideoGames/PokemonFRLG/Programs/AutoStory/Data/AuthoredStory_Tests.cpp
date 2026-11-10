/*  FRLG Authored Story Tests
 *  From: https://github.com/PokemonAutomation/
 *
 *  The story table as loaded from the StoryData plans: the trace of the
 *  whole story on the story maps (StoryMapsSim) against the golden one
 *  taken from the C++ rows they were exported from, what a plan may not
 *  say, and the sections in the log.
 *
 */

#include <format>
#include <string>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Pokemon_StorySegments.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryRowSim.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryTable.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_StoryReplay.h"
#include "JsonParser.h"
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

//  00.28's plan, changed by `mutate`, read as an authored plan.
StoryCheckpoint read_changed_rock_tunnel(void (*mutate)(JsonObject& root)){
    JsonValue plan;
    for (const auto& [name, text] : autostory_checkpoint_json()){
        if (name == "frlg-00.28.plan"){
            plan = parse_json(text);
        }
    }
    mutate(plan.to_object_throw());
    return read_authored_checkpoint(plan, "frlg-00.28.plan (changed)");
}

JsonObject& first_event(JsonObject& root){
    JsonArray& sections = root.get_object_throw("checkpoint").get_array_throw("sections");
    return sections[0].to_object_throw().get_array_throw("events")[0].to_object_throw();
}

void unknown_event_field(JsonObject& root){
    first_event(root)["speed"] = "fast";
}
void unknown_variant(JsonObject& root){
    first_event(root)["variant"] = "bike";
}
void unknown_game_action(JsonObject& root){
    JsonObject& event = first_event(root);
    event["kind"] = "game_action";
    event["variant"] = "mart Nowhere";
}
void events_beside_sections(JsonObject& root){
    root.get_object_throw("checkpoint")["events"] = JsonArray();
}
void unknown_execution_field(JsonObject& root){
    root.get_object_throw("execution")["repel"] = true;
}
void unknown_lead(JsonObject& root){
    JsonObject supplies;
    supplies["lead"] = "after_misty";
    root.get_object_throw("execution")["supplies"] = std::move(supplies);
}
void empty_section(JsonObject& root){
    JsonObject section;
    section["id"] = "nothing";
    section["title"] = "no events";
    section["events"] = JsonArray();
    root.get_object_throw("checkpoint").get_array_throw("sections").push_back(std::move(section));
}
void repeated_section(JsonObject& root){
    JsonArray& sections = root.get_object_throw("checkpoint").get_array_throw("sections");
    sections[1].to_object_throw()["id"] = sections[0].to_object_throw().get_string_throw("id");
}

struct RejectCase{
    std::string_view name;
    void (*mutate)(JsonObject& root);
};
const RejectCase REJECT_CASES[] = {
    {"an unknown event field", unknown_event_field},
    {"a variant FRLG doesn't do", unknown_variant},
    {"a GAME step FRLG doesn't do", unknown_game_action},
    {"events beside sections", events_beside_sections},
    {"an unknown execution field", unknown_execution_field},
    {"an unknown lead", unknown_lead},
    {"a section without events", empty_section},
    {"two sections with one id", repeated_section},
};

void unchanged(JsonObject&){}

UnitTestResult test_rejects(){
    //  Unchanged, it reads: each rejection is the change's.
    if (read_changed_rock_tunnel(unchanged).id != "00.28"){
        return "Expected the unchanged plan to read as 00.28.";
    }
    for (const RejectCase& test : REJECT_CASES){
        try{
            read_changed_rock_tunnel(test.mutate);
            return std::format("Expected {} to be rejected.", test.name);
        }catch (const ParseException&){}
    }
    return true;
}

//  00.28 in its sections, numbered across them, each event's line naming
//  its section: Winston is event 12 (after from_save), on B1F's east side.
UnitTestResult test_rock_tunnel_sections(){
    const StoryCheckpoint* rock_tunnel = nullptr;
    for (const StoryCheckpoint& row : story_checkpoints()){
        if (row.id == "00.28"){
            rock_tunnel = &row;
        }
    }
    if (rock_tunnel == nullptr){
        return "Expected a checkpoint 00.28.";
    }
    std::vector<std::string> ids;
    for (const Pokemon::StorySection& section : rock_tunnel->sections){
        ids.push_back(section.id);
    }
    const std::vector<std::string> expected{
        "route10_to_mouth", "1f_north", "b1f_east", "1f_west", "b1f_west", "1f_south", "route10_south",
    };
    if (ids != expected){
        return "Expected 00.28's sections by area, read " + std::to_string(ids.size()) + " others.";
    }
    StoryMapsSim game;
    Pokemon::Checkpoint checkpoint = Pokemon::build_story_checkpoint(*rock_tunnel, game);
    const Pokemon::Event& winston = checkpoint.events[11];
    const std::string line = Pokemon::event_log_line(checkpoint.id, 12, winston.name, "start", winston.section);
    if (line != "[Event] 00.28/12 [b1f_east] winston_line: start"){
        return "Expected Winston's line in b1f_east, read: " + line;
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

struct AuthoredInput{
    std::string name;
    UnitTestResult (*body)();
};

struct AuthoredExpectation{
    std::string description;
};

struct AuthoredTestCase{
    AuthoredInput input;
    AuthoredExpectation expectation;
};

const AuthoredTestCase AUTHORED_CASES[] = {
    {.input = {.name = "the story sim's trace is the golden one", .body = test_trace_golden},
     .expectation = {.description = "the trace the C++ rows gave"}},
    {.input = {.name = "malformed plans are rejected", .body = test_rejects},
     .expectation = {.description = "a ParseException for each change"}},
    {.input = {.name = "Rock Tunnel's sections", .body = test_rock_tunnel_sections},
     .expectation = {.description = "00.28 split by area, sections in its log lines"}},
};

class AuthoredTest : public UnitTest{
public:
    explicit AuthoredTest(AuthoredTestCase test)
        : UnitTest("PokemonFRLG::AutoStory - Authored: " + test.input.name), m_test(std::move(test)){}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        try{
            return m_test.input.body();
        }catch (Exception& e){
            return std::string(e.name()) + ": " + e.message();
        }
    }
private:
    AuthoredTestCase m_test;
};

} // namespace


void add_tests_AuthoredStory(UnitTestDatabase& database){
    for (const AuthoredTestCase& test : AUTHORED_CASES){
        database.add<AuthoredTest>(test);
    }
}


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
