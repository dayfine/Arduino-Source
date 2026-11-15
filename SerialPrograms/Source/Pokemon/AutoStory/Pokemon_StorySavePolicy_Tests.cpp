/*  Story Save Policy Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The engine's save policy (Engine/Pokemon_StoryEnginePolicy.h): the
 *  steps hash and the save mode a checkpoint's phase and recorded hash
 *  give. Then the postcondition check (Pokemon_StoryPostconditionCheck.h)
 *  on a checkpoint's end state.
 *
 */

#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Json/JsonValue.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Data/Pokemon_StoryStepJson.h"
#include "Engine/Pokemon_StoryEnginePolicy.h"
#include "Pokemon_StoryPostconditionCheck.h"
#include "Pokemon_StorySavePolicy_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{


struct HashCase{
    //  Two checkpoints' events, hashed with the built-in actions.
    struct Input{
        std::string_view a;
        std::string_view b;
    };

    //  Whether they hash the same, and `a`'s hash when pinned: a change to it
    //  means every recorded verification goes stale.
    struct Expectation{
        bool same = true;
        std::optional<std::string_view> hash_a = std::nullopt;
    };

    std::string_view name;
    Input input;
    Expectation expectation;
};

inline constexpr std::string_view ROUTE_B3F = R"([{"id": "go", "kind": "route", "args": {"target": {"map": "RocketHideout_B3F", "tiles": [[12, 13]]}}}])";

const HashCase HASH_CASES[] = {
    {.name = "pinned", .input = {.a = ROUTE_B3F, .b = ROUTE_B3F},
     .expectation = {.same = true, .hash_a = "sha256:13bc877c960a15cd0ed2f37248fa8336308b048a865623dde7654d41ac7147b8"}},
    {.name = "whitespace is not hashed", .input = {.a = R"([{"id": "t", "kind": "talk"}])", .b = R"([ { "id":"t",  "kind":"talk" } ])"},
     .expectation = {.same = true}},
    {.name = "why differs", .input = {.a = R"([{"id": "t", "kind": "talk", "why": "a"}])", .b = R"([{"id": "t", "kind": "talk", "why": "b"}])"},
     .expectation = {.same = false}},
    {.name = "written fallback differs", .input = {.a = R"([{"id": "f", "kind": "battle"}])", .b = R"([{"id": "f", "kind": "battle", "args": {"may_lose": false}}])"},
     .expectation = {.same = false}},
    {.name = "another target differs", .input = { .a = ROUTE_B3F,
               .b = R"([{"id": "go", "kind": "route", "args": {"target": {"map": "RocketHideout_B3F", "tiles": [[12, 14]]}}}])"},
     .expectation = {.same = false}},
    {.name = "another step name differs", .input = {.a = R"([{"id": "t", "kind": "talk"}])", .b = R"([{"id": "u", "kind": "talk"}])"},
     .expectation = {.same = false}},
};

std::string hash_of(std::string_view events, std::string_view source){
    const std::string json = std::format(R"({{"id": "x", "title": "x", "events": {}}})", events);
    return story_steps_hash(read_story_checkpoint(parse_json(json), source));
}

std::string check_hash(const HashCase& test){
    const std::string a = hash_of(test.input.a, test.name);
    const std::string b = hash_of(test.input.b, test.name);
    if ((a == b) != test.expectation.same){
        return std::format("{}{}: {}, {}", "expected the hashes ", (test.expectation.same ? "equal" : "different"), a, b);
    }
    if (test.expectation.hash_a && a != *test.expectation.hash_a){
        return std::format("expected the pinned hash {}, got {}", *test.expectation.hash_a, a);
    }
    return "";
}

class Test_StepsHash : public UnitTest{
public:
    explicit Test_StepsHash(HashCase test)
        : UnitTest(std::format("Pokemon::StorySavePolicy - steps hash: {}", test.name))
        , m_case(std::move(test))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::string error = check_hash(m_case);
        return error.empty() ? UnitTestResult(true) : UnitTestResult(error);
    }

private:
    HashCase m_case;
};


//  The save mode of a run of checkpoint "00.31" in `phase`, verified with
//  `recorded` (none: never verified), whose steps now hash to "sha256:now".
struct SaveDecisionCase{
    std::string_view name;
    struct Input{
        StoryPhase phase;
        std::optional<std::string_view> recorded;
    };

    struct Expectation{
        StorySaveMode mode;
        std::string_view reason;
    };

    Input input;
    Expectation expectation;
};

