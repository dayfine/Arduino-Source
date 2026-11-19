/*  PokemonLZA AutoStory - Scene step golden Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */
//  code-health: allow file_lines -- one golden table for the scene steps, sharing the recorded screens above it

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
        {"text box", ScreenEvent::DIALOG_BOX}, {"choice box", ScreenEvent::CHOICE_BOX},
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

//  One per line: "5.9s: text box, advance arrow".
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

//  03.02b tepig_battle_start (console B 2026-11-17 23:25): "We're not done
//  just yet!", the Type Matchups card, the battle's fade, the HUD.
constexpr std::string_view TEPIG_START = R"(
0.0s: text box, advance arrow
0.4s: text box
0.8s: nothing
1.3s: tutorial card
1.7s: still, tutorial card
2.5s: tutorial card
2.9s: nothing
3.4s: black fade
4.5s: still
4.7s: battle, still
)";

//  The same start four minutes before (console B 2026-11-17 23:21), the
//  camera turned away by the Totodile battle's lock-on recovery: after the
//  fade, the battle's effects with no HUD for 22 s, then the party bar.
constexpr std::string_view TEPIG_START_OFF_CAMERA = R"(
0.0s: text box, advance arrow
0.4s: nothing
0.9s: tutorial card
1.7s: still, tutorial card
2.1s: tutorial card
2.6s: nothing
3.1s: black fade
4.1s: nothing
26.6s: party bar
)";

