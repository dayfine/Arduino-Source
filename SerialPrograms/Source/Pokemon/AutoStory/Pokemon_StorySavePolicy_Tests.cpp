/*  Story Save Policy Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The postcondition check (Pokemon_StoryPostconditionCheck.h) on a
 *  checkpoint's end state.
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
#include "Pokemon_StoryPostconditionCheck.h"
#include "Pokemon_StorySavePolicy_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{


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
    for (const SavePolicyTest& test : SAVE_POLICY_TESTS){
        database.add<Test_StorySavePolicy>(test);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
