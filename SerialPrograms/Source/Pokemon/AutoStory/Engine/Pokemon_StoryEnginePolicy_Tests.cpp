/*  Story Engine Policy Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The engine's whole run of a checkpoint on a scripted backend: the save
 *  mode from the checkpoint's phase and steps hash, the postconditions
 *  (passed, failed, not checked) and whether it saved. Then the steps hash
 *  on its own, the metadata in the JSON, and ScreenMachine states that
 *  match on an observation.
 *
 */

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonTools/ScreenMachine/ScreenMachine.h"
#include "Pokemon/AutoStory/Data/Pokemon_StoryStepJson.h"
#include "Pokemon_StoryEngine.h"
#include "Pokemon_StoryFakeBackend.h"
#include "Pokemon_StoryEngine_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{


//  FRLG 00.31's end in the shared format, its steps given: beside B3F's
//  Rare Candy, facing north, with two Rare Candies.
std::string checkpoint_0031(std::string_view phase, std::string_view steps){
    return std::string(R"({"id": "00.31", "title": "Rocket Hideout", "events": )") + std::string(steps) + R"(,
        "metadata": {"phase": ")" + std::string(phase) + R"(", "source": "test", "postconditions": {
            "position": {"map": "RocketHideout_B3F", "tiles": [[12, 13]], "facing": "north"},
            "items": [{"item": "RARE_CANDY", "quantity": 2}]
        }}})";
}

inline constexpr std::string_view TALK_STEPS = R"([{"id": "b3f_rare_candy", "kind": "talk", "why": "the second candy"}])";
inline constexpr std::string_view SAVE_STEPS = R"([{"id": "b3f_rare_candy", "kind": "talk"}, {"id": "save_here", "kind": "save"}])";

//  The verification a checkpoint records.
enum class HashRecord{
    MATCHING,   //  of these steps
    STALE,      //  of other steps
    MISSING,    //  none
};

//  A look at B3F (12, 13) or wherever `map` is.
Observation<GridPose> end_look(std::string map){
    Observation<GridPose> observation;
    observation.events = ScreenEvents{ScreenEvent::CAN_MOVE};
    observation.pose = GridPose{.map = std::move(map), .tiles = {{12, 13}}, .facing = Direction::NORTH};
    return observation;
}

//  A game whose save is recorded as an input, and whose "bag" reader
//  reads `candies` Rare Candies (no reader when unset).
StoryGame<GridPose> engine_game(std::optional<int> candies){
    StoryGame<GridPose> game;
    game.id = "test";
    game.save = [](StoryBackend<GridPose>& backend){
        backend.wait(Milliseconds(1));
        backend.log("[Test] save");
        return outcome_ok();
    };
    if (candies){
        game.readers.emplace_back("bag", [count = *candies](StoryBackend<GridPose>&, Observation<GridPose>& observation){
            observation.readings["bag.RARE_CANDY"] = count;
        });
    }
    return game;
}



//  One engine run of 00.31 in `phase`, the player ending on `end_map`.
struct EngineInput{
    std::string_view name;
    std::string_view phase;
    HashRecord hash = HashRecord::MATCHING;
    std::string_view steps = TALK_STEPS;
    //  Where the talk leaves the player; empty: the talk opens no text.
    std::string_view end_map = "RocketHideout_B3F";
    std::optional<int> candies = 2;
};

//  The save mode chosen, the verdict, whether the game saved (each save
//  logs "[Test] save") and a line the run logged.
struct EngineExpectation{
    StorySaveMode mode = StorySaveMode::NONE;
    CheckpointVerdict verdict = CheckpointVerdict::PASSED;
    size_t saves = 0;
    std::string_view line = {};
};

struct EngineCase{
    EngineInput input;
    EngineExpectation expectation;
};

