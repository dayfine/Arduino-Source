/*  PokemonLZA AutoStory - Scene step golden Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

/*  Golden sequences: a scene step's screens as a console run logged them,
 *  played back through the built-in plan's step by time (the clock moves
 *  with its presses and waits), with the key it must press on each screen.
 */

#include <algorithm>
#include <cstdint>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "NintendoSwitch/Controllers/NintendoSwitch_ControllerButtons.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryEngine.h"
#include "Detectors/PokemonLZA_AutoStoryTutorialCardDetector.h"
#include "PokemonLZA_AutoStoryAutosaves.h"
#include "PokemonLZA_AutoStoryBackend.h"
#include "PokemonLZA_AutoStoryGame.h"
#include "PokemonLZA_AutoStoryReadings.h"
#include "PokemonLZA_AutoStoryStoryFiles.h"
#include "PokemonLZA_AutoStorySceneGolden_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;


namespace{



std::string joined(const std::vector<std::string>& texts, std::string_view separator){
    std::string text;
    for (const std::string& part : texts){
        text += (text.empty() ? "" : std::string(separator)) + part;
    }
    return text;
}

Milliseconds seconds_in_log(std::string_view text){
    return Milliseconds((int64_t)(std::stod(std::string(text)) * 1000 + 0.5));
}

//  `screen` as describe_scene_look() names it, plus a choice's "2 rows" and
//  highlighted "row 0".
ScreenObservation golden_look(Milliseconds at, std::string_view screen){
    const std::pair<std::string_view, ScreenEvent> events[] = {
        {"black fade", ScreenEvent::FADE_BLACK}, {"white fade", ScreenEvent::FADE_WHITE},
        {"choice box", ScreenEvent::CHOICE_BOX},
        {"battle", ScreenEvent::BATTLE_STARTED}, {"party bar", ScreenEvent::PLAYER_CAN_MOVE},
        {"A prompt", ScreenEvent::INTERACT_PROMPT}, {"still", ScreenEvent::SCREEN_STILL},
    };
    const std::pair<std::string_view, std::string_view> readings[] = {
        {"controls hint", CONTROLS_HINT_READING}, {"advance arrow", ADVANCE_ARROW_READING},
        {"mission banner", MISSION_BANNER_READING}, {"mission complete", MISSION_COMPLETE_READING},
        {"tutorial card", TUTORIAL_CARD_READING},
    };
    ScreenObservation look;
    look.when = WallClock::min() + at;
    size_t from = 0;
    while (from < screen.size()){
        const size_t comma = std::min(screen.find(", ", from), screen.size());
        const std::string_view name = screen.substr(from, comma - from);
        from = comma + 2;
        for (const auto& [event_name, event] : events){
            if (name == event_name){
                look.events.set(event, true);
            }
        }
        for (const auto& [reading_name, reading] : readings){
            if (name == reading_name){
                look.readings[std::string(reading)] = 1;
            }
        }
        if (name.ends_with(" rows")){
            look.readings[std::string(CHOICE_ROWS_READING)] = std::stoi(std::string(name));
        }else if (name.starts_with("row ")){
            look.readings[std::string(CHOICE_SELECTED_READING)] = std::stoi(std::string(name.substr(4)));
        }
    }
    return look;
}

using Pokemon::StoryCheckpoint;

struct GoldenScreen{
    Milliseconds at;
    std::string_view screen;
};

//  One per line: "5.9s: party bar, advance arrow".
std::vector<GoldenScreen> golden_screens(std::string_view text){
    std::vector<GoldenScreen> screens;
    size_t from = 0;
    while (from < text.size()){
        const size_t end = std::min(text.find('\n', from), text.size());
        const std::string_view line = text.substr(from, end - from);
        from = end + 1;
        const size_t colon = line.find("s: ");
        if (colon == std::string_view::npos){
            continue;
        }
        screens.push_back({seconds_in_log(line.substr(0, colon)), line.substr(colon + 3)});
    }
    return screens;
}

//  The console as a recording; each press is put down to the screen the
//  step last looked at.
//  code-health: allow public_methods -- the backend calls a step makes, plus the presses and lines a test reads
class GoldenBackend : public PlzaBackend{
public:
    struct Press{
        size_t screen;
        std::string key;
    };

    GoldenBackend(std::vector<GoldenScreen> screens, std::optional<size_t> symbols_from_watch, CancellableScope& scope)
        : m_screens(std::move(screens))
        , m_symbols_from_watch(symbols_from_watch)
        , m_scope(scope)
    {}

    //  Logged as a ZL press.
    bool watch_lock_on(Milliseconds hold){
        m_presses.push_back({m_shown, "ZL"});
        m_now += hold;
        m_watches++;
        return m_symbols_from_watch && m_watches >= *m_symbols_from_watch;
    }

