/*  PokemonLZA AutoStory - Battle on screen Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Steps of the built-in 03.02b on a FakeStoryBackend: a battle HUD seen
 *  before its story battle step ends the step under way, and the run goes on
 *  at that battle.
 *
 */

#include <cstddef>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryEngine.h"
#include "Pokemon/AutoStory/Testing/Pokemon_StoryFakeBackend.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Actions/PokemonLZA_AutoStoryActionTable.h"
#include "PokemonLZA_AutoStoryAutosaves.h"
#include "PokemonLZA_AutoStoryGame.h"
#include "PokemonLZA_AutoStoryStoryFiles.h"
#include "PokemonLZA_AutoStoryBattleOnScreen_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

using Pokemon::StoryCheckpoint;


ScreenObservation look_at(ScreenEvents events, Milliseconds at){
    ScreenObservation observation;
    observation.when = WallClock::min() + at;
    observation.events = events;
    return observation;
}

ScreenObservation battle_hud(Milliseconds at){
    return look_at({ScreenEvent::BATTLE_STARTED}, at);
}


struct BattleJumpCase{
    std::string name;
    struct Input{
        //  Of 03.02b, 0-based.
        size_t first_step;
        size_t steps;
        std::vector<ScreenObservation> looks;
    };
    Input input;
    struct Expectation{
        std::string inputs;
        std::string line;
    };
    Expectation expectation;
};

const BattleJumpCase BATTLE_JUMP_CASES[] = {
    {
        //  Console B 2026-11-19 22:10: the plaza scene ran on into Taunie's battle,
        //  its HUD on every other look; A on it attacked for 30 s, then the walk to
        //  her found no talk. The fade takes the scene's first A.
        .name = "a flickering battle HUD in the plaza scene jumps to the first story battle",
        .input = {.first_step = 1, .steps = 4, .looks = {
            look_at({ScreenEvent::FADE_BLACK}, Milliseconds(0)), battle_hud(Milliseconds(400)),
            look_at({}, Milliseconds(800)), battle_hud(Milliseconds(1200)),
        }},
        .expectation = {
            .inputs = "press A, press A, press A",
            .line = "[Event] 03.02b: battle on screen during plaza_scene: jumping to battle_totodile, skipping to_taunie, taunie_battle_start",
        },
    },
    {
        .name = "a battle HUD at the walk to Taunie jumps to the story battle with no walk",
        .input = {.first_step = 2, .steps = 3, .looks = {
            battle_hud(Milliseconds(0)), look_at({ScreenEvent::PLAYER_CAN_MOVE}, Milliseconds(400)), battle_hud(Milliseconds(800)),
        }},
        .expectation = {
            .inputs = "wait 250ms, wait 250ms",
            .line = "[Event] 03.02b: battle on screen during to_taunie: jumping to battle_totodile, skipping taunie_battle_start",
        },
    },
};

std::string joined(const std::vector<std::string>& texts, std::string_view separator){
    std::string text;
    for (const std::string& part : texts){
        text += (text.empty() ? "" : std::string(separator)) + part;
    }
    return text;
}

//  03.02b's steps [first_step, first_step + steps).
std::vector<StoryCheckpoint> rows_of(PlzaConsoleHooks& hooks, const BattleJumpCase::Input& input){
    std::vector<StoryCheckpoint> rows;
    for (StoryCheckpoint& row : load_plza_story(hooks).checkpoints){
        if (row.id == "03.02b"){
            row.steps.erase(row.steps.begin(), row.steps.begin() + (std::ptrdiff_t)input.first_step);
            row.steps.resize(input.steps);
            rows.push_back(std::move(row));
        }
    }
    return rows;
}

//  The battle code passes at once, so a jump to it ends the run done.
UnitTestResult check_battle_jump(const BattleJumpCase& test, CancellableScope& scope){
    AutosaveSightings sightings;
    PlzaConsoleHooks hooks{.sightings = &sightings};
    hooks.battle = [](PlzaBackend&) -> std::optional<Pokemon::StoryFailure>{
        return std::nullopt;
    };
    Pokemon::StoryGame game = plza_story_game(builtin_story_data(), sightings);
    Pokemon::FakeStoryBackend backend({.looks = test.input.looks}, scope);
    Pokemon::RunState state;
    std::vector<StoryCheckpoint> rows = rows_of(hooks, test.input);
    note_autosaves(rows, sightings);
    const std::optional<Pokemon::CheckpointFailure> failure = Pokemon::run_checkpoints(backend, rows, game, state);
    const std::string log = joined(backend.lines(), "\n");
    if (failure){
        return std::format("expected done, got failed: {}; log:\n{}", failure->failure, log);
    }
    const std::string inputs = joined(backend.inputs(), ", ");
    if (inputs != test.expectation.inputs){
        return std::format("expected inputs \"{}\", got \"{}\"", test.expectation.inputs, inputs);
    }
    for (const std::string& line : backend.lines()){
        if (line == test.expectation.line){
            return true;
        }
    }
    return std::format("expected the line \"{}\" in:\n{}", test.expectation.line, log);
}


class Test_BattleJump : public UnitTest{
public:
    explicit Test_BattleJump(const BattleJumpCase& test)
        : UnitTest(std::format("PokemonLZA::AutoStoryGame battle on screen - {}", test.name))
        , m_test(test)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope& scope) const override{
        return check_battle_jump(m_test, scope);
    }

private:
    const BattleJumpCase& m_test;
};

} // namespace


void add_tests_AutoStoryBattleOnScreen(UnitTestDatabase& database){
    for (const BattleJumpCase& test : BATTLE_JUMP_CASES){
        database.add<Test_BattleJump>(test);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
