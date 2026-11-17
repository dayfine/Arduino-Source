/*  PokemonLZA AutoStory - Scene step golden Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Golden sequences: a scene step's screens as a console run showed them,
 *  played back through the step itself (the built-in plan's step, with its
 *  args), with the key the step must press on each screen. Every press is
 *  checked against the screen it was pressed on, not counted.
 *
 *  The screens come by time, as on the console: a recorded screen shows
 *  from its time until the next one's, and the clock moves with the step's
 *  presses (hold + release) and waits; the last screen stays. A screen is
 *  its events and readings by their names in the scene machine's log
 *  ("text box, advance arrow"; golden_look()), plus a choice's rows when
 *  they read ("2 rows", "row 0": CHOICE_ROWS_READING, the highlighted
 *  CHOICE_SELECTED_READING).
 *
 *  Each case's keys are its step's screen -> key table: what the step
 *  presses on each screen it meets, and why. One table for all steps, with
 *  the differences as per-step data, is the design these record:
 *
 *      screen                  key     per-step override
 *      text, with the arrow    B       text_key: A (advance_with_a,
 *      text drawing            B         story_battle_start, the map talk
 *                                        before its card); B in a step
 *                                        that ends in control, where A on
 *                                        a closing box restarts the talk
 *      choice                  A       Up first while a lower row is
 *                                        highlighted; end_on: choice
 *      tutorial card           A       (only A turns its pages)
 *      menu (nothing reads)    -       menu_key: B every look (mash), every
 *                                        2 s (the Pokedex), once per gap
 *                                        (gap_press)
 *      map                     B       (the map talk's: A pins)
 *      A prompt                -       end_on: prompt held 1 s past min_length
 *      fade                    -       end_on: the Nth black fade settled
 *      quiet                   -       end_on: quiet (quiet_is_control)
 *      battle HUD              -       end_on: battle
 *      party bar, hint         -       end: held 1 s
 *
 */

#include <algorithm>
#include <cstdint>
#include <format>
#include <optional>
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

using Pokemon::Observation;
using Pokemon::ZonePose;


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