    const std::vector<Press>& presses() const{
        return m_presses;
    }
    const std::vector<std::string>& lines() const{
        return m_lines;
    }

    virtual void press(Button button, Milliseconds hold, Milliseconds release) override{
        m_presses.push_back({m_shown, button_to_string(button)});
        m_now += hold + release;
    }
    virtual void stick(double, Milliseconds duration, Milliseconds release, Pokemon::StoryPace) override{
        m_presses.push_back({m_shown, "stick"});
        m_now += duration + release;
    }
    virtual void wait(Milliseconds duration) override{
        m_now += duration;
    }
    virtual ScreenObservation observe() override{
        while (m_shown + 1 < m_screens.size() && m_screens[m_shown + 1].at <= m_now){
            m_shown++;
        }
        if (m_screens.empty()){
            return golden_look(m_now, "nothing");
        }
        return golden_look(m_now, m_screens[m_shown].screen);
    }
    virtual void log(std::string_view line) override{
        m_lines.emplace_back(line);
    }
    virtual void log_detail(std::string_view line) override{
        m_lines.emplace_back(line);
    }
    virtual CancellableScope& scope() override{
        return m_scope;
    }

private:
    std::vector<GoldenScreen> m_screens;
    std::optional<size_t> m_symbols_from_watch;
    CancellableScope& m_scope;
    Milliseconds m_now = 0ms;
    size_t m_shown = 0;
    size_t m_watches = 0;
    std::vector<Press> m_presses;
    std::vector<std::string> m_lines;
};

//  "A", "B", "Up", "-" for nothing, or "- or B" where it depends on time.
struct ScreenKey{
    std::string_view screen;
    std::string_view key;
};

struct GoldenCase{
    std::string name;
    struct Input{
        std::string_view checkpoint;
        //  Empty: the whole checkpoint.
        std::string_view step;
        std::string_view screens;
        //  The first lock-on watch that sees the move symbols; none: never.
        std::optional<size_t> symbols_from_watch = std::nullopt;
    };
    Input input;
    struct Expectation{
        std::vector<ScreenKey> keys;
        //  "done", or "failed: <failure>".
        std::string_view verdict;
        //  The step's last log line.
        std::string_view end;
    };
    Expectation expectation;
};

const GoldenCase GOLDEN_CASES[] = {
    {
        .name = "03.06 wild_zones_tutorial, the HUD 45 s and no scene: not at its trigger (failed 2026-11-16 21:03)",
        .input = {.checkpoint = "03.06", .step = "wild_zones_tutorial", .screens = R"(
0.0s: party bar, still, controls hint
0.3s: party bar, controls hint
28.2s: party bar, still, controls hint
28.5s: party bar, controls hint
)"},
        .expectation = {
            .keys = {{"party bar, still, controls hint", "-"}, {"party bar, controls hint", "-"}},
            .verdict = "failed: scene_never_started",
            .end = "[Event] 03.06/1 wild_zones_tutorial: failed: scene_never_started: scene: the scene never started (player not at its "
                "trigger?): the HUD up and no text, choice, battle or cutscene for 45000 ms (expecting the controls hint, the party bar, "
                "the A prompt or the mission banner).",
        },
    },
    {
        .name = "03.09 research_lab_gate, the party bar and an A prompt 45 s and no scene (failed 2026-11-16 21:34)",
        .input = {.checkpoint = "03.09", .step = "research_lab_gate", .screens = R"(
0.0s: party bar, A prompt
)"},
        .expectation = {
            .keys = {{"party bar, A prompt", "-"}},
            .verdict = "failed: scene_never_started",
            .end = "[Event] 03.09/1 research_lab_gate: failed: scene_never_started: scene: the scene never started (player not at its "
                "trigger?): the HUD up and no text, choice, battle or cutscene for 45000 ms (expecting the controls hint, the party bar, "
                "the A prompt or the mission banner).",
        },
    },
    {
        .name = "03.04 into_pokemon_center, a timed walk whose last turn faces the nurse's A prompt (passed 2026-11-17 14:26)",
        .input = {.checkpoint = "03.04", .step = "into_pokemon_center", .screens = R"(
0.0s: party bar
9.5s: party bar, A prompt
)"},
        .expectation = {
            .keys = {{"party bar", "L or stick or hold"}, {"party bar, A prompt", "-"}},
            .verdict = "done", .end = "[PLZA] walk: the end event after the last turn.",
        },
    },
    {
        //  Chained from 03.03: the legs ran along a wall, never still.
        .name = "03.04 into_pokemon_center, no A prompt after the last turn (failed 2026-11-17 02:34)",
        .input = {.checkpoint = "03.04", .step = "into_pokemon_center", .screens = R"(
0.0s: party bar
)"},
        .expectation = {
            .keys = {{"party bar", "L or stick or hold"}},
            .verdict = "failed: walk_no_end",
            .end = "[Event] 03.04/1 into_pokemon_center: failed: walk_no_end: walk: no end event after the last turn",
        },
    },
    {
        //  The turn into the Center ran into the wall beside its opening; steps
        //  sideways from there walked back out (console B 2026-11-19 05:42).
        .name = "03.04 into_pokemon_center, no A prompt after the last turn, one shows later",
        .input = {.checkpoint = "03.04", .step = "into_pokemon_center", .screens = R"(
0.0s: party bar
12.0s: party bar, A prompt
)"},
        .expectation = {
            .keys = {{"party bar", "L or stick or hold"}, {"party bar, A prompt", "-"}},
            .verdict = "failed: walk_no_end",
            .end = "[Event] 03.04/1 into_pokemon_center: failed: walk_no_end: walk: no end event after the last turn",
        },
    },
    {
        .name = "03.06 out_of_pokemon_center, a timed walk, no end event (passed 2026-11-17 14:27)",
        .input = {.checkpoint = "03.06", .step = "out_of_pokemon_center", .screens = R"(
0.0s: party bar
)"},
        .expectation = {
            .keys = {{"party bar", "L or stick or hold"}},
            .verdict = "done", .end = "[PLZA] walk: legs done, screen now: player can move",
        },
    },
    {
        .name = "03.11 to_mable, a walk to her dialog that a black fade ends (passed 2026-11-17 14:36)",
        .input = {.checkpoint = "03.11", .step = "to_mable", .screens = R"(
0.0s: party bar
5.6s: black fade
)"},
        .expectation = {
            .keys = {{"party bar", "L or stick or hold"}, {"black fade", "-"}},
            .verdict = "done", .end = "[PLZA] walk: a black fade after 1250 ms of the last leg: the scene takes over.",
        },
    },
    {
        .name = "03.02b to_taunie, straight on until her A prompt holds (passed 2026-11-16 03:47)",
        .input = {.checkpoint = "03.02b", .step = "to_taunie", .screens = R"(
0.0s: party bar
2.6s: party bar, A prompt
)"},
        .expectation = {
            .keys = {{"party bar", "stick"}, {"party bar, A prompt", "-"}},
            .verdict = "done", .end = "[PLZA] walk: the end event after 2500 ms of the last leg.",
        },
    },
    {
        .name = "01.03 andi_battle, the battle code on the battle HUD (passed 2026-11-16 22:51)",
        .input = {.checkpoint = "01.03", .step = "andi_battle", .screens = R"(
0.0s: battle, controls hint
)"},
        .expectation = {
            .keys = {{"battle, controls hint", "-"}},
            .verdict = "done", .end = "[Event] 01.03/1 andi_battle: done",
        },
    },
    {
        .name = "03.02b battle_tepig, the start locked on, the HUD only while ZL is held: ZL, the move symbols, the battle code",
        .input = {.checkpoint = "03.02b", .step = "battle_tepig", .screens = R"(
0.0s: party bar
)", .symbols_from_watch = 1},
        .expectation = {
            .keys = {{"party bar", "ZL"}},
            .verdict = "done", .end = "[Event] 03.02b/1 battle_tepig: done",
        },
    },
    {
        .name = "03.02b battle_tepig, no battle HUD and no move symbols under ZL in 10 s: battle_never_started",
        .input = {.checkpoint = "03.02b", .step = "battle_tepig", .screens = R"(
0.0s: party bar
)"},
        .expectation = {
            .keys = {{"party bar", "ZL"}},
            .verdict = "failed: battle_never_started",
            .end = "[Event] 03.02b/1 battle_tepig: failed: battle_never_started: no battle HUD and no move symbols under ZL "
                "within 10 s",
        },
    },
    {
        //  The controls hint after the cutscene is synthetic.
        .name = "02.02 street_scene, English, a textless cutscene past the start window, then the controls hint (failed as scene_never_started 2026-11-15 23:23)",
        .input = {.checkpoint = "02.02", .step = "street_scene", .screens = R"(
0.0s: black fade, still
3.8s: still
4.9s: nothing
46.0s: controls hint
)"},
        .expectation = {
            .keys = {{"black fade, still", "-"}, {"still", "- or A or +"}, {"nothing", "- or A or +"}, {"controls hint", "-"}},
            .verdict = "done", .end = "[PLZA scene] done on controls_hint.",
        },
    },
};

//  All of checkpoint `id` with an empty `step`, bound to `tools`.
StoryCheckpoint step_row(std::string_view id, std::string_view step, PlzaConsoleHooks& tools){
    for (StoryCheckpoint& row : load_plza_story(tools).checkpoints){
        if (row.id != id){
            continue;
        }
        StoryCheckpoint part = std::move(row);
        if (step.empty()){
            return part;
        }
        std::erase_if(part.steps, [step](const Pokemon::StoryStep& candidate){
            return candidate.name != step;
        });
        return part;
    }
    return {};
}

std::optional<std::string_view> key_of(const std::vector<ScreenKey>& keys, std::string_view screen){
    for (const ScreenKey& row : keys){
        if (row.screen == screen){
            return row.key;
        }
    }
    return std::nullopt;
}

bool key_allowed(std::string_view expected, std::string_view pressed){
    size_t from = 0;
    while (from <= expected.size()){
        const size_t end = std::min(expected.find(" or ", from), expected.size());
        if (expected.substr(from, end - from) == pressed){
            return true;
        }
        from = end + 4;
    }
    return false;
}

//  A screen shown under a second in all can pass between two looks, so
//  needs no press.
std::vector<std::string> wrong_presses(
    const GoldenCase& test, const std::vector<GoldenScreen>& screens, const GoldenBackend& backend
){
    std::vector<std::string> wrong;
    for (const GoldenBackend::Press& press : backend.presses()){
        const GoldenScreen& screen = screens[press.screen];
        const std::string_view expected = key_of(test.expectation.keys, screen.screen).value_or("none");
        if (!key_allowed(expected, press.key)){
            wrong.push_back(std::format("{} on \"{}\" (at {} ms), expected {}", press.key, screen.screen, screen.at.count(), expected));
        }
    }
    for (const ScreenKey& row_key : test.expectation.keys){
        if (row_key.key == "-" || row_key.key.starts_with("- or")){
            continue;
        }
        Milliseconds shown = 0ms;
        for (size_t index = 0; index + 1 < screens.size(); index++){
            shown += screens[index].screen == row_key.screen ? screens[index + 1].at - screens[index].at : 0ms;
        }
        const bool pressed = std::ranges::any_of(backend.presses(), [&](const GoldenBackend::Press& press){
            return screens[press.screen].screen == row_key.screen;
        });
        if (shown >= 1000ms && !pressed){
            wrong.push_back(std::format("no press on \"{}\", expected {}", row_key.screen, row_key.key));
        }
    }
    return wrong;
}

UnitTestResult check_golden(const GoldenCase& test, CancellableScope& scope){
    const std::vector<GoldenScreen> screens = golden_screens(test.input.screens);
    for (const GoldenScreen& screen : screens){
        if (!key_of(test.expectation.keys, screen.screen)){
            return std::format("no key for the screen \"{}\"", screen.screen);
        }
    }
    GoldenBackend backend(screens, test.input.symbols_from_watch, scope);
    PlzaConsoleHooks tools;
    tools.lock_on = [&backend](Milliseconds hold){
        return backend.watch_lock_on(hold);
    };
    tools.battle = [](PlzaBackend&) -> std::optional<Pokemon::StoryFailure>{
        return std::nullopt;
    };
    StoryCheckpoint row = step_row(test.input.checkpoint, test.input.step, tools);
    if (row.steps.empty() || (!test.input.step.empty() && row.steps.size() != 1)){
        return std::format("no step {} in {}", test.input.step, test.input.checkpoint);
    }
    AutosaveSightings sightings;
    Pokemon::StoryGame game = plza_story_game(builtin_story_data(), sightings);
    Pokemon::RunState state;
    const std::optional<Pokemon::CheckpointFailure> failure = Pokemon::run_checkpoints(backend, std::span<StoryCheckpoint>(&row, 1), game, state);
    std::vector<std::string> wrong = wrong_presses(test, screens, backend);
    const std::string verdict = !failure ? "done" : std::format("failed: {}", failure->failure.code);
    if (verdict != test.expectation.verdict){
        wrong.push_back(std::format("ended \"{}\", expected \"{}\"", verdict, test.expectation.verdict));
    }else if (std::find(backend.lines().begin(), backend.lines().end(), test.expectation.end) == backend.lines().end()){
        wrong.push_back(std::format("no line \"{}\"", test.expectation.end));
    }
    if (wrong.empty()){
        return true;
    }
    return std::format("{}\nlog:\n{}", joined(wrong, "\n"), joined(backend.lines(), "\n"));
}

class GoldenTest : public UnitTest{
public:
    explicit GoldenTest(const GoldenCase& test)
        : UnitTest(std::format("PokemonLZA::AutoStorySceneMachine golden - {}", test.name))
        , m_test(test)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope& scope) const override{
        return check_golden(m_test, scope);
    }
private:
    const GoldenCase& m_test;
};

} // namespace


void add_tests_AutoStorySceneGolden(UnitTestDatabase& database){
    for (const GoldenCase& test : GOLDEN_CASES){
        database.add<GoldenTest>(test);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