const EngineCase ENGINE_CASES[] = {
    {.input = {.name = "testing never saves", .phase = "testing"},
     .expectation = {.mode = StorySaveMode::NONE, .saves = 0, .line = "[Save] 00.31: phase testing: save mode none"}},
    {.input = {.name = "verification saves at the end", .phase = "verification"},
     .expectation = {.mode = StorySaveMode::AFTER_VERIFICATION, .saves = 1, .line = "[Save] 00.31: saved"}},
    {.input = {.name = "reproduction saves", .phase = "reproduction"},
     .expectation = {.mode = StorySaveMode::PER_CHECKPOINT, .saves = 1, .line = "[Save] 00.31: phase reproduction: save mode per_checkpoint"}},
    {.input = {.name = "stale steps_hash is testing", .phase = "verification", .hash = HashRecord::STALE},
     .expectation = {.mode = StorySaveMode::NONE, .saves = 0, .line = "[Save] 00.31: phase verification but steps_hash mismatch (recorded sha256:old, now "}},
    {.input = {.name = "no steps_hash is testing", .phase = "reproduction", .hash = HashRecord::MISSING},
     .expectation = {.mode = StorySaveMode::NONE, .saves = 0, .line = "[Save] 00.31: phase reproduction but steps_hash mismatch (recorded none, now "}},
    {.input = {.name = "B4F false pass fails postconditions", .phase = "verification", .end_map = "RocketHideout_B4F"},
     .expectation = {.mode = StorySaveMode::AFTER_VERIFICATION, .verdict = CheckpointVerdict::FAILED, .saves = 0,
                     .line = "[Postcondition] 00.31 map: failed: expected RocketHideout_B3F, read RocketHideout_B4F"}},
    {.input = {.name = "unread bag is not checked", .phase = "verification", .candies = std::nullopt},
     .expectation = {.mode = StorySaveMode::AFTER_VERIFICATION, .saves = 1,
                     .line = "[Postcondition] 00.31: 3 passed, 0 failed, 1 not checked: incomplete"}},
    {.input = {.name = "wrong candy count fails", .phase = "reproduction", .candies = 1},
     .expectation = {.mode = StorySaveMode::PER_CHECKPOINT, .verdict = CheckpointVerdict::FAILED, .saves = 0,
                     .line = "[Postcondition] 00.31 item RARE_CANDY: failed: expected 2, read 1"}},
    {.input = {.name = "failed run never saves", .phase = "reproduction", .end_map = ""},
     .expectation = {.mode = StorySaveMode::PER_CHECKPOINT, .verdict = CheckpointVerdict::FAILED, .saves = 0,
                     .line = "[Save] 00.31: run failed: not saved"}},
    {.input = {.name = "save step skipped while verifying", .phase = "verification", .steps = SAVE_STEPS},
     .expectation = {.mode = StorySaveMode::AFTER_VERIFICATION, .saves = 1, .line = "[Save] save step skipped: save mode after_verification"}},
    {.input = {.name = "save step saves in reproduction", .phase = "reproduction", .steps = SAVE_STEPS},
     .expectation = {.mode = StorySaveMode::PER_CHECKPOINT, .saves = 2, .line = "[Step] save_here SAVE/default: ok"}},
};

//  A text box, then the overworld on `end_map`; only the overworld when
//  `end_map` is empty.
std::vector<Observation<GridPose>> engine_looks(std::string_view end_map){
    if (end_map.empty()){
        return {end_look("RocketHideout_B3F")};
    }
    Observation<GridPose> text;
    text.events = ScreenEvents{ScreenEvent::TEXT_BOX};
    return {std::move(text), end_look(std::string(end_map))};
}

std::string check_engine_lines(const EngineCase& test, const FakeStoryBackend& backend){
    size_t saves = 0;
    bool found = false;
    for (const std::string& line : backend.lines()){
        saves += line == "[Test] save" ? 1 : 0;
        found |= line.starts_with(test.expectation.line);
    }
    if (saves != test.expectation.saves){
        return "expected " + std::to_string(test.expectation.saves) + " saves, got " + std::to_string(saves);
    }
    return found ? "" : "expected a line starting \"" + std::string(test.expectation.line) + "\"";
}

std::string check_engine(const EngineCase& test, CancellableScope& scope){
    const StoryGame<GridPose> game = engine_game(test.input.candies);
    FakeStoryBackend backend({.looks = engine_looks(test.input.end_map)}, scope);
    StoryEngine<GridPose> engine(game, backend);
    StoryCheckpoint checkpoint = read_story_checkpoint(
        parse_json(checkpoint_0031(test.input.phase, test.input.steps)), test.input.name, engine.registry().specs()
    );
    if (test.input.hash != HashRecord::MISSING){
        checkpoint.metadata.verified = StoryVerification{
            .steps_hash = test.input.hash == HashRecord::MATCHING ? engine.steps_hash(checkpoint) : "sha256:old",
            .build = "abc", .date = "2026-11-10", .runs = "3/3",
        };
    }
    RunState state;
    const StoryEngineReport report = engine.run_checkpoint(checkpoint, state);
    if (report.save.mode != test.expectation.mode){
        return "expected save mode " + story_save_mode_name(test.expectation.mode) + ", got " + story_save_mode_name(report.save.mode);
    }
    if (report.verdict != test.expectation.verdict){
        return std::string("expected the checkpoint ") + (test.expectation.verdict == CheckpointVerdict::PASSED ? "passed" : "failed");
    }
    return check_engine_lines(test, backend);
}

class StoryEngineSaveTest : public UnitTest{
public:
    explicit StoryEngineSaveTest(EngineCase test)
        : UnitTest("Pokemon::StoryEngine save - " + std::string(test.input.name))
        , m_case(std::move(test))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope& scope) const override{
        const std::string error = check_engine(m_case, scope);
        return error.empty() ? UnitTestResult(true) : UnitTestResult(error);
    }