//  The look of `screen` at `at`: its events and readings by their names in
//  the scene machine's log (describe_scene_look()), and a choice's rows as
//  "2 rows" and the highlighted "row 0".
Observation<ZonePose> golden_look(Milliseconds at, std::string_view screen){
    const std::pair<std::string_view, ScreenEvent> events[] = {
        {"black fade", ScreenEvent::FADE_BLACK}, {"white fade", ScreenEvent::FADE_WHITE},
        {"text box", ScreenEvent::DIALOG_BOX}, {"choice box", ScreenEvent::CHOICE_BOX},
        {"battle", ScreenEvent::BATTLE_STARTED}, {"party bar", ScreenEvent::PLAYER_CAN_MOVE},
        {"A prompt", ScreenEvent::INTERACT_PROMPT}, {"still", ScreenEvent::SCREEN_STILL},
    };
    const std::pair<std::string_view, std::string_view> readings[] = {
        {"controls hint", CONTROLS_HINT_READING}, {"advance arrow", ADVANCE_ARROW_READING},
        {"mission banner", MISSION_BANNER_READING}, {"mission complete", MISSION_COMPLETE_READING},
        {"tutorial card", TUTORIAL_CARD_READING},
    };
    Observation<ZonePose> look;
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

//  One recorded screen: from when it showed, and what showed.
struct GoldenScreen{
    Milliseconds at;
    std::string_view screen;
};

//  The screens of `text`, one per line: "5.9s: text box, advance arrow".
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

//  The console as a recording: observe() shows the screen whose time has
//  come by the clock, which the step's presses and waits move; each press
//  is put down to the screen the step last looked at.
//  code-health: allow public_methods -- the backend calls a step makes, plus the presses and lines a test reads
class GoldenBackend : public PlzaBackend{
public:
    struct Press{
        size_t screen;
        std::string key;
    };

    GoldenBackend(std::vector<GoldenScreen> screens, CancellableScope& scope)
        : m_screens(std::move(screens))
        , m_scope(scope)
    {}

    const std::vector<Press>& presses() const{
        return m_presses;
    }
    const std::vector<std::string>& lines() const{
        return m_lines;
    }

    virtual Pokemon::StoryBackendKind kind() const override{
        return Pokemon::StoryBackendKind::SCRIPT;
    }
    virtual void press(Button button, Milliseconds hold, Milliseconds release) override{
        m_presses.push_back({m_shown, button_to_string(button)});
        m_now += hold + release;
    }
    virtual void tap(Pokemon::Direction) override{
        m_presses.push_back({m_shown, "tap"});
    }
    virtual void hold(Pokemon::Direction, Milliseconds duration, Pokemon::StoryPace) override{
        m_presses.push_back({m_shown, "hold"});
        m_now += duration;
    }
    virtual void stick(double, Milliseconds duration, Milliseconds release, Pokemon::StoryPace) override{
        m_presses.push_back({m_shown, "stick"});
        m_now += duration + release;
    }
    virtual void wait(Milliseconds duration) override{
        m_now += duration;
    }
    virtual Observation<ZonePose> observe() override{
        while (m_shown + 1 < m_screens.size() && m_screens[m_shown + 1].at <= m_now){
            m_shown++;
        }
        if (m_screens.empty()){
            return golden_look(m_now, "nothing");
        }
        return golden_look(m_now, m_screens[m_shown].screen);
    }
    virtual Pokemon::Outcome battle() override{
        return Pokemon::outcome_ok();
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
    CancellableScope& m_scope;
    Milliseconds m_now = 0ms;
    size_t m_shown = 0;
    std::vector<Press> m_presses;
    std::vector<std::string> m_lines;
};

//  What a step presses on one screen: "A", "B", "Up", "-" for nothing, or
//  "- or B" where it depends on time (a blind press every 2 s).
struct ScreenKey{
    std::string_view screen;
    std::string_view key;
};

struct GoldenCase{
    std::string name;
    struct Input{
        //  The built-in checkpoint and its step the screens played.
        std::string_view checkpoint;
        std::string_view step;
        //  golden_screens()' lines.
        std::string_view screens;
    };
    Input input;
    struct Expectation{
        //  The key on every screen of the input.
        std::vector<ScreenKey> keys;
        //  "done", or "failed: <failure>".
        std::string_view verdict;
        //  The step's last log line.
        std::string_view end;
    };
    Expectation expectation;
};

//  01.02 pancham_cornered, French, passed (console B 2026-11-16 22:45).
//  From 35 s on, the text and the screen between its lines took turns.
constexpr std::string_view PANCHAM_FR = R"(
0.0s: still
0.8s: nothing
5.9s: text box
8.3s: text box, advance arrow
8.7s: nothing
10.8s: text box
16.0s: nothing
16.6s: text box, advance arrow
17.0s: nothing
19.5s: text box
19.9s: text box, advance arrow
20.3s: text box
21.5s: nothing
22.0s: text box
24.5s: text box, advance arrow
24.9s: nothing
27.4s: text box
28.6s: nothing
30.7s: text box, advance arrow
31.1s: nothing
33.6s: text box
34.8s: text box, advance arrow
35.2s: nothing
35.7s: text box, advance arrow
36.1s: nothing
36.6s: text box, advance arrow
37.0s: nothing
37.9s: text box, advance arrow
38.3s: nothing
38.7s: text box, advance arrow
39.1s: nothing
39.6s: text box, advance arrow
40.0s: nothing
40.4s: text box, advance arrow
40.8s: nothing
41.2s: text box, advance arrow
41.6s: nothing
42.2s: text box, advance arrow
42.6s: nothing
43.0s: text box, advance arrow
43.4s: nothing
44.4s: text box, advance arrow
44.8s: nothing
45.2s: text box, advance arrow
45.6s: nothing
46.5s: text box
48.2s: nothing
48.7s: text box
49.1s: nothing
49.5s: text box
49.9s: nothing
50.3s: text box
50.7s: text box, advance arrow
51.1s: nothing
51.6s: text box
53.3s: nothing
55.3s: text box
55.7s: nothing
56.1s: text box
56.5s: text box, advance arrow
56.9s: text box
58.5s: text box, choice box
)";

//  03.01 lobby_scene up to Alton's first choice (console B 2026-11-17
//  00:12, German): B for the text then, one in flight as the choice
//  opened. The run went on to loop on his choice; here the screen stays
//  quiet after it, as once he has his answer.
constexpr std::string_view LOBBY_ALTON_DE = R"(
0.0s: nothing
0.5s: black fade
6.7s: still
7.4s: nothing
13.0s: text box, advance arrow
13.4s: nothing
14.5s: text box, advance arrow
14.9s: nothing
16.9s: text box
19.0s: text box, advance arrow
19.4s: text box
19.9s: text box, choice box, 2 rows, row 0
20.3s: nothing
)";

