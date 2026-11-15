/*  Story Engine Policy Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The engine's whole run of a checkpoint on a scripted backend: the save
 *  mode from the checkpoint's phase and steps hash, the postconditions
 *  (passed, failed, not checked) and whether it saved.
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
#include "Pokemon/AutoStory/Data/Pokemon_StoryStepJson.h"
#include "Pokemon_StoryEngine.h"
#include "Pokemon_StoryFakeBackend.h"
#include "Pokemon_StoryEnginePolicy_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{


//  A checkpoint's end in the shared format, its steps given: beside B3F's
//  Rare Candy, facing north, with two Rare Candies.
std::string checkpoint_0031(std::string_view phase, std::string_view steps){
    return std::format(R"({{"id": "00.31", "title": "Rocket Hideout", "events": {},
        "metadata": {{"phase": "{}", "source": "test", "postconditions": {{
            "position": {{"map": "RocketHideout_B3F", "tiles": [[12, 13]], "facing": "north"}},
            "items": [{{"item": "RARE_CANDY", "quantity": 2}}]
        }}}}}})", steps, phase);
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
    observation.events = ScreenEvents{ScreenEvent::PLAYER_CAN_MOVE};
    observation.pose = GridPose{.map = std::move(map), .tiles = {{12, 13}}, .facing = Direction::NORTH};
    return observation;
}

//  A game whose save is recorded as an input, and whose "bag" reader
//  reads `candies` Rare Candies (no reader when unset).
StoryGame<GridPose> engine_game(std::optional<int> candies){
    StoryGame<GridPose> game;
    game.id = "test";
    game.save = [](StoryBackend<GridPose>& backend, const StoryCheckpoint&){
        backend.wait(Milliseconds(1));
        backend.log("[Test] save");
        return outcome_ok();
    };
    if (candies){
        game.readers.push_back({
            .name = "bag", .supplies = {"bag.*"}, .use = StoryReaderUse::AFTER_CHECKPOINT, .console_only = false,
            .read = [count = *candies](StoryBackend<GridPose>&, Observation<GridPose>& observation){
                observation.readings["bag.RARE_CANDY"] = count;
            },
        });
    }
    return game;
}



struct EngineCase{
    //  One engine run of 00.31 in `phase`, the player ending on `end_map`.
    struct Input{
        std::string_view phase;
        HashRecord hash = HashRecord::MATCHING;
        std::string_view steps = TALK_STEPS;
        //  Where the talk leaves the player; empty: the talk opens no text.
        std::string_view end_map = "RocketHideout_B3F";
        std::optional<int> candies = 2;
    };

    //  The save mode chosen, the verdict, whether the game saved (each save
    //  logs "[Test] save") and a line the run logged.
    struct Expectation{
        StorySaveMode mode = StorySaveMode::NONE;
        CheckpointVerdict verdict = CheckpointVerdict::PASSED;
        size_t saves = 0;
        std::string_view line = {};
    };

    std::string_view name;
    Input input;
    Expectation expectation;
};

const EngineCase ENGINE_CASES[] = {
    {.name = "testing never saves", .input = {.phase = "testing"},
     .expectation = {.mode = StorySaveMode::NONE, .saves = 0, .line = "[Save] 00.31: phase testing: save mode none"}},
    {.name = "verification saves at the end", .input = {.phase = "verification"},
     .expectation = {.mode = StorySaveMode::AFTER_VERIFICATION, .saves = 1, .line = "[Save] 00.31: saved"}},
    {.name = "reproduction saves", .input = {.phase = "reproduction"},
     .expectation = {.mode = StorySaveMode::PER_CHECKPOINT, .saves = 1, .line = "[Save] 00.31: phase reproduction: save mode per_checkpoint"}},
    {.name = "stale steps_hash is testing", .input = {.phase = "verification", .hash = HashRecord::STALE},
     .expectation = {.mode = StorySaveMode::NONE, .saves = 0, .line = "[Save] 00.31: phase verification but steps_hash mismatch (recorded sha256:old, now "}},
    {.name = "never verified is testing", .input = {.phase = "reproduction", .hash = HashRecord::MISSING},
     .expectation = {.mode = StorySaveMode::NONE, .saves = 0, .line = "[Save] 00.31: phase reproduction but never verified: treated as testing, save mode none"}},
    {.name = "B4F false pass fails postconditions", .input = {.phase = "verification", .end_map = "RocketHideout_B4F"},
     .expectation = {.mode = StorySaveMode::AFTER_VERIFICATION, .verdict = CheckpointVerdict::FAILED, .saves = 0,
                     .line = "[Postcondition] 00.31 map: failed: expected RocketHideout_B3F, read RocketHideout_B4F"}},
    {.name = "unread bag is not checked", .input = {.phase = "verification", .candies = std::nullopt},
     .expectation = {.mode = StorySaveMode::AFTER_VERIFICATION, .saves = 1,
                     .line = "[Postcondition] 00.31: 3 passed, 0 failed, 1 not checked: incomplete"}},
    {.name = "wrong candy count fails", .input = {.phase = "reproduction", .candies = 1},
     .expectation = {.mode = StorySaveMode::PER_CHECKPOINT, .verdict = CheckpointVerdict::FAILED, .saves = 0,
                     .line = "[Postcondition] 00.31 item RARE_CANDY: failed: expected 2, read 1"}},
    {.name = "failed run never saves", .input = {.phase = "reproduction", .end_map = ""},
     .expectation = {.mode = StorySaveMode::PER_CHECKPOINT, .verdict = CheckpointVerdict::FAILED, .saves = 0,
                     .line = "[Save] 00.31: run failed: not saved"}},
    {.name = "save step skipped while testing", .input = {.phase = "testing", .steps = SAVE_STEPS},
     .expectation = {.mode = StorySaveMode::NONE, .saves = 0, .line = "[Save] save step skipped: save mode none"}},
    {.name = "save step skipped while verifying", .input = {.phase = "verification", .steps = SAVE_STEPS},
     .expectation = {.mode = StorySaveMode::AFTER_VERIFICATION, .saves = 1, .line = "[Save] save step skipped: save mode after_verification"}},
    {.name = "save step saves in reproduction", .input = {.phase = "reproduction", .steps = SAVE_STEPS},
     .expectation = {.mode = StorySaveMode::PER_CHECKPOINT, .saves = 2, .line = "[Step] save_here SAVE/default: ok"}},
};

//  A text box, then the overworld on `end_map`; only the overworld when
//  `end_map` is empty.
std::vector<Observation<GridPose>> engine_looks(std::string_view end_map){
    if (end_map.empty()){
        return {end_look("RocketHideout_B3F")};
    }
    Observation<GridPose> text;
    text.events = ScreenEvents{ScreenEvent::DIALOG_BOX};
    return {std::move(text), end_look(std::string(end_map))};
}

std::string check_engine_lines(const EngineCase& test, const FakeStoryBackend<GridPose>& backend){
    size_t saves = 0;
    bool found = false;
    for (const std::string& line : backend.lines()){
        saves += line == "[Test] save" ? 1 : 0;
        found |= line.starts_with(test.expectation.line);
    }
    if (saves != test.expectation.saves){
        return std::format("expected {} saves, got {}", test.expectation.saves, saves);
    }
    return found ? "" : std::format("expected a line starting \"{}\"", test.expectation.line);
}

std::string check_engine(const EngineCase& test, CancellableScope& scope){
    const StoryGame<GridPose> game = engine_game(test.input.candies);
    FakeStoryBackend<GridPose> backend({.looks = engine_looks(test.input.end_map)}, scope);
    StoryEngine<GridPose> engine(game, backend);
    StoryCheckpoint checkpoint = read_story_checkpoint(
        parse_json(checkpoint_0031(test.input.phase, test.input.steps)), test.name, engine.registry().specs()
    );
    if (test.input.hash != HashRecord::MISSING){
        checkpoint.metadata.verified = StoryVerification{
            .steps_hash = test.input.hash == HashRecord::MATCHING ? engine.steps_hash(checkpoint) : "sha256:old",
            .build = "abc", .date = "2026-11-10", .runs = "3/3",
        };
    }
    RunState state;
    const StoryEngineReport report = engine.run_checkpoint(checkpoint, SavePoint::RUN_END, state);
    if (report.save.mode != test.expectation.mode){
        return std::format("expected save mode {}, got {}", story_save_mode_name(test.expectation.mode), story_save_mode_name(report.save.mode));
    }
    if (report.verdict != test.expectation.verdict){
        return std::format("expected the checkpoint {}", test.expectation.verdict == CheckpointVerdict::PASSED ? "passed" : "failed");
    }
    return check_engine_lines(test, backend);
}

class StoryEngineSaveTest : public UnitTest{
public:
    explicit StoryEngineSaveTest(EngineCase test)
        : UnitTest(std::format("Pokemon::StoryEngine save - {}", test.name))
        , m_case(std::move(test))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope& scope) const override{
        const std::string error = check_engine(m_case, scope);
        return error.empty() ? UnitTestResult(true) : UnitTestResult(error);
    }
private:
    EngineCase m_case;
};


} // namespace


void add_tests_StoryEnginePolicy(UnitTestDatabase& database){
    for (const EngineCase& test : ENGINE_CASES){
        database.add<StoryEngineSaveTest>(test);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
