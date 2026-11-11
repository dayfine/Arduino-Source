/*  Story Step JSON Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Each case reads a record from JSON: accepted cases check the values
 *  read, rejected cases check that the read fails. The Python reader's
 *  shared case file is checked on the Python side.
 *
 */

#include <format>
#include <iostream>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "Pokemon_StoryStepJson.h"
#ifndef PA_STORY_DATA_STANDALONE
#include "Pokemon_StoryStepJson_Tests.h"
#endif

namespace PokemonAutomation{
namespace Pokemon{


namespace{


//  Which record a case reads.
enum class Record{
    POSE,
    DECLARED,
    STEP,
    CHECKPOINT,
};

//  A record to read, and which kind it is.
struct WireInput{
    std::string_view name;
    Record record = Record::STEP;
    std::string_view json;
};

//  What a read gives.
using ReadRecord = std::variant<GridPose, DeclaredPose, StoryStep, StoryCheckpoint>;

//  "" when the record read is as expected, else what differs.
using ReadCheck = std::string (*)(const ReadRecord& record);

//  What reading it gives: a record the check accepts, or a rejection.
struct WireExpectation{
    ReadCheck check = nullptr;
    bool rejected = false;
};

struct WireCase{
    WireInput input;
    WireExpectation expectation;
};

std::string expect(bool ok, std::string_view what){
    return ok ? "" : std::format("Expected {}.", what);
}

const StoryStep& step_of(const ReadRecord& record){
    return std::get<StoryStep>(record);
}

const WireCase CASES[] = {
    {.input = {.name = "no args", .json = R"({"id":"talk","kind":"talk"})"}, .expectation = {.check = [](const ReadRecord& record){
        const StoryStep& step = step_of(record);
        return expect(step.name == "talk" && step.kind == StoryStepKind::TALK && step.args.empty() && !step.pose_after, "a bare talk step");
    }}},
    {.input = {.name = "all shared fields", .json = R"({"id":"all","kind":"game_action","why":"source reason","action":"profile","skip_if":["flag"],"declared_after":{"map":"B","tiles":[[0,0],[1,0]],"facing":"east"}})"}, .expectation = {.check = [](const ReadRecord& record){
        const StoryStep& step = step_of(record);
        const DeclaredPose after = TileDeclared{.map = "B", .tiles = {{0, 0}, {1, 0}}, .facing = Direction::EAST};
        return expect(step.kind == StoryStepKind::GAME && step.why == "source reason" && step.action == "profile"
            && step.skip_if == std::vector<std::string>{"flag"} && step.pose_after == after, "every shared field read");
    }}},
    {.input = {.name = "route fills its fallback", .json = R"({"id":"r","kind":"route","args":{"target":{"map":"A","tiles":[[1,2]]}}})"}, .expectation = {.check = [](const ReadRecord& record){
        const StoryActionArgs expected = {{"fight_wild", false}, {"target", GridPose{.map = "A", .tiles = {{1, 2}}, .facing = std::nullopt}}};
        return expect(step_of(record).args == expected, "the target and fight_wild false");
    }}},
    {.input = {.name = "legs", .json = R"({"id":"l","kind":"legs","action":"legs","args":{"legs":[{"direction":"south","tiles":2}],"fight_wild":true}})"}, .expectation = {.check = [](const ReadRecord& record){
        const StoryActionArgs expected = {{"fight_wild", true}, {"legs", std::vector<StoryLeg>{{.direction = Direction::SOUTH, .tiles = 2}}}};
        return expect(step_of(record).args == expected, "one south leg of 2 tiles");
    }}},
    {.input = {.name = "direction", .json = R"({"id":"s","kind":"step","args":{"direction":"west"}})"}, .expectation = {.check = [](const ReadRecord& record){
        const StoryActionArgs expected = {{"direction", Direction::WEST}};
        return expect(step_of(record).args == expected, "direction west");
    }}},
    {.input = {.name = "items", .json = R"({"id":"s","kind":"shop","args":{"items":[{"item":"potion","quantity":5}]}})"}, .expectation = {.check = [](const ReadRecord& record){
        const StoryActionArgs expected = {{"items", std::vector<StoryItem>{{.item = "potion", .quantity = 5}}}};
        return expect(step_of(record).args == expected, "5 potions");
    }}},
    {.input = {.name = "game args", .json = R"({"id":"buy","kind":"game_action","action":"mart","args":{"map":"CeruleanCity_Mart","budget":3000,"if_present":true}})"}, .expectation = {.check = [](const ReadRecord& record){
        const StoryActionArgs expected = {{"budget", int64_t(3000)}, {"if_present", true}, {"map", std::string("CeruleanCity_Mart")}};
        return expect(step_of(record).args == expected, "the Mart args");
    }}},
    {.input = {.name = "game words", .json = R"({"id":"buy","kind":"game_action","action":"mart","args":{"map":"M","budget":1,"skip":["a","b"]}})"}, .expectation = {.check = [](const ReadRecord& record){
        const StoryActionArgs expected = {{"budget", int64_t(1)}, {"if_present", false}, {"map", std::string("M")}, {"skip", std::vector<std::string>{"a", "b"}}};
        return expect(step_of(record).args == expected, "the words and if_present false");
    }}},
    {.input = {.name = "game integer rows", .json = R"({"id":"w","kind":"game_action","action":"walk","args":{"legs":[[0,4500],[-90,0],[]]}})"}, .expectation = {.check = [](const ReadRecord& record){
        const StoryActionArgs expected = {{"legs", std::vector<std::vector<int64_t>>{{0, 4500}, {-90, 0}, {}}}};
        return expect(step_of(record).args == expected, "three rows, the last empty");
    }}},
    {.input = {.name = "timed legs", .json = R"({"id":"w","kind":"game_action","action":"timed_walk","args":{"legs":[[0,4500],[-90,1200]]}})"}, .expectation = {.check = [](const ReadRecord& record){
        const StoryActionArgs expected = {{"legs", std::vector<StoryTimedLeg>{{.turn = 0, .ms = 4500}, {.turn = -90, .ms = 1200}}}};
        return expect(step_of(record).args == expected, "two timed legs");
    }}},
    {.input = {.name = "unknown facing stays absent", .record = Record::POSE, .json = R"({"map":"M","tiles":[[0,0]]})"}, .expectation = {.check = [](const ReadRecord& record){
        return expect(std::get<GridPose>(record) == GridPose{.map = "M", .tiles = {{0, 0}}, .facing = std::nullopt}, "one tile, no facing");
    }}},
    {.input = {.name = "no tiles", .record = Record::POSE, .json = R"({"map":"M","tiles":[]})"}, .expectation = {.check = [](const ReadRecord& record){
        return expect(std::get<GridPose>(record).tiles.empty(), "no tiles");
    }}},
    {.input = {.name = "declared tile pose", .record = Record::DECLARED, .json = R"({"map":"M","tiles":[[1,2]],"facing":"north"})"}, .expectation = {.check = [](const ReadRecord& record){
        const DeclaredPose expected = TileDeclared{.map = "M", .tiles = {{1, 2}}, .facing = Direction::NORTH};
        return expect(std::get<DeclaredPose>(record) == expected, "a tile pose facing north");
    }}},
    {.input = {.name = "declared place", .record = Record::DECLARED, .json = R"({"place":"LumioseCity","spot":"HotelZ","heading":90})"}, .expectation = {.check = [](const ReadRecord& record){
        const DeclaredPose expected = ZoneDeclared{.place = "LumioseCity", .spot = "HotelZ", .heading = 90};
        return expect(std::get<DeclaredPose>(record) == expected, "a zone pose with spot and heading");
    }}},
    {.input = {.name = "declared place alone", .record = Record::DECLARED, .json = R"({"place":"HotelZ"})"}, .expectation = {.check = [](const ReadRecord& record){
        const DeclaredPose expected = ZoneDeclared{.place = "HotelZ", .spot = std::nullopt, .heading = std::nullopt};
        return expect(std::get<DeclaredPose>(record) == expected, "a zone pose alone");
    }}},
    {.input = {.name = "checkpoint starting at a place", .record = Record::CHECKPOINT, .json = R"({"id":"03.01","title":"Lobby","start":{"place":"HotelZ"},"events":[{"id":"down","kind":"scene","declared_after":{"place":"HotelZLobby","heading":180}}]})"}, .expectation = {.check = [](const ReadRecord& record){
        const StoryCheckpoint& checkpoint = std::get<StoryCheckpoint>(record);
        const DeclaredPose start = ZoneDeclared{.place = "HotelZ", .spot = std::nullopt, .heading = std::nullopt};
        return expect(checkpoint.start == start && checkpoint.steps.size() == 1 && checkpoint.steps[0].kind == StoryStepKind::MASH, "a zone start and one scene");
    }}},
    {.input = {.name = "ordered checkpoint", .record = Record::CHECKPOINT, .json = R"({"id":"00.05","title":"Parcel","events":[ {"id":"first","kind":"talk"}, {"id":"second","kind":"scene"} ]})"}, .expectation = {.check = [](const ReadRecord& record){
        const StoryCheckpoint& checkpoint = std::get<StoryCheckpoint>(record);
        return expect(checkpoint.id == "00.05" && checkpoint.title == "Parcel" && !checkpoint.start && checkpoint.steps.size() == 2
            && checkpoint.steps[0].name == "first" && checkpoint.steps[1].name == "second"
            && checkpoint.steps_json == R"([{"id":"first","kind":"talk"},{"id":"second","kind":"scene"}])", "two steps in order and their compact JSON");
    }}},
    {.input = {.name = "checkpoint postconditions", .record = Record::CHECKPOINT, .json = R"({"id":"x","title":"X","events":[],"metadata":{"phase":"reproduction","postconditions":{"position":{"map":"M","tiles":[[3,4]]},"items":[{"item":"LIFT_KEY","quantity":1}],"flags":["f"]}}})"}, .expectation = {.check = [](const ReadRecord& record){
        const StoryCheckpoint& checkpoint = std::get<StoryCheckpoint>(record);
        const StoryPostconditions& post = checkpoint.postconditions;
        return expect(checkpoint.metadata.phase == StoryPhase::REPRODUCTION
            && post.position == GridPose{.map = "M", .tiles = {{3, 4}}, .facing = std::nullopt}
            && post.items == std::vector<StoryItem>{{.item = "LIFT_KEY", .quantity = 1}}
            && post.flags == std::vector<std::string>{"f"}, "the phase, position, item and flag");
    }}},
    {.input = {.name = "integer rows, not rows", .json = R"({"id":"w","kind":"game_action","action":"walk","args":{"legs":[0,4500]}})"}, .expectation = {.rejected = true}},
    {.input = {.name = "integer rows, fraction", .json = R"({"id":"w","kind":"game_action","action":"walk","args":{"legs":[[0,4.5]]}})"}, .expectation = {.rejected = true}},
    {.input = {.name = "timed leg of three numbers", .json = R"({"id":"w","kind":"game_action","action":"timed_walk","args":{"legs":[[0,4500,1]]}})"}, .expectation = {.rejected = true}},
    {.input = {.name = "unknown arg", .json = R"({"id":"a","kind":"game_action","action":"mart","args":{"map":"M","budget":1,"budgte":1}})"}, .expectation = {.rejected = true}},
    {.input = {.name = "missing required arg", .json = R"({"id":"a","kind":"game_action","action":"mart","args":{"map":"M"}})"}, .expectation = {.rejected = true}},
    {.input = {.name = "route without target", .json = R"({"id":"a","kind":"route"})"}, .expectation = {.rejected = true}},
    {.input = {.name = "args on a kind with none", .json = R"({"id":"a","kind":"talk","args":{"yes":true}})"}, .expectation = {.rejected = true}},
    {.input = {.name = "game step without action", .json = R"({"id":"a","kind":"game_action"})"}, .expectation = {.rejected = true}},
    {.input = {.name = "unknown action name", .json = R"({"id":"a","kind":"game_action","action":"dance"})"}, .expectation = {.rejected = true}},
    {.input = {.name = "fractional arg", .json = R"({"id":"a","kind":"game_action","action":"mart","args":{"map":"M","budget":1.5}})"}, .expectation = {.rejected = true}},
    {.input = {.name = "args not an object", .json = R"({"id":"a","kind":"game_action","action":"mart","args":["map"]})"}, .expectation = {.rejected = true}},
    {.input = {.name = "old field", .json = R"({"id":"a","kind":"battle","may_lose":true})"}, .expectation = {.rejected = true}},
    {.input = {.name = "old variant", .json = R"({"id":"a","kind":"game_action","variant":"mart"})"}, .expectation = {.rejected = true}},
    {.input = {.name = "unknown field", .json = R"({"id":"a","kind":"talk","mystery":0})"}, .expectation = {.rejected = true}},
    {.input = {.name = "unknown kind", .json = R"({"id":"a","kind":"magic"})"}, .expectation = {.rejected = true}},
    {.input = {.name = "boolean count", .json = R"({"id":"a","kind":"train","args":{"battles":false}})"}, .expectation = {.rejected = true}},
    {.input = {.name = "wrong bool", .json = R"({"id":"a","kind":"answer","args":{"yes":0}})"}, .expectation = {.rejected = true}},
    {.input = {.name = "old pose", .record = Record::POSE, .json = R"({"map":"M","certainty":"unknown","where":{"kind":"unknown"}})"}, .expectation = {.rejected = true}},
    {.input = {.name = "bad coordinate", .record = Record::POSE, .json = R"({"map":"M","tiles":[[0,true]]})"}, .expectation = {.rejected = true}},
    {.input = {.name = "declared map and place", .record = Record::DECLARED, .json = R"({"map":"M","place":"HotelZ"})"}, .expectation = {.rejected = true}},
    {.input = {.name = "declared place with tiles", .record = Record::DECLARED, .json = R"({"place":"HotelZ","tiles":[[1,2]]})"}, .expectation = {.rejected = true}},
    {.input = {.name = "declared map with heading", .record = Record::DECLARED, .json = R"({"map":"M","heading":90})"}, .expectation = {.rejected = true}},
    {.input = {.name = "declared neither", .record = Record::DECLARED, .json = R"({"spot":"Lobby"})"}, .expectation = {.rejected = true}},
    {.input = {.name = "duplicate events", .record = Record::CHECKPOINT, .json = R"({"id":"x","title":"X","events":[{"id":"a","kind":"talk"},{"id":"a","kind":"talk"}]})"}, .expectation = {.rejected = true}},
};


//  The built-in actions and a game's: a Mart step (GAME "mart") and a
//  profile check that takes no args.
std::vector<StoryActionSpec> test_actions(){
    std::vector<StoryActionSpec> actions(builtin_story_actions().begin(), builtin_story_actions().end());
    actions.push_back({.kind = StoryStepKind::GAME, .name = "mart", .args = {
        {.key = "map", .type = StoryArgType::WORD, .required = true, .fallback = std::nullopt, .why = "the Mart's map"},
        {.key = "budget", .type = StoryArgType::INTEGER, .required = true, .fallback = std::nullopt, .why = "money to spend"},
        {.key = "if_present", .type = StoryArgType::BOOLEAN, .required = false, .fallback = false, .why = ""},
        {.key = "skip", .type = StoryArgType::WORDS, .required = false, .fallback = std::nullopt, .why = ""},
    }});
    actions.push_back({.kind = StoryStepKind::GAME, .name = "walk", .args = {
        {.key = "legs", .type = StoryArgType::INTEGER_ROWS, .required = true, .fallback = std::nullopt, .why = "rows of numbers"},
    }});
    actions.push_back({.kind = StoryStepKind::GAME, .name = "timed_walk", .args = {
        {.key = "legs", .type = StoryArgType::TIMED_LEGS, .required = true, .fallback = std::nullopt, .why = "[turn degrees, ms] legs"},
    }});
    actions.push_back({.kind = StoryStepKind::GAME, .name = "profile", .args = {}});
    return actions;
}


//  `input` read.
ReadRecord read_record(const WireInput& input){
    static const std::vector<StoryActionSpec> actions = test_actions();
    const JsonValue value = parse_json(std::string(input.json));
    switch (input.record){
    case Record::POSE:
        return read_grid_pose(value, input.name);
    case Record::DECLARED:
        return read_declared_pose(value, input.name);
    case Record::STEP:
        return read_story_step(value, input.name, actions);
    case Record::CHECKPOINT:
        return read_story_checkpoint(value, input.name, actions);
    }
    throw ParseException("Unknown test record.");
}

//  "" when `test` reads as expected, else what differs.
std::string check_case(const WireCase& test){
    try{
        const ReadRecord record = read_record(test.input);
        if (test.expectation.rejected){
            return "Expected rejection, read accepted record.";
        }
        return test.expectation.check(record);
    }catch (const ParseException& error){
        return test.expectation.rejected ? "" : std::format("Unexpected parse failure: {}", error.message());
    }
}


#ifndef PA_STORY_DATA_STANDALONE

class StoryJsonTest : public UnitTest{
public:
    explicit StoryJsonTest(WireCase test)
        : UnitTest(std::format("Pokemon::StoryStepJson - {}", test.input.name))
        , m_case(test)
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::string error = check_case(m_case);
        return error.empty() ? UnitTestResult(true) : UnitTestResult(error);
    }

private:
    WireCase m_case;
};

#endif


} // namespace


#ifndef PA_STORY_DATA_STANDALONE

void add_tests_StoryStepJson(UnitTestDatabase& database){
    for (const WireCase& test : CASES){
        database.add<StoryJsonTest>(test);
    }
}

#endif



} // namespace Pokemon
} // namespace PokemonAutomation


#ifdef PA_STORY_DATA_STANDALONE

int main(){
    int failed = 0;
    for (const auto& test : PokemonAutomation::Pokemon::CASES){
        const std::string error = PokemonAutomation::Pokemon::check_case(test);
        std::cout << test.input.name << ": " << (error.empty() ? "PASS" : error) << '\n';
        failed += !error.empty();
    }
    return failed != 0;
}

#endif
