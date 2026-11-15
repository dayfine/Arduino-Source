/*  Story Step JSON Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Each case reads a record from JSON: accepted cases check the values
 *  read, rejected cases check that the read fails.
 *
 */

#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "Pokemon/AutoStory/Types/Pokemon_DeclaredPose.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryActionArg.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckpointMetadata.h"
#include "Pokemon_StoryStepJson.h"
#include "Pokemon_StoryStepJson_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{


//  Which record a case reads.
enum class Record{
    DECLARED,
    STEP,
    CHECKPOINT,
};

//  What a read gives.
using ReadRecord = std::variant<DeclaredPose, StoryStep, StoryCheckpoint>;

//  "" when the record read is as expected, else what differs.
using ReadCheck = std::string (*)(const ReadRecord& record);

struct WireCase{
    //  A record to read, and which kind it is.
    std::string_view name;
    struct Input{
        Record record = Record::STEP;
        std::string_view json;
    };

    //  What reading it gives: a record the check accepts, or a rejection.
    struct Expectation{
        ReadCheck check = nullptr;
        bool rejected = false;
    };

    Input input;
    Expectation expectation;
};

std::string expect(bool ok, std::string_view what){
    return ok ? "" : std::format("Expected {}.", what);
}

const StoryStep& step_of(const ReadRecord& record){
    return std::get<StoryStep>(record);
}

const WireCase CASES[] = {
    {.name = "no args", .input = {.json = R"({"id":"talk","kind":"talk"})"}, .expectation = {.check = [](const ReadRecord& record){
        const StoryStep& step = step_of(record);
        return expect(step.name == "talk" && step.kind == StoryStepKind::TALK && step.args.empty(), "a bare talk step");
    }}},
    {.name = "game args", .input = {.json = R"({"id":"buy","kind":"game_action","action":"mart","args":{"map":"CeruleanCity_Mart","budget":3000,"if_present":true}})"}, .expectation = {.check = [](const ReadRecord& record){
        const StoryActionArgs expected = {{"budget", int64_t(3000)}, {"if_present", true}, {"map", std::string("CeruleanCity_Mart")}};
        return expect(step_of(record).args == expected, "the Mart args");
    }}},
    {.name = "game words", .input = {.json = R"({"id":"buy","kind":"game_action","action":"mart","args":{"map":"M","budget":1,"skip":["a","b"]}})"}, .expectation = {.check = [](const ReadRecord& record){
        const StoryActionArgs expected = {{"budget", int64_t(1)}, {"if_present", false}, {"map", std::string("M")}, {"skip", std::vector<std::string>{"a", "b"}}};
        return expect(step_of(record).args == expected, "the words and if_present false");
    }}},
    {.name = "game integer rows", .input = {.json = R"({"id":"w","kind":"game_action","action":"walk","args":{"legs":[[0,4500],[-90,0],[]]}})"}, .expectation = {.check = [](const ReadRecord& record){
        const StoryActionArgs expected = {{"legs", std::vector<std::vector<int64_t>>{{0, 4500}, {-90, 0}, {}}}};
        return expect(step_of(record).args == expected, "three rows, the last empty");
    }}},
    {.name = "timed legs", .input = {.json = R"({"id":"w","kind":"game_action","action":"timed_walk","args":{"legs":[[0,4500],[-90,1200]]}})"}, .expectation = {.check = [](const ReadRecord& record){
        const StoryActionArgs expected = {{"legs", std::vector<StoryTimedLeg>{{.turn = 0, .ms = 4500}, {.turn = -90, .ms = 1200}}}};
        return expect(step_of(record).args == expected, "two timed legs");
    }}},
    {.name = "declared place", .input = {.record = Record::DECLARED, .json = R"({"place":"LumioseCity","spot":"HotelZ","heading":90})"}, .expectation = {.check = [](const ReadRecord& record){
        const DeclaredPose expected = ZoneDeclared{.place = "LumioseCity", .spot = "HotelZ", .heading = 90};
        return expect(std::get<DeclaredPose>(record) == expected, "a zone pose with spot and heading");
    }}},
    {.name = "declared place alone", .input = {.record = Record::DECLARED, .json = R"({"place":"HotelZ"})"}, .expectation = {.check = [](const ReadRecord& record){
        const DeclaredPose expected = ZoneDeclared{.place = "HotelZ", .spot = std::nullopt, .heading = std::nullopt};
        return expect(std::get<DeclaredPose>(record) == expected, "a zone pose alone");
    }}},
    {.name = "checkpoint starting at a place", .input = {.record = Record::CHECKPOINT, .json = R"({"id":"03.01","title":"Lobby","start":{"place":"HotelZ"},"events":[{"id":"down","kind":"scene"}]})"}, .expectation = {.check = [](const ReadRecord& record){
        const StoryCheckpoint& checkpoint = std::get<StoryCheckpoint>(record);
        const DeclaredPose start = ZoneDeclared{.place = "HotelZ", .spot = std::nullopt, .heading = std::nullopt};
        return expect(checkpoint.start == start && checkpoint.steps.size() == 1 && checkpoint.steps[0].kind == StoryStepKind::MASH, "a zone start and one scene");
    }}},
    {.name = "ordered checkpoint", .input = {.record = Record::CHECKPOINT, .json = R"({"id":"00.05","title":"Parcel","events":[ {"id":"first","kind":"talk"}, {"id":"second","kind":"scene"} ]})"}, .expectation = {.check = [](const ReadRecord& record){
        const StoryCheckpoint& checkpoint = std::get<StoryCheckpoint>(record);
        return expect(checkpoint.id == "00.05" && checkpoint.title == "Parcel" && !checkpoint.start && checkpoint.steps.size() == 2
            && checkpoint.steps[0].name == "first" && checkpoint.steps[1].name == "second", "two steps in order");
    }}},
    {.name = "checkpoint without metadata is testing", .input = {.record = Record::CHECKPOINT, .json = R"({"id":"x","title":"X","events":[]})"}, .expectation = {.check = [](const ReadRecord& record){
        const StoryCheckpoint& checkpoint = std::get<StoryCheckpoint>(record);
        return expect(checkpoint.metadata.phase == StoryPhase::TESTING, "the testing phase");
    }}},
    {.name = "integer rows, not rows", .input = {.json = R"({"id":"w","kind":"game_action","action":"walk","args":{"legs":[0,4500]}})"}, .expectation = {.rejected = true}},
    {.name = "integer rows, fraction", .input = {.json = R"({"id":"w","kind":"game_action","action":"walk","args":{"legs":[[0,4.5]]}})"}, .expectation = {.rejected = true}},
    {.name = "timed leg of three numbers", .input = {.json = R"({"id":"w","kind":"game_action","action":"timed_walk","args":{"legs":[[0,4500,1]]}})"}, .expectation = {.rejected = true}},
    {.name = "unknown arg", .input = {.json = R"({"id":"a","kind":"game_action","action":"mart","args":{"map":"M","budget":1,"budgte":1}})"}, .expectation = {.rejected = true}},
    {.name = "missing required arg", .input = {.json = R"({"id":"a","kind":"game_action","action":"mart","args":{"map":"M"}})"}, .expectation = {.rejected = true}},
    {.name = "args on a kind with none", .input = {.json = R"({"id":"a","kind":"talk","args":{"yes":true}})"}, .expectation = {.rejected = true}},
    {.name = "game step without action", .input = {.json = R"({"id":"a","kind":"game_action"})"}, .expectation = {.rejected = true}},
    {.name = "unknown action name", .input = {.json = R"({"id":"a","kind":"game_action","action":"dance"})"}, .expectation = {.rejected = true}},
    {.name = "fractional arg", .input = {.json = R"({"id":"a","kind":"game_action","action":"mart","args":{"map":"M","budget":1.5}})"}, .expectation = {.rejected = true}},
    {.name = "args not an object", .input = {.json = R"({"id":"a","kind":"game_action","action":"mart","args":["map"]})"}, .expectation = {.rejected = true}},
    {.name = "may_lose outside args", .input = {.json = R"({"id":"a","kind":"battle","may_lose":true})"}, .expectation = {.rejected = true}},
    {.name = "variant instead of action", .input = {.json = R"({"id":"a","kind":"game_action","variant":"mart"})"}, .expectation = {.rejected = true}},
    {.name = "unknown field", .input = {.json = R"({"id":"a","kind":"talk","mystery":0})"}, .expectation = {.rejected = true}},
    {.name = "unknown kind", .input = {.json = R"({"id":"a","kind":"magic"})"}, .expectation = {.rejected = true}},
    {.name = "declared place with tiles", .input = {.record = Record::DECLARED, .json = R"({"place":"HotelZ","tiles":[[1,2]]})"}, .expectation = {.rejected = true}},
    {.name = "declared neither", .input = {.record = Record::DECLARED, .json = R"({"spot":"Lobby"})"}, .expectation = {.rejected = true}},
    {.name = "unknown phase", .input = {.record = Record::CHECKPOINT, .json = R"({"id":"x","title":"X","events":[],"metadata":{"phase":"tested"}})"}, .expectation = {.rejected = true}},
    {.name = "unknown metadata field", .input = {.record = Record::CHECKPOINT, .json = R"({"id":"x","title":"X","events":[],"metadata":{"phase":"testing","runs":3}})"}, .expectation = {.rejected = true}},
    {.name = "duplicate events", .input = {.record = Record::CHECKPOINT, .json = R"({"id":"x","title":"X","events":[{"id":"a","kind":"talk"},{"id":"a","kind":"talk"}]})"}, .expectation = {.rejected = true}},
};