const GoldenCase GOLDEN_CASES[] = {
    {
        .name = "03.11 leave_mable, a bare scene through Mable's menus (passed 2026-11-16 21:40)",
        .input = {.checkpoint = "03.11", .step = "leave_mable", .screens = R"(
0.0s: text box, still
0.4s: nothing
1.6s: text box
2.0s: nothing
2.9s: black fade
5.9s: still
6.3s: text box
9.2s: nothing
9.6s: text box
11.7s: text box, advance arrow
12.1s: text box
19.4s: nothing
19.8s: black fade
22.9s: still
23.3s: party bar
)"},
        .expectation = {
            .keys = {
                //  B turns a line; on a box that is closing it does nothing.
                {"text box", "B"}, {"text box, still", "B"}, {"text box, advance arrow", "B"},
                //  Her mission and reward menus: nothing reads them, and
                //  only B closes them, so B on every look.
                {"nothing", "B"}, {"still", "B"},
                {"black fade", "-"},
                //  Control once it holds a second.
                {"party bar", "-"},
            },
            .verdict = "done", .end = "[PLZA scene] done on can_move.",
        },
    },
    {
        .name = "03.04 pokedex_app, the nurse and the Pokedex app (passed 2026-11-16 03:51)",
        .input = {.checkpoint = "03.04", .step = "pokedex_app", .screens = R"(
0.0s: still
0.4s: text box
0.8s: text box, advance arrow
1.2s: text box
7.3s: nothing
7.7s: text box
10.6s: text box, advance arrow
11.4s: nothing
21.2s: text box, advance arrow
21.6s: nothing
25.0s: text box
28.3s: nothing
32.1s: white fade
33.4s: nothing
36.3s: text box
36.7s: nothing
38.2s: text box
38.6s: nothing
45.4s: tutorial card
46.2s: still, tutorial card
46.6s: nothing
50.0s: text box
50.8s: text box, advance arrow
51.2s: text box
54.0s: nothing
55.5s: black fade
57.0s: still
57.4s: nothing
57.9s: text box
59.5s: text box, advance arrow
59.9s: text box
61.2s: nothing
64.5s: black fade
66.8s: still
67.2s: party bar
)"},
        .expectation = {
            .keys = {
                {"text box", "B"}, {"text box, advance arrow", "B"},
                //  The Pokedex screens: nothing reads them, and only B
                //  closes them (A plays the entry's motions): B every 2 s.
                {"nothing", "- or B"}, {"still", "- or B"},
                //  Only A turns a card's pages.
                {"tutorial card", "A"}, {"still, tutorial card", "A"},
                {"white fade", "-"}, {"black fade", "-"},
                {"party bar", "-"},
            },
            .verdict = "done", .end = "[PLZA talk_to_pokedex_screen] done on can_move.",
        },
    },
    {
        .name = "01.03 andi_battle_start, Andi's talk to the battle HUD (passed 2026-11-16 22:51)",
        .input = {.checkpoint = "01.03", .step = "andi_battle_start", .screens = R"(
0.0s: text box, still, advance arrow
0.4s: nothing
2.4s: text box
3.6s: text box, advance arrow
4.1s: nothing
5.6s: text box
6.8s: nothing
7.3s: text box
9.0s: text box, advance arrow
9.4s: nothing
10.4s: text box
12.9s: nothing
16.9s: text box
18.6s: text box, advance arrow
19.5s: black fade
25.6s: still
26.6s: still, advance arrow
27.0s: advance arrow
27.8s: nothing
38.5s: battle, controls hint
)"},
        .expectation = {
            .keys = {
                //  A through the talk: it ends in the battle, with no A
                //  prompt for an A to restart.
                {"text box", "A"}, {"text box, still, advance arrow", "A"}, {"text box, advance arrow", "A"},
                //  The VS splash's subtitles show only the chevron.
                {"still, advance arrow", "A"}, {"advance arrow", "A"},
                {"nothing", "-"}, {"still", "-"}, {"black fade", "-"},
                //  The end, before anything else.
                {"battle, controls hint", "-"},
            },
            .verdict = "done", .end = "[PLZA story_battle_start] done on battle.",
        },
    },
    {
        .name = "02.05 floor_question, the elevator's talk with A to its choice (passed 2026-11-16 23:50)",
        .input = {.checkpoint = "02.05", .step = "floor_question", .screens = R"(
0.0s: black fade, still
1.3s: still
2.1s: text box, still, advance arrow
2.5s: nothing
3.0s: text box, advance arrow
3.4s: nothing
3.9s: text box, choice box
)"},
        .expectation = {
            .keys = {
                //  advance_with_a: B closes the elevator's talk.
                {"text box, still, advance arrow", "A"}, {"text box, advance arrow", "A"},
                {"black fade, still", "-"}, {"still", "-"}, {"nothing", "-"},
                //  The next step answers it.
                {"text box, choice box", "-"},
            },
            .verdict = "done", .end = "[PLZA text_to_choice] done on choice.",
        },
    },
    {
        .name = "03.01 floor_question, the elevator's talk with A to its choice (passed 2026-11-17 00:28)",
        .input = {.checkpoint = "03.01", .step = "floor_question", .screens = R"(
0.0s: text box, still, advance arrow
0.6s: text box, advance arrow
1.1s: text box, choice box
)"},
        .expectation = {
            .keys = {
                {"text box, still, advance arrow", "A"}, {"text box, advance arrow", "A"},
                {"text box, choice box", "-"},
            },
            .verdict = "done", .end = "[PLZA text_to_choice] done on choice.",
        },
    },
    {
        .name = "01.02 pancham_cornered, French, B to Pancham's choice (passed 2026-11-16 22:45)",
        .input = {.checkpoint = "01.02", .step = "pancham_cornered", .screens = PANCHAM_FR},
        .expectation = {
            .keys = {
                //  B: an A as the text ends could land on the choice.
                {"text box", "B"}, {"text box, advance arrow", "B"},
                {"still", "-"}, {"nothing", "-"},
                {"text box, choice box", "-"},
            },
            .verdict = "done", .end = "[PLZA text_to_choice] done on choice.",
        },
    },
    {
        .name = "01.02 battle_area, French, to Ryon's A prompt (passed 2026-11-16 22:46)",
        .input = {.checkpoint = "01.02", .step = "battle_area", .screens = R"(
0.0s: text box
2.0s: nothing
2.6s: text box
5.1s: text box, advance arrow
5.5s: text box
6.7s: nothing
8.2s: black fade
13.8s: still
14.4s: nothing
14.9s: A prompt
)"},
        .expectation = {
            .keys = {
                //  B: an A on the closing box restarts the talk.
                {"text box", "B"}, {"text box, advance arrow", "B"},
                {"nothing", "-"}, {"still", "-"}, {"black fade", "-"},
                //  Control, held a second past min_length_ms: no A, which
                //  restarts Ryon's talk.
                {"A prompt", "-"},
            },
            .verdict = "done", .end = "[PLZA mash_to_move_hints] done on talk_prompt.",
        },
    },
    {
        .name = "03.05 heal_party, the nurse's talk to control (passed 2026-11-16 06:59)",
        .input = {.checkpoint = "03.05", .step = "heal_party", .screens = R"(
0.0s: still
0.3s: text box, still
0.7s: text box
2.3s: nothing
2.8s: text box
4.2s: party bar
4.7s: party bar, A prompt
)"},
        .expectation = {
            .keys = {
                //  B: an A on her closing box restarts her talk.
                {"text box", "B"}, {"text box, still", "B"},
                {"still", "-"}, {"nothing", "-"},
                {"party bar", "-"}, {"party bar, A prompt", "-"},
            },
            .verdict = "done", .end = "[PLZA scene] done on can_move.",
        },
    },
    {
        .name = "03.01 lobby_scene, German, A on the text as Alton's choice opens (B in flight picked his cancel row, 2026-11-17 00:12)",
        .input = {.checkpoint = "03.01", .step = "lobby_scene", .screens = LOBBY_ALTON_DE},
        .expectation = {
            .keys = {
                //  advance_with_a: a B that lands as the choice opens
                //  answers it with its last row before any look sees it;
                //  an A there takes the top row.
                {"text box", "A"}, {"text box, advance arrow", "A"},
                //  The top row is highlighted: A.
                {"text box, choice box, 2 rows, row 0", "A"},
                {"nothing", "-"}, {"still", "-"}, {"black fade", "-"},
            },
            .verdict = "done", .end = "[PLZA mash_to_move_hints] done: quiet, taken as control.",
        },
    },
};