private:
    EngineCase m_case;
};



//  Two checkpoints' events, hashed with the built-in actions.
struct HashInput{
    std::string_view name;
    std::string_view a;
    std::string_view b;
};

//  Whether they hash the same, and `a`'s hash when pinned: a change to it
//  means every recorded verification goes stale.
struct HashExpectation{
    bool same = true;
    std::optional<std::string_view> hash_a = std::nullopt;
};

struct HashCase{
    HashInput input;
    HashExpectation expectation;
};

inline constexpr std::string_view ROUTE_B3F = R"([{"id": "go", "kind": "route", "args": {"target": {"map": "RocketHideout_B3F", "tiles": [[12, 13]]}}}])";

const HashCase HASH_CASES[] = {
    {.input = {.name = "pinned", .a = ROUTE_B3F, .b = ROUTE_B3F},
     .expectation = {.same = true, .hash_a = "sha256:PIN"}},
    {.input = {.name = "why is not hashed", .a = R"([{"id": "t", "kind": "talk", "why": "a"}])", .b = R"([{"id": "t", "kind": "talk", "why": "b"}])"},
     .expectation = {.same = true}},
    {.input = {.name = "fallbacks are filled", .a = R"([{"id": "f", "kind": "battle"}])", .b = R"([{"id": "f", "kind": "battle", "args": {"may_lose": false}}])"},
     .expectation = {.same = true}},
    {.input = {.name = "default action is named", .a = ROUTE_B3F,
               .b = R"([{"id": "go", "kind": "route", "action": "route", "args": {"target": {"map": "RocketHideout_B3F", "tiles": [[12, 13]]}}}])"},
     .expectation = {.same = true}},
    {.input = {.name = "another target differs", .a = ROUTE_B3F,
               .b = R"([{"id": "go", "kind": "route", "args": {"target": {"map": "RocketHideout_B3F", "tiles": [[12, 14]]}}}])"},
     .expectation = {.same = false}},
    {.input = {.name = "another step name differs", .a = R"([{"id": "t", "kind": "talk"}])", .b = R"([{"id": "u", "kind": "talk"}])"},
     .expectation = {.same = false}},
};

std::string hash_of(std::string_view events, std::string_view source){
    const std::string json = std::string(R"({"id": "x", "title": "x", "events": )") + std::string(events) + "}";
    return story_steps_hash(read_story_checkpoint(parse_json(json), source), builtin_story_actions());
}

std::string check_hash(const HashCase& test){
    const std::string a = hash_of(test.input.a, test.input.name);
    const std::string b = hash_of(test.input.b, test.input.name);
    if ((a == b) != test.expectation.same){
        return std::string("expected the hashes ") + (test.expectation.same ? "equal" : "different") + ": " + a + ", " + b;
    }
    if (test.expectation.hash_a && a != *test.expectation.hash_a){
        return "expected the pinned hash " + std::string(*test.expectation.hash_a) + ", got " + a;
    }
    return "";
}

class StoryStepsHashTest : public UnitTest{
public:
    explicit StoryStepsHashTest(HashCase test)
        : UnitTest("Pokemon::StoryEngine steps_hash - " + std::string(test.input.name))
        , m_case(std::move(test))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::string error = check_hash(m_case);
        return error.empty() ? UnitTestResult(true) : UnitTestResult(error);
    }
private:
    HashCase m_case;
};



//  A checkpoint's "metadata", read.
struct MetadataInput{
    std::string_view name;
    std::string_view metadata;
};

//  The error the read throws (contained), or what it read.
struct MetadataExpectation{
    std::string_view error = {};
    StoryPhase phase = StoryPhase::TESTING;
    std::string_view steps_hash = {};
    size_t items = 0;
};

struct MetadataCase{
    MetadataInput input;
    MetadataExpectation expectation;
};

const MetadataCase METADATA_CASES[] = {
    {.input = {.name = "none is testing", .metadata = ""},
     .expectation = {.phase = StoryPhase::TESTING}},
    {.input = {.name = "verified", .metadata = R"(, "metadata": {"phase": "verification", "verified": {"steps_hash": "sha256:ab", "build": "1", "date": "d", "runs": "3/3"}})"},
     .expectation = {.phase = StoryPhase::VERIFICATION, .steps_hash = "sha256:ab"}},
    {.input = {.name = "postconditions", .metadata = R"(, "metadata": {"phase": "reproduction", "postconditions": {"items": [{"item": "LIFT_KEY", "quantity": 1}]}})"},
     .expectation = {.phase = StoryPhase::REPRODUCTION, .items = 1}},
    {.input = {.name = "unknown phase", .metadata = R"(, "metadata": {"phase": "tested"})"},
     .expectation = {.error = "unknown phase tested"}},
    {.input = {.name = "unknown field", .metadata = R"(, "metadata": {"phase": "testing", "runs": 3})"},
     .expectation = {.error = "unsupported field runs"}},
};

