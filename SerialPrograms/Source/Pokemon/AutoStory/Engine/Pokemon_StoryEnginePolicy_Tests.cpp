/*  Story Engine Policy Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The engine's whole run of a checkpoint on a scripted backend: the
 *  postconditions (passed, failed, not checked) and whether it saved: at
 *  the end of a checkpoint that passed, and at each SAVE step.
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
std::string checkpoint_0031(std::string_view steps){
    return std::format(R"({{"id": "00.31", "title": "Rocket Hideout", "events": {},
        "metadata": {{"source": "test", "postconditions": {{
            "position": {{"map": "RocketHideout_B3F", "tiles": [[12, 13]], "facing": "north"}},
            "items": [{{"item": "RARE_CANDY", "quantity": 2}}]
        }}}}}})", steps);
}

inline constexpr std::string_view TALK_STEPS = R"([{"id": "b3f_rare_candy", "kind": "talk", "why": "the second candy"}])";
inline constexpr std::string_view SAVE_STEPS = R"([{"id": "b3f_rare_candy", "kind": "talk"}, {"id": "save_here", "kind": "save"}])";

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
    //  One engine run of 00.31, the player ending on `end_map`.
    struct Input{
        std::string_view steps = TALK_STEPS;
        //  Where the talk leaves the player; empty: the talk opens no text.
        std::string_view end_map = "RocketHideout_B3F";
        std::optional<int> candies = 2;
    };

    //  The verdict, how many times the game saved (each save logs
    //  "[Test] save") and a line the run logged.
    struct Expectation{
        CheckpointVerdict verdict = CheckpointVerdict::PASSED;
        size_t saves = 0;
        std::string_view line = {};
    };

    std::string_view name;
    Input input;
    Expectation expectation;
};

const EngineCase ENGINE_CASES[] = {
    {.name = "a pass saves at the end", .input = {},
     .expectation = {.saves = 1, .line = "[Save] 00.31: saved"}},
    {.name = "B4F false pass fails postconditions", .input = {.end_map = "RocketHideout_B4F"},
     .expectation = {.verdict = CheckpointVerdict::FAILED, .saves = 0,
                     .line = "[Postcondition] 00.31 map: failed: expected RocketHideout_B3F, read RocketHideout_B4F"}},
    {.name = "unread bag is not checked", .input = {.candies = std::nullopt},
     .expectation = {.saves = 1, .line = "[Postcondition] 00.31: 3 passed, 0 failed, 1 not checked: incomplete"}},
    {.name = "wrong candy count fails", .input = {.candies = 1},
     .expectation = {.verdict = CheckpointVerdict::FAILED, .saves = 0,
                     .line = "[Postcondition] 00.31 item RARE_CANDY: failed: expected 2, read 1"}},
    {.name = "failed run never saves", .input = {.end_map = ""},
     .expectation = {.verdict = CheckpointVerdict::FAILED, .saves = 0, .line = "[Save] 00.31: run failed: not saved"}},
    {.name = "save step saves", .input = {.steps = SAVE_STEPS},
     .expectation = {.saves = 2, .line = "[Step] save_here SAVE/default: ok"}},
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
    const StoryCheckpoint checkpoint = read_story_checkpoint(
        parse_json(checkpoint_0031(test.input.steps)), test.name, engine.registry().specs()
    );
    RunState state;
    const StoryEngineReport report = engine.run_checkpoint(checkpoint, state);
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