//  A battle start whose fade a cutscene follows (the screen moving, no
//  party bar), as Totodile's start (console B 2026-11-17 23:20) got blind
//  ZL and turns ~4 s after its fade; the party bar once it settles.
constexpr std::string_view BATTLE_START_CUTSCENE = R"(
0.0s: text box, advance arrow
0.4s: black fade
1.4s: nothing
20.0s: party bar
)";

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
                {"text box", "B"}, {"text box, still", "B"}, {"text box, advance arrow", "B"},
                //  Her mission and reward menus: only B closes them.
                {"nothing", "B"}, {"still", "B"},
                {"black fade", "-"},
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
                //  The Pokedex screens: only B closes them.
                {"nothing", "- or B"}, {"still", "- or B"},
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
                //  A: the talk ends in the battle, with no prompt to restart.
                {"text box", "A"}, {"text box, still, advance arrow", "A"}, {"text box, advance arrow", "A"},
                //  The VS splash's subtitles show only the chevron.
                {"still, advance arrow", "A"}, {"advance arrow", "A"},
                {"nothing", "- or +"}, {"still", "-"}, {"black fade", "-"},
                {"battle, controls hint", "-"},
            },
            .verdict = "done", .end = "[PLZA story_battle_start] done on battle.",
        },
    },
    {
        .name = "03.02b tepig_battle_start, the battle HUD right after the battle's fade (passed 2026-11-17 23:25)",
        .input = {.checkpoint = "03.02b", .step = "tepig_battle_start", .screens = TEPIG_START},
        .expectation = {
            .keys = {
                {"text box, advance arrow", "A"}, {"text box", "A"},
                {"tutorial card", "A"}, {"still, tutorial card", "A"},
                //  No lock-on: the HUD shows within 3 s of the fade.
                {"nothing", "-"}, {"black fade", "-"}, {"still", "-"},
                {"battle, still", "-"},
            },
            .verdict = "done", .end = "[PLZA story_battle_start] done on battle.",
        },
    },
    {
        //  The battle ran off camera, with no HUD until locked on.
        .name = "03.02b tepig_battle_start, the party bar a second after the battle's fade: ZL, the move symbols lock on (failed 2026-11-17 23:21)",
        .input = {.checkpoint = "03.02b", .step = "tepig_battle_start", .screens = TEPIG_START_OFF_CAMERA, .symbols_from_watch = 1},
        .expectation = {
            .keys = {
                {"text box, advance arrow", "A"},
                {"tutorial card", "A"}, {"still, tutorial card", "A"},
                {"nothing", "-"}, {"black fade", "-"},
                {"party bar", "ZL"},
            },
            .verdict = "done", .end = "[PLZA story_battle_start] done on locked_on.",
        },
    },
    {
        .name = "03.02b tepig_battle_start, the opponent behind the camera: no move symbols, a turn, L and ZL again",
        .input = {
            .checkpoint = "03.02b", .step = "tepig_battle_start", .screens = TEPIG_START_OFF_CAMERA, .symbols_from_watch = 2,
        },
        .expectation = {
            .keys = {
                {"text box, advance arrow", "A"},
                {"tutorial card", "A"}, {"still, tutorial card", "A"},
                {"nothing", "-"}, {"black fade", "-"},
                {"party bar", "ZL or stick or L"},
            },
            .verdict = "done", .end = "[PLZA story_battle_start] lock-on 2/5: turned 90 degrees right, L, ZL held: the move symbols showed.",
        },
    },
    {
        .name = "03.02b taunie_battle_start, a cutscene after the battle's fade: no ZL until it settles",
        .input = {.checkpoint = "03.02b", .step = "taunie_battle_start", .screens = BATTLE_START_CUTSCENE, .symbols_from_watch = 1},
        .expectation = {
            .keys = {
                {"text box, advance arrow", "A"}, {"black fade", "-"},
                {"nothing", "-"},
                {"party bar", "ZL"},
            },
            .verdict = "done", .end = "[PLZA story_battle_start] done on locked_on.",
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
                //  B would close the elevator's talk.
                {"text box, still, advance arrow", "A"}, {"text box, advance arrow", "A"},
                {"black fade, still", "-"}, {"still", "-"}, {"nothing", "-"},
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
                {"still", "-"}, {"nothing", "- or +"},
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
                //  No A: it restarts Ryon's talk.
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
                //  A B landing as the choice opens would take its last row.
                {"text box", "A"}, {"text box, advance arrow", "A"},
                {"text box, choice box, 2 rows, row 0", "A"},
                {"nothing", "- or +"}, {"still", "-"}, {"black fade", "-"},
            },
            .verdict = "done", .end = "[PLZA mash_to_move_hints] done: quiet, taken as control.",
        },
    },
    {
        .name = "03.06 wild_zones_tutorial, French, B through the text and A on the card to control (passed 2026-11-17 14:28)",
        .input = {.checkpoint = "03.06", .step = "wild_zones_tutorial", .screens = R"(
0.0s: still
0.3s: black fade, still
1.0s: still
2.1s: text box, still
2.5s: text box
2.9s: text box, advance arrow
3.3s: text box
3.7s: nothing
5.7s: text box
7.0s: text box, advance arrow
7.4s: text box
9.0s: nothing
11.0s: text box
11.8s: text box, advance arrow
14.9s: tutorial card
15.3s: still, tutorial card
16.1s: nothing
16.6s: text box
18.3s: text box, advance arrow
18.7s: nothing
19.2s: black fade
21.5s: still
22.0s: party bar, still
22.2s: party bar
)"},
        .expectation = {
            .keys = {
                {"text box", "B"}, {"text box, still", "B"}, {"text box, advance arrow", "B"},
                {"tutorial card", "A"}, {"still, tutorial card", "A"},
                {"nothing", "-"}, {"still", "-"}, {"black fade", "-"}, {"black fade, still", "-"},
                {"party bar", "-"}, {"party bar, still", "-"},
            },
            .verdict = "done", .end = "[PLZA scene] done on can_move.",
        },
    },
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
        .name = "03.07 after_catches, French, A on the subtitles' chevron, B on the text box, to control (passed 2026-11-17 14:31)",
        .input = {.checkpoint = "03.07", .step = "after_catches", .screens = R"(
0.0s: text box, advance arrow
0.4s: nothing
1.9s: advance arrow
2.4s: nothing
2.9s: text box
3.7s: text box, advance arrow
4.1s: nothing
4.6s: text box
10.1s: black fade
12.2s: still
12.5s: nothing
13.0s: party bar, A prompt, controls hint
)"},
        .expectation = {
            .keys = {
                //  B: an A on the closing box restarts the talk.
                {"text box", "B"}, {"text box, advance arrow", "B"},
                //  A subtitle with no box.
                {"advance arrow", "A"},
                {"nothing", "-"}, {"still", "-"}, {"black fade", "-"},
                {"party bar, A prompt, controls hint", "-"},
            },
            .verdict = "done", .end = "[PLZA mash_to_move_hints] done on can_move.",
        },
    },
    {
        .name = "03.09 research_lab_gate, French, Ryon's scene to control (passed 2026-11-17 14:34)",
        .input = {.checkpoint = "03.09", .step = "research_lab_gate", .screens = R"(
0.0s: black fade, still
1.3s: nothing
7.4s: text box
8.6s: text box, advance arrow
9.0s: nothing
14.1s: text box
17.8s: nothing
20.8s: advance arrow
21.2s: nothing
25.3s: black fade
27.3s: still
27.8s: party bar, still
28.1s: party bar
)"},
        .expectation = {
            .keys = {
                {"text box", "B"}, {"text box, advance arrow", "B"}, {"advance arrow", "A"},
                {"nothing", "- or +"}, {"still", "-"}, {"black fade", "-"}, {"black fade, still", "-"},
                {"party bar", "-"}, {"party bar, still", "-"},
            },
            .verdict = "done", .end = "[PLZA scene] done on can_move.",
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
        .name = "03.10 arrive_3f, French, a bare scene off the elevator to control (passed 2026-11-17 14:35)",
        .input = {.checkpoint = "03.10", .step = "arrive_3f", .screens = R"(
0.0s: nothing
0.8s: black fade
5.4s: still
5.8s: nothing
6.6s: text box, advance arrow
7.0s: nothing
10.7s: text box
14.0s: text box, advance arrow
14.4s: text box
16.0s: nothing
17.2s: black fade
18.8s: still
19.2s: party bar
)"},
        .expectation = {
            .keys = {
                {"text box", "B"}, {"text box, advance arrow", "B"},
                {"nothing", "B or +"}, {"still", "B"},
                {"black fade", "-"}, {"party bar", "-"},
            },
            .verdict = "done", .end = "[PLZA scene] done on can_move.",
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
        .name = "03.05 back_to_nurse, a walk held until the A prompt holds (passed 2026-11-17 14:27)",
        .input = {.checkpoint = "03.05", .step = "back_to_nurse", .screens = R"(
0.0s: party bar
2.6s: party bar, A prompt
)"},
        .expectation = {
            .keys = {{"party bar", "L or stick or hold"}, {"party bar, A prompt", "-"}},
            .verdict = "done", .end = "[PLZA] walk: ended on: party bar, A prompt",
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
        .name = "03.05 talk_to_nurse, A at her A prompt, then her text box (passed 2026-11-17 14:27)",
        .input = {.checkpoint = "03.05", .step = "talk_to_nurse", .screens = R"(
0.0s: party bar, A prompt
0.5s: text box
)"},
        .expectation = {
            .keys = {{"party bar, A prompt", "A"}, {"text box", "-"}},
            .verdict = "done", .end = "[PLZA] talk: started 250 ms after A 1: text box",
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
        //  heal_party's screens start where the talk hands over (3.5 s).
        .name = "03.05 whole checkpoint: the walk to the nurse, the talk, her heal to control (passed 2026-11-17 14:27)",
        .input = {.checkpoint = "03.05", .step = "", .screens = R"(
0.0s: party bar
2.6s: party bar, A prompt
3.5s: text box, still
3.9s: text box
4.7s: text box, advance arrow
5.1s: text box
6.0s: nothing
6.5s: text box, advance arrow
6.9s: nothing
7.9s: party bar
8.4s: party bar, A prompt
)"},
        .expectation = {
            .keys = {
                {"party bar", "L or stick or hold"}, {"party bar, A prompt", "A"},
                {"text box", "B"}, {"text box, still", "B"}, {"text box, advance arrow", "B"},
                {"nothing", "-"},
            },
            .verdict = "done", .end = "[PLZA scene] done on can_move.",
        },
    },
    {
        //  From the black fade on; the controls hint at the end is synthetic.
        .name = "01.01 arrival_scenes, Italian, Jacinthe's subtitles each after a quiet spell, then the controls hint (stopped as a loop 2026-11-15 22:27)",
        .input = {.checkpoint = "01.01", .step = "arrival_scenes", .screens = R"(
0.0s: black fade
2.4s: nothing
3.9s: still
5.6s: nothing
12.5s: advance arrow
12.9s: nothing
13.8s: text box
14.3s: nothing
24.4s: advance arrow
24.8s: nothing
34.2s: advance arrow
34.7s: nothing
39.7s: advance arrow
40.6s: controls hint
)"},
        .expectation = {
            .keys = {
                {"text box", "B"}, {"advance arrow", "A"},
                {"nothing", "- or +"}, {"still", "-"}, {"black fade", "-"}, {"controls hint", "-"},
            },
            .verdict = "done", .end = "[PLZA mash_to_move_hints] done on controls_hint.",
        },
    },
    {
        .name = "01.02 battle_area, Italian, the A Parla prompt past 20 s ends the step (A there restarted the talk 2026-11-15 21:56)",
        .input = {.checkpoint = "01.02", .step = "battle_area", .screens = R"(
0.0s: text box
1.2s: text box, advance arrow
1.7s: nothing
2.2s: text box
3.0s: text box, advance arrow
3.4s: nothing
3.7s: text box
5.6s: nothing
7.1s: black fade
12.8s: still
13.2s: nothing
13.7s: A prompt
15.7s: nothing
16.2s: text box
17.0s: text box, advance arrow
17.4s: nothing
18.4s: A prompt
22.4s: nothing
)"},
        .expectation = {
            .keys = {
                {"text box", "B"}, {"text box, advance arrow", "B"},
                {"nothing", "- or +"}, {"still", "-"}, {"black fade", "-"}, {"A prompt", "-"},
            },
            .verdict = "done", .end = "[PLZA mash_to_move_hints] done on talk_prompt.",
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
            .keys = {{"black fade, still", "-"}, {"still", "-"}, {"nothing", "- or +"}, {"controls hint", "-"}},
            .verdict = "done", .end = "[PLZA scene] done on controls_hint.",
        },
    },
};

//  All of checkpoint `id` with an empty `step`, bound to `tools`.
StoryCheckpoint step_row(std::string_view id, std::string_view step, const PlzaStoryTools& tools){
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
    PlzaStoryTools tools;
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