//  A game's actions: a Mart step (GAME "mart"), walks and a profile check
//  that takes no args.
std::vector<StoryActionSpec> test_actions(){
    std::vector<StoryActionSpec> actions;
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


//  `input` read, with `source` naming it in parse errors.
ReadRecord read_record(const WireCase::Input& input, std::string_view source){
    static const std::vector<StoryActionSpec> actions = test_actions();
    const JsonValue value = parse_json(std::string(input.json));
    switch (input.record){
    case Record::DECLARED:
        return read_declared_pose(value, source);
    case Record::STEP:
        return read_story_step(value, source, actions);
    case Record::CHECKPOINT:
        return read_story_checkpoint(value, source, actions);
    }
    throw ParseException("Unknown test record.");
}

//  "" when `test` reads as expected, else what differs.
std::string check_case(const WireCase& test){
    try{
        const ReadRecord record = read_record(test.input, test.name);
        if (test.expectation.rejected){
            return "Expected rejection, read accepted record.";
        }
        return test.expectation.check(record);
    }catch (const ParseException& error){
        return test.expectation.rejected ? "" : std::format("Unexpected parse failure: {}", error.message());
    }
}


class StoryJsonTest : public UnitTest{
public:
    explicit StoryJsonTest(WireCase test)
        : UnitTest(std::format("Pokemon::StoryStepJson - {}", test.name))
        , m_case(test)
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::string error = check_case(m_case);
        return error.empty() ? UnitTestResult(true) : UnitTestResult(error);
    }

private:
    WireCase m_case;
};


} // namespace


void add_tests_StoryStepJson(UnitTestDatabase& database){
    for (const WireCase& test : CASES){
        database.add<StoryJsonTest>(test);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