//  The built-in checkpoint `id` with only its step `step`.
StoryCheckpoint step_row(std::string_view id, std::string_view step){
    for (const StoryCheckpoint& row : builtin_story_data().checkpoints){
        if (row.id != id){
            continue;
        }
        StoryCheckpoint part = row;
        std::erase_if(part.steps, [step](const Pokemon::StoryStep& candidate){
            return candidate.name != step;
        });
        return part;
    }
    return {};
}

//  The key `keys` gives `screen`; none when it has no row for it.
std::optional<std::string_view> key_of(const std::vector<ScreenKey>& keys, std::string_view screen){
    for (const ScreenKey& row : keys){
        if (row.screen == screen){
            return row.key;
        }
    }
    return std::nullopt;
}

//  Whether `pressed` is one of `expected`'s keys ("- or B").
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

//  The presses `backend` made against `test`'s keys: each press must be its
//  screen's key, and a screen with a key that shows a second or more in all
//  must get it at least once (a shorter one can pass between two looks).
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
    const StoryCheckpoint row = step_row(test.input.checkpoint, test.input.step);
    if (row.steps.size() != 1){
        return std::format("no step {} in {}", test.input.step, test.input.checkpoint);
    }
    AutosaveSightings sightings;
    const Pokemon::StoryGame<ZonePose> game = make_plza_story_game_without_console(builtin_story_data(), sightings);
    GoldenBackend backend(screens, scope);
    Pokemon::StoryEngine<ZonePose> engine(game, backend);
    Pokemon::RunState state;
    const Pokemon::SegmentReport report = engine.run_segment("test", std::vector<StoryCheckpoint>{row}, 1, state);
    std::vector<std::string> wrong = wrong_presses(test, screens, backend);
    const std::string verdict = report.completed ? "done" : std::format("failed: {}", report.failure.failure);
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
