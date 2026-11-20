/*  PokemonLZA AutoStory - Scene Golden Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
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
#include <cstddef>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "NintendoSwitch/Controllers/NintendoSwitch_ControllerButtons.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryEngine.h"
#include "PokemonLZA_AutoStoryTypes.h"
#include "PokemonLZA_AutoStoryAutosaves.h"
#include "Actions/PokemonLZA_AutoStoryActionTable.h"
#include "PokemonLZA_AutoStoryGame.h"
#include "PokemonLZA_AutoStoryPlan.h"
#include "PokemonLZA_AutoStorySceneGolden_Tests.h"
#include "Pokemon/AutoStory/Testing/Pokemon_StoryFakeBackend.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Actions/PokemonLZA_AutoStoryActionTable.h"

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
constexpr std::string_view PANCHAM_FR = R"(
0.0s: still
0.8s: nothing
5.9s: text box
8.3s: text box, advance arrow
9.5s: nothing
12.0s: text box
14.0s: text box, advance arrow
15.2s: text box
16.5s: text box, choice box
)";

//  03.01 lobby_scene up to Alton's first choice (console B 2026-11-17
//  00:12, German): B for the text then, one in flight as the choice
//  opened. The run went on to loop on his choice; here the party bar shows
//  after it, as at the end of the lobby talk on 2026-11-19 09:19.
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
40.0s: party bar
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
                {"text box", "B"}, {"text box, still", "B"}, {"text box, advance arrow", "B"}, {"nothing", "B"}, {"still", "B"},
                {"black fade", "-"}, {"party bar", "-"}
            },
            .verdict = "done", .end = "[PLZA scene] done on control.",
        },
    },
    {
        .name = "03.04 pokedex_app, the nurse and the Pokedex app (passed 2026-11-16 03:51)",
        .input = {.checkpoint = "03.04", .step = "pokedex_app", .screens = R"(
0.0s: still
1.2s: text box
2.8s: text box, advance arrow
4.0s: nothing
6.0s: white fade
7.5s: nothing
9.0s: tutorial card
10.2s: still, tutorial card
11.4s: nothing
13.0s: text box
14.5s: black fade
16.0s: still
16.4s: party bar
)"},
        .expectation = {
            .keys = {
                {"text box", "B"}, {"text box, advance arrow", "B"}, {"nothing", "B"}, {"still", "B"}, {"tutorial card", "A"},
                {"still, tutorial card", "A"}, {"white fade", "-"}, {"black fade", "-"}, {"party bar", "-"}
            },
            .verdict = "done", .end = "[PLZA talk_to_pokedex_screen] done on control.",
        },
    },
    {
        .name = "03.02b tepig_battle_start, the battle HUD right after the battle's fade (passed 2026-11-17 23:25)",
        .input = {.checkpoint = "03.02b", .step = "tepig_battle_start", .screens = TEPIG_START},
        .expectation = {
            .keys = {
                {"text box, advance arrow", "A"}, {"text box", "A"}, {"tutorial card", "A"}, {"still, tutorial card", "A"},
                {"nothing", "A"}, {"black fade", "-"}, {"still", "-"}, {"battle, still", "-"}
            },
            .verdict = "done", .end = "[PLZA scene] done on battle.",
        },
    },
    {
        //  The battle ran off camera, with no HUD until locked on.
        .name = "03.02b tepig_battle_start, the party bar a second after the battle's fade: ZL, the move symbols lock on (failed 2026-11-17 23:21)",
        .input = {.checkpoint = "03.02b", .step = "tepig_battle_start", .screens = TEPIG_START_OFF_CAMERA, .symbols_from_watch = 1},
        .expectation = {
            .keys = {
                {"text box, advance arrow", "A"}, {"tutorial card", "A"}, {"still, tutorial card", "A"}, {"nothing", "A"},
                {"black fade", "-"}, {"party bar", "ZL"}
            },
            .verdict = "done", .end = "[PLZA scene] done on locked_on.",
        },
    },
    {
        .name = "03.02b tepig_battle_start, the opponent behind the camera: no move symbols, a turn, L and ZL again",
        .input = {
            .checkpoint = "03.02b", .step = "tepig_battle_start", .screens = TEPIG_START_OFF_CAMERA, .symbols_from_watch = 2,
        },
        .expectation = {
            .keys = {
                {"text box, advance arrow", "A"}, {"tutorial card", "A"}, {"still, tutorial card", "A"}, {"nothing", "A"},
                {"black fade", "-"}, {"party bar", "ZL or stick or L"}
            },
            .verdict = "done", .end = "[PLZA scene] lock-on 2/5: turned 90 degrees right, L, ZL held: the move symbols showed.",
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
            .verdict = "done", .end = "[PLZA scene] done on locked_on.",
        },
    },
    {
        .name = "02.05 floor_question, the elevator's talk with A to its choice (passed 2026-11-16 23:50)",
        .input = {.checkpoint = "02.05", .step = "floor_question", .screens = R"(
0.0s: party bar, A prompt
0.5s: black fade, still
1.8s: still
2.6s: text box, still, advance arrow
3.0s: nothing
3.5s: text box, advance arrow
3.9s: nothing
4.4s: text box, choice box
)"},
        .expectation = {
            .keys = {{"party bar, A prompt", "A"},
                {"text box, still, advance arrow", "A"}, {"text box, advance arrow", "A"}, {"black fade, still", "-"},
                {"still", "-"}, {"nothing", "-"}, {"text box, choice box", "-"}},
            .verdict = "done", .end = "[PLZA scene] done on choice.",
        },
    },
    {
        .name = "01.02 pancham_cornered, French, B on the text to Pancham's choice (passed 2026-11-16 22:45)",
        .input = {.checkpoint = "01.02", .step = "pancham_cornered", .screens = PANCHAM_FR},
        .expectation = {
            .keys = {
                {"text box", "B"}, {"text box, advance arrow", "B"}, {"still", "-"}, {"nothing", "-"},
                {"text box, choice box", "-"}
            },
            .verdict = "done", .end = "[PLZA scene] done on choice.",
        },
    },
    {
        .name = "03.01 lobby_scene, German, A on the text as Alton's choice opens (B in flight picked his cancel row, 2026-11-17 00:12)",
        .input = {.checkpoint = "03.01", .step = "lobby_scene", .screens = LOBBY_ALTON_DE},
        .expectation = {
            .keys = {
                {"text box", "A"}, {"text box, advance arrow", "A"}, {"text box, choice box, 2 rows, row 0", "A"},
                {"nothing", "A"}, {"still", "A"}, {"black fade", "-"}, {"party bar", "-"}
            },
            .verdict = "done", .end = "[PLZA scene] done on control.",
        },
    },
    {
        //  03.01 on console B 2026-11-19: Taunie asks one 2-row question after
        //  another; B on the next one (taken for the last one back) made her ask
        //  again, B each time, for minutes.
        .name = "03.01 lobby_scene, a 2-row choice, text, another 2-row choice: A on both",
        .input = {.checkpoint = "03.01", .step = "lobby_scene", .screens = R"(
0.0s: text box, advance arrow
0.4s: text box, choice box, 2 rows, row 0
0.8s: nothing
1.2s: text box, advance arrow
1.6s: nothing
2.0s: text box, choice box, 2 rows, row 0
2.4s: nothing
5.0s: party bar
)"},
        .expectation = {
            .keys = {
                {"text box, advance arrow", "A"}, {"text box, choice box, 2 rows, row 0", "A"}, {"nothing", "A"},
                {"party bar", "-"}
            },
            .verdict = "done", .end = "[PLZA scene] done on control.",
        },
    },
    {
        .name = "03.06 wild_zones_tutorial, French, A through the text and the card to control (passed 2026-11-17 14:28)",
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
                {"text box", "A"}, {"text box, still", "A"}, {"text box, advance arrow", "A"}, {"tutorial card", "A"},
                {"still, tutorial card", "A"}, {"nothing", "A"}, {"still", "A"}, {"black fade", "-"}, {"black fade, still", "-"},
                {"party bar", "-"}, {"party bar, still", "-"}
            },
            .verdict = "done", .end = "[PLZA scene] done on control.",
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
            .keys = {
                {"party bar, still, controls hint", "-"}, {"party bar, controls hint", "-"}
            },
            .verdict = "failed: scene_never_started",
            .end = "[Event] 03.06/1 wild_zones_tutorial: failed: scene_never_started: scene: the scene never started (player not at its "
                "trigger?): the HUD up and no text, choice, battle or cutscene for 45000 ms (expecting the controls hint, the party bar "
                "or the A prompt).",
        },
    },
    {
        //  The nurse's text box after the A is synthetic.
        .name = "03.04 into_pokemon_center, a timed walk whose last leg ends at the nurse's A prompt: A, her talk (passed 2026-11-17 14:26)",
        .input = {.checkpoint = "03.04", .step = "into_pokemon_center", .screens = R"(
0.0s: party bar
9.5s: party bar, A prompt
10.0s: text box, still
)"},
        .expectation = {
            .keys = {{"party bar", "L or stick or hold"}, {"party bar, A prompt", "A"}, {"text box, still", "-"}},
            .verdict = "done", .end = "[PLZA] walk: A on the A prompt after 2000 ms of the last leg started a talk.",
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
            .end = "[Event] 03.04/1 into_pokemon_center: failed: walk_no_end: walk: no talk started within 1000 ms after the last leg",
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
                {"text box", "A"}, {"advance arrow", "A"}, {"nothing", "A"}, {"still", "A"}, {"black fade", "-"},
                {"controls hint", "-"}
            },
            .verdict = "done", .end = "[PLZA scene] done on control.",
        },
    },
    {
        .name = "01.02 battle_area, Italian, the A Parla prompt held ends the step, with no A on it (A there restarted the talk 2026-11-15 21:56)",
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
                {"text box", "A"}, {"text box, advance arrow", "A"}, {"nothing", "A"}, {"still", "A"}, {"black fade", "-"},
                {"A prompt", "-"}
            },
            .verdict = "done", .end = "[PLZA scene] done on control.",
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
            .keys = {
                {"black fade, still", "-"}, {"still", "A"}, {"nothing", "A"}, {"controls hint", "-"}
            },
            .verdict = "done", .end = "[PLZA scene] done on control.",
        },
    },
    {
        //  Console B, 2026-11-19 16:20:42: the talk's last lines, then the
        //  black fade that opens the president's ad.
        .name = "01.01 arrival_to_ad, English, A through the talk, done on the black fade",
        .input = {.checkpoint = "01.01", .step = "arrival_to_ad", .screens = R"(
0.0s: text box, advance arrow
0.4s: nothing
5.7s: text box, advance arrow
6.1s: nothing
6.9s: black fade
)"},
        .expectation = {
            .keys = {{"text box, advance arrow", "A"}, {"nothing", "A"}, {"black fade", "-"}},
            .verdict = "done", .end = "[PLZA scene] done on black_fade.",
        },
    },
    {
        //  Console B, 2026-11-19 06:19, its times moved to this step's Plus:
        //  Plus 3.4 s after the opening fade skipped the cutscene, the black
        //  fade 2.7 s after the press (unskipped, it played 76 s).
        .name = "02.02 skip_street_cutscene, Plus 2.5 s after the opening fade ends, done on its black fade",
        .input = {.checkpoint = "02.02", .step = "skip_street_cutscene", .screens = R"(
0.0s: black fade, still
5.6s: still
10.8s: black fade
15.6s: still
)"},
        .expectation = {
            .keys = {{"black fade, still", "-"}, {"still", "+"}, {"black fade", "-"}},
            .verdict = "done", .end = "[PLZA] plus_skip: a black fade 750 ms after the hold.",
        },
    },
    {
        .name = "02.02 skip_street_cutscene, no black fade within 5 s after Plus: cutscene_not_skipped",
        .input = {.checkpoint = "02.02", .step = "skip_street_cutscene", .screens = R"(
0.0s: black fade, still
5.6s: still
)"},
        .expectation = {
            .keys = {{"black fade, still", "-"}, {"still", "+"}},
            .verdict = "failed: cutscene_not_skipped", .end = "[PLZA] plus_skip: no black fade within 5000 ms after the hold.",
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

std::string joined_battle_steps(const std::vector<std::string>& texts, std::string_view separator){
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
    const std::string log = joined_battle_steps(backend.lines(), "\n");
    if (failure){
        return std::format("expected done, got failed: {}; log:\n{}", failure->failure, log);
    }
    const std::string inputs = joined_battle_steps(backend.inputs(), ", ");
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


void add_tests_AutoStorySceneGolden(UnitTestDatabase& database){
    for (const GoldenCase& test : GOLDEN_CASES){
        database.add<GoldenTest>(test);
    }
    for (const BattleJumpCase& test : BATTLE_JUMP_CASES){
        database.add<Test_BattleJump>(test);
    }
}


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