std::string check_metadata(const MetadataCase& test){
    const std::string json = std::string(R"({"id": "x", "title": "x", "events": [])") + std::string(test.input.metadata) + "}";
    StoryCheckpoint checkpoint;
    try{
        checkpoint = read_story_checkpoint(parse_json(json), "metadata");
    }catch (const ParseException& error){
        const bool expected = !test.expectation.error.empty() && error.message().find(test.expectation.error) != std::string::npos;
        return expected ? "" : "unexpected error: " + error.message();
    }
    if (!test.expectation.error.empty()){
        return "expected the error \"" + std::string(test.expectation.error) + "\"";
    }
    const std::string hash = checkpoint.metadata.verified ? checkpoint.metadata.verified->steps_hash : "";
    if (checkpoint.metadata.phase != test.expectation.phase || hash != test.expectation.steps_hash){
        return "read phase " + story_phase_name(checkpoint.metadata.phase) + ", steps_hash \"" + hash + "\"";
    }
    if (checkpoint.postconditions.items.size() != test.expectation.items){
        return "read " + std::to_string(checkpoint.postconditions.items.size()) + " items";
    }
    return "";
}

class StoryMetadataTest : public UnitTest{
public:
    explicit StoryMetadataTest(MetadataCase test)
        : UnitTest("Pokemon::StoryEngine metadata - " + std::string(test.input.name))
        , m_case(std::move(test))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::string error = check_metadata(m_case);
        return error.empty() ? UnitTestResult(true) : UnitTestResult(error);
    }
private:
    MetadataCase m_case;
};



//  What a look showed, classified by ScreenMachine states that match on
//  observations.
struct ScreenInput{
    std::string_view name;
    std::vector<ScreenEvent> events;
};

//  The state matched; none when no state did.
struct ScreenExpectation{
    std::optional<std::string_view> state = std::nullopt;
};

struct ScreenCase{
    ScreenInput input;
    ScreenExpectation expectation;
};

const ScreenCase SCREEN_CASES[] = {
    {.input = {.name = "battle first", .events = {ScreenEvent::BATTLE, ScreenEvent::TEXT_BOX}}, .expectation = {.state = "battle"}},
    {.input = {.name = "text", .events = {ScreenEvent::TEXT_BOX}}, .expectation = {.state = "text"}},
    {.input = {.name = "overworld", .events = {ScreenEvent::CAN_MOVE}}, .expectation = {.state = "overworld"}},
    {.input = {.name = "image-only state skipped", .events = {ScreenEvent::FADE_WHITE}}, .expectation = {}},
};

ScreenState observation_state(std::string name, ScreenEvent event){
    ScreenState state;
    state.name = std::move(name);
    state.matches_observation = [event](const ScreenObservation& observation){
        return observation.events.has(event);
    };
    state.max_visits = 10;
    return state;
}

std::string check_screen(const ScreenCase& test){
    std::vector<ScreenState> states{
        observation_state("battle", ScreenEvent::BATTLE),
        observation_state("text", ScreenEvent::TEXT_BOX),
        observation_state("overworld", ScreenEvent::CAN_MOVE),
    };
    ScreenState image_only;
    image_only.name = "image_only";
    image_only.matches = [](const ImageViewRGB32&){
        return true;
    };
    image_only.max_visits = 10;
    states.push_back(std::move(image_only));
    ScreenObservation observation;
    for (ScreenEvent event : test.input.events){
        observation.events.add(event);
    }
    const ScreenState* state = match_screen_state(states, observation);
    const std::string got = state ? state->name : "none";
    return got == test.expectation.state.value_or("none") ? "" : "matched " + got;
}

class ScreenObservationTest : public UnitTest{
public:
    explicit ScreenObservationTest(ScreenCase test)
        : UnitTest("Pokemon::StoryEngine screen observation - " + std::string(test.input.name))
        , m_case(std::move(test))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::string error = check_screen(m_case);
        return error.empty() ? UnitTestResult(true) : UnitTestResult(error);
    }
private:
    ScreenCase m_case;
};


} // namespace


void add_tests_StoryEnginePolicy(UnitTestDatabase& database){
    for (const EngineCase& test : ENGINE_CASES){
        database.add<StoryEngineSaveTest>(test);
    }
    for (const HashCase& test : HASH_CASES){
        database.add<StoryStepsHashTest>(test);
    }
    for (const MetadataCase& test : METADATA_CASES){
        database.add<StoryMetadataTest>(test);
    }
    for (const ScreenCase& test : SCREEN_CASES){
        database.add<ScreenObservationTest>(test);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