const SaveDecisionCase SAVE_DECISION_CASES[] = {
    {.name = "testing never saves", .input = {.phase = StoryPhase::TESTING, .recorded = "sha256:now"},
     .expectation = {.mode = StorySaveMode::NONE, .reason = "[Save] 00.31: phase testing: save mode none"}},
    {.name = "verification saves after its runs", .input = {.phase = StoryPhase::VERIFICATION, .recorded = "sha256:now"},
     .expectation = {.mode = StorySaveMode::AFTER_VERIFICATION, .reason = "[Save] 00.31: phase verification: save mode after_verification"}},
    {.name = "reproduction saves each checkpoint", .input = {.phase = StoryPhase::REPRODUCTION, .recorded = "sha256:now"},
     .expectation = {.mode = StorySaveMode::PER_CHECKPOINT, .reason = "[Save] 00.31: phase reproduction: save mode per_checkpoint"}},
    {.name = "changed steps are testing", .input = {.phase = StoryPhase::REPRODUCTION, .recorded = "sha256:old"},
     .expectation = {.mode = StorySaveMode::NONE,
                     .reason = "[Save] 00.31: phase reproduction but steps_hash mismatch (recorded sha256:old, now sha256:now): treated as testing, save mode none"}},
    {.name = "never verified is testing", .input = {.phase = StoryPhase::VERIFICATION, .recorded = std::nullopt},
     .expectation = {.mode = StorySaveMode::NONE,
                     .reason = "[Save] 00.31: phase verification but never verified: treated as testing, save mode none"}},
};

class Test_SaveDecision : public UnitTest{
public:
    explicit Test_SaveDecision(SaveDecisionCase test)
        : UnitTest(std::format("Pokemon::StorySavePolicy - save decision: {}", test.name))
        , m_case(std::move(test))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        StoryCheckpoint checkpoint;
        checkpoint.id = "00.31";
        checkpoint.metadata.phase = m_case.input.phase;
        if (m_case.input.recorded){
            checkpoint.metadata.verified = StoryVerification{.steps_hash = std::string(*m_case.input.recorded)};
        }
        const StorySaveDecision decision = decide_story_save(checkpoint, "sha256:now");
        if (decision.mode != m_case.expectation.mode || decision.reason != m_case.expectation.reason){
            return std::format("got {}: {}", story_save_mode_name(decision.mode), decision.reason);
        }
        return true;
    }

private:
    SaveDecisionCase m_case;
};


//  A checkpoint that ends beside B3F's Rare Candy, with the LIFT KEY and
//  both Rare Candies.
inline constexpr std::string_view CHECKPOINT_0031 = R"({
    "id": "00.31",
    "title": "Rocket Hideout to the LIFT KEY",
    "start": {"map": "CeladonCity_PokemonCenter_1F", "tiles": [[7, 4]], "facing": "north"},
    "events": [
        {"id": "beside_b3f_rare_candy", "kind": "route", "args": {"target": {"map": "RocketHideout_B3F", "tiles": [[12, 13]], "facing": "north"}}},
        {"id": "b3f_rare_candy", "kind": "talk"}
    ],
    "metadata": {"phase": "testing", "postconditions": {
        "position": {"map": "RocketHideout_B3F", "tiles": [[12, 13]], "facing": "north"},
        "items": [{"item": "LIFT_KEY", "quantity": 1}, {"item": "RARE_CANDY", "quantity": 2}]
    }}
})";

StoryCheckpoint checkpoint_0031(){
    return read_story_checkpoint(parse_json(std::string(CHECKPOINT_0031)), "00.31");
}

ObservedState read_0031(std::string map, int candies){
    ObservedState observed;
    observed.map = std::move(map);
    observed.tile = TilePos{12, 13};
    observed.facing = Direction::NORTH;
    observed.items = {{"LIFT_KEY", 1}, {"RARE_CANDY", (size_t)candies}};
    return observed;
}

UnitTestResult test_postconditions_json(){
    const StoryCheckpoint checkpoint = checkpoint_0031();
    const StoryPostconditions& post = checkpoint.postconditions;
    if (!post.position || post.position->map != "RocketHideout_B3F" || post.items.size() != 2
        || post.items[1] != StoryItem{.item = "RARE_CANDY", .quantity = 2}
    ){
        return "Unexpected postconditions read.";
    }
    if (post.flags.size() != 0 || post.position->tiles != std::vector<TilePos>{TilePos{12, 13}}){
        return "Unexpected postcondition position or flags read.";
    }
    return true;
}

UnitTestResult test_postconditions_pass(){
    const PostconditionReport report = check_postconditions(checkpoint_0031(), read_0031("RocketHideout_B3F", 2));
    const std::vector<std::string> lines = postcondition_log_lines(report);
    if (!report.all_passed() || lines.back() != "[Postcondition] 00.31: 5 passed, 0 failed, 0 not checked: passed"){
        return std::format("Expected every postcondition passed: {}", lines.back());
    }
    return true;
}

//  A run that reports success while still on B4F, the Rare Candy not
//  picked up.
UnitTestResult test_postconditions_fail(){
    const PostconditionReport report = check_postconditions(checkpoint_0031(), read_0031("RocketHideout_B4F", 1));
    const std::vector<std::string> lines = postcondition_log_lines(report);
    const std::vector<std::string> expected = {
        "[Postcondition] 00.31 map: failed: expected RocketHideout_B3F, read RocketHideout_B4F",
        "[Postcondition] 00.31 tile: not checked: on another map",
        "[Postcondition] 00.31 facing: not checked: on another map",
        "[Postcondition] 00.31 item LIFT_KEY: passed: 1",
        "[Postcondition] 00.31 item RARE_CANDY: failed: expected 2, read 1",
        "[Postcondition] 00.31: 1 passed, 2 failed, 2 not checked: failed",
    };
    if (lines != expected){
        std::string text;
        for (const std::string& line : lines){
            text += std::format("\n{}", line);
        }
        return std::format("Unexpected lines:{}", text);
    }
    return true;
}

UnitTestResult test_postconditions_not_checked(){
    StoryCheckpoint checkpoint = checkpoint_0031();
    checkpoint.postconditions.flags = {"hideout_lift_key_grunt"};
    ObservedState observed;
    observed.map = "RocketHideout_B3F";
    const PostconditionReport report = check_postconditions(checkpoint, observed);
    const std::vector<std::string> lines = postcondition_log_lines(report);
    if (report.all_passed() || report.count(PostconditionVerdict::NOT_CHECKED) != 5
        || lines.back() != "[Postcondition] 00.31: 1 passed, 0 failed, 5 not checked: incomplete"
    ){
        return std::format("What isn't read is not checked, never passed: {}", lines.back());
    }
    observed.flags["hideout_lift_key_grunt"] = false;
    if (check_postconditions(checkpoint, observed).count(PostconditionVerdict::FAILED) != 1){
        return "A flag read clear should fail.";
    }
    return true;
}

UnitTestResult test_end_pose_from_the_plan(){
    StoryCheckpoint checkpoint = checkpoint_0031();
    checkpoint.postconditions.position.reset();
    const std::optional<GridPose> end = checkpoint_end_pose(checkpoint);
    if (!end || end->map != "RocketHideout_B3F" || end->tiles != std::vector<TilePos>{TilePos{12, 13}}){
        return "Expected the last route's target as the end.";
    }
    StoryCheckpoint stepped = checkpoint;
    StoryStep step;
    step.kind = StoryStepKind::STEP;
    step.name = "one more";
    stepped.steps.push_back(std::move(step));
    if (checkpoint_end_pose(stepped)){
        return "A step after the last target leaves the end unknown.";
    }
    StoryStep legs;
    legs.kind = StoryStepKind::LEGS;
    legs.name = "taps";
    checkpoint.steps.push_back(std::move(legs));
    if (checkpoint_end_pose(checkpoint)){
        return "Legs after the last target leave the end unknown.";
    }
    if (!check_postconditions(checkpoint, read_0031("RocketHideout_B3F", 2)).all_passed()){
        return "Without an end pose, the items alone should be checked.";
    }
    return true;
}


//  A named test; it returns its own failure.
struct SavePolicyTest{
    std::string name;
    UnitTestResult (*test)();
};

class Test_StorySavePolicy : public UnitTest{
public:
    explicit Test_StorySavePolicy(SavePolicyTest test)
        : UnitTest(std::format("Pokemon::StorySavePolicy - {}", test.name))
        , m_test(std::move(test))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return m_test.test();
    }

private:
    SavePolicyTest m_test;
};

const SavePolicyTest SAVE_POLICY_TESTS[] = {
    {.name = "postconditions: json", .test = test_postconditions_json},
    {.name = "postconditions: pass", .test = test_postconditions_pass},
    {.name = "postconditions: fail", .test = test_postconditions_fail},
    {.name = "postconditions: not checked", .test = test_postconditions_not_checked},
    {.name = "postconditions: end pose from the plan", .test = test_end_pose_from_the_plan},
};

} // namespace


void add_tests_StorySavePolicy(UnitTestDatabase& database){
    for (const HashCase& test : HASH_CASES){
        database.add<Test_StepsHash>(test);
    }
    for (const SaveDecisionCase& test : SAVE_DECISION_CASES){
        database.add<Test_SaveDecision>(test);
    }
    for (const SavePolicyTest& test : SAVE_POLICY_TESTS){
        database.add<Test_StorySavePolicy>(test);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
