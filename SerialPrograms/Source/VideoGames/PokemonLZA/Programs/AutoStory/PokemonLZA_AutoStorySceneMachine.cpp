/*  PokemonLZA AutoStory - Scene machine
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Three parts: SceneTimeline names the screen a look shows (classify())
 *  from the look and what earlier looks showed (record()); SCREEN_RULES says
 *  what the runner does on each screen; SceneRunner does it and SceneLog
 *  writes the lines.
 *
 */
//  code-health: allow file_lines -- one state machine: the classifier, the per-screen rule table and the runner read together; deleting the config fields the [SceneQuirk] lines prove dead brings it back under the limit

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <format>
#include <functional>
#include <set>
#include <stdint.h>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/PrettyPrint.h"
#include "CommonTools/ScreenMachine/ScreenMachine.h"
#include "Detectors/PokemonLZA_AutoStoryTutorialCardDetector.h"
#include "PokemonLZA_AutoStoryMenuReadings.h"
#include "PokemonLZA_AutoStoryReadings.h"
#include "PokemonLZA_AutoStorySceneMachine.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;


namespace{


//  Held this long is free control (two looks or more); the party bar also
//  shows between a scene's lines, and A on a prompt restarts the talk (01.02).
constexpr Milliseconds CONTROLS_HINT_HOLD = 1000ms;
constexpr Milliseconds PARTY_BAR_HOLD = 1000ms;
constexpr Milliseconds PROMPT_HOLD = 1000ms;
//  With stop_at_battle or stop_at_choice, the party bar this long with
//  nothing busy is the scene over with no battle or choice.
constexpr Milliseconds CONTROL_INSTEAD_HOLD = 5000ms;
//  The player can move 5 s after 01.01's third fade at the latest.
constexpr Milliseconds FADE_SETTLE = 5000ms;
constexpr Milliseconds FADE_WAIT = 30000ms;
constexpr Milliseconds STILL_WAIT = 10000ms;
//  The longest a cutscene may hold one still frame.
constexpr Milliseconds SCENE_STILL_WAIT = 60000ms;
constexpr Milliseconds POLL = 250ms;
//  "MAIN MISSION 01" shows ~3 s; 01.03's "MAIN MISSION 02" stays past 10 s
//  and gets A.
constexpr Milliseconds BANNER_GONE_WAIT = 10000ms;
constexpr size_t BANNER_PRESSES = 3;
constexpr Milliseconds BANNER_PRESS_EVERY = 1000ms;
//  With end_after_black_fades: the Italian banner stays until A.
constexpr Milliseconds BANNER_PRESS_AFTER = 2000ms;
//  The battle HUD shows ~1 s after the fade when the camera faces the
//  opponent; a still screen this long after is a battle off camera (03.02b).
constexpr Milliseconds LOCK_ON_AFTER = 3000ms;
constexpr size_t LOCK_ON_ATTEMPTS = 5;
//  TrainerBattle's attempt_one_attack() looks 1 s, the catches' sweep 1.5 s.
constexpr Milliseconds LOCK_ON_HOLD = 2000ms;
//  Upstream's AutoStory (#1933) holds Plus 2 s, then takes the skip
//  prompt's default row with A. A talk's screen between two lines shows no
//  text for up to 2.5 s (01.02's Pancham scene).
constexpr Milliseconds SKIP_AFTER = 3000ms;
constexpr Milliseconds SKIP_HOLD = 2000ms;
//  From the look that pressed Plus: the hold, then the prompt.
constexpr Milliseconds SKIP_PROMPT_WAIT = 5000ms;
constexpr Milliseconds PRESS_HOLD = 160ms;
constexpr Milliseconds PRESS_RELEASE = 240ms;
//  The log sums up repeated looks this often.
constexpr Milliseconds SUMMARY_EVERY = 30000ms;


//  In the order classify() tries them, which is also SCREEN_RULES' order.
enum class SceneScreen{
    BATTLE,
    FADE_BLACK,
    FADE_WHITE,
    SKIP_PROMPT,    //  a choice right after SKIP_CUTSCENE: A on its default row
    CHOICE,
    DIALOG,         //  a box or the advance chevron
    MAP,            //  Plus on free control opens it: B
    MISSION_COMPLETE,
    TUTORIAL_CARD,  //  only A turns its pages
    MISSION_BANNER, //  terminal on its second look
    BANNER_SHOWING, //  its first look (every look, with end_after_black_fades)
    BANNER_PRESS,
    FADE_SETTLED,
    CAN_MOVE,       //  the party bar, held PARTY_BAR_HOLD
    PARTY_BAR,
    CONTROLS_HINT,
    HINT_SHOWING,
    TALK_PROMPT,    //  an A prompt held PROMPT_HOLD, past min_length
    PROMPT_SHOWING,
    LOCKED_ON,
    LOCK_ON,
    ENDED_IN_CONTROL,   //  failing: stop_at_battle/choice, but control held CONTROL_INSTEAD_HOLD
    AWAITING_SCENE,
    NEVER_STARTED,
    QUIET_END,
    SKIP_CUTSCENE,  //  moving with no text or HUD for SKIP_AFTER: Plus held SKIP_HOLD
    BLIND_PRESS,
    SCENE_MOVING,
    SCENE_STILL,
};

//  What the runner does on a screen.
enum class SceneAction{
    END,            //  terminal: the machine stops
    PRESS_A,
    PRESS_B,
    PRESS_TEXT,     //  dialog_button(), or towards a choice's top row
    PRESS_BLIND,    //  config.blind_button
    HOLD_PLUS,      //  SKIP_HOLD, to skip a cutscene
    LOCK_ON,        //  ZL held while config.lock_on watches
    WAIT_FADE,      //  until the fade lifts, up to FADE_WAIT
    WAIT_STILL,     //  until the screen is another, up to STILL_WAIT
    POLL,           //  one POLL
    POLL_TWICE,
};

bool action_presses(SceneAction action){
    switch (action){
    case SceneAction::PRESS_A:
    case SceneAction::PRESS_B:
    case SceneAction::PRESS_TEXT:
    case SceneAction::PRESS_BLIND:
    case SceneAction::HOLD_PLUS:
    case SceneAction::LOCK_ON:
        return true;
    default:
        return false;
    }
}

struct SceneScreenRule{
    SceneScreen screen = SceneScreen::BATTLE;
    std::string_view name = {};
    SceneAction action = SceneAction::END;
    //  How long the screen may stay unchanged; zero: every look must change
    //  it (DIALOG's presses must).
    Milliseconds max_still = Milliseconds::zero();
    //  A box or a card that A does not get past fails.
    size_t max_visits = SIZE_MAX;
    //  Its line goes to the overlay too, not only to the file log.
    bool on_overlay = false;
    //  The config field that alone makes this screen ([SceneQuirk]).
    std::optional<std::string_view> field = std::nullopt;
};

//  One row per SceneScreen, in its order. SCENE_STILL_WAIT is for screens
//  whose presses need not change the screen.
constexpr SceneScreenRule SCREEN_RULES[] = {
    {.screen = SceneScreen::BATTLE, .name = "battle", .action = SceneAction::END, .field = "stop_at_battle"},
    {.screen = SceneScreen::FADE_BLACK, .name = "fade_black", .action = SceneAction::WAIT_FADE, .max_still = FADE_WAIT, .on_overlay = true},
    {.screen = SceneScreen::FADE_WHITE, .name = "fade_white", .action = SceneAction::WAIT_FADE, .max_still = FADE_WAIT},
    {.screen = SceneScreen::SKIP_PROMPT, .name = "skip_prompt", .action = SceneAction::PRESS_A, .on_overlay = true},
    {.screen = SceneScreen::CHOICE, .name = "choice", .action = SceneAction::END, .field = "stop_at_choice"},
    {.screen = SceneScreen::DIALOG, .name = "dialog", .action = SceneAction::PRESS_TEXT, .max_visits = 400},
    {.screen = SceneScreen::MAP, .name = "map", .action = SceneAction::PRESS_B, .on_overlay = true},
    {.screen = SceneScreen::MISSION_COMPLETE, .name = "mission_complete", .action = SceneAction::PRESS_A, .max_visits = 30},
    {.screen = SceneScreen::TUTORIAL_CARD, .name = "tutorial_card", .action = SceneAction::PRESS_A},
    {.screen = SceneScreen::MISSION_BANNER, .name = "mission_banner", .action = SceneAction::END},
    //  ~30 s of looks: an uncleared banner's frames change too much for max_still.
    {.screen = SceneScreen::BANNER_SHOWING, .name = "banner_showing", .action = SceneAction::POLL, .max_still = STILL_WAIT, .max_visits = 120, .on_overlay = true},
    {.screen = SceneScreen::BANNER_PRESS, .name = "banner_press", .action = SceneAction::PRESS_A, .max_still = SCENE_STILL_WAIT, .on_overlay = true, .field = "end_after_black_fades"},
    {.screen = SceneScreen::FADE_SETTLED, .name = "fade_settled", .action = SceneAction::END, .field = "end_after_black_fades"},
    {.screen = SceneScreen::CAN_MOVE, .name = "can_move", .action = SceneAction::END},
    {.screen = SceneScreen::PARTY_BAR, .name = "party_bar", .action = SceneAction::POLL, .max_still = STILL_WAIT},
    {.screen = SceneScreen::CONTROLS_HINT, .name = "controls_hint", .action = SceneAction::END},
    {.screen = SceneScreen::HINT_SHOWING, .name = "hint_showing", .action = SceneAction::POLL, .max_still = STILL_WAIT, .on_overlay = true},
    {.screen = SceneScreen::TALK_PROMPT, .name = "talk_prompt", .action = SceneAction::END},
    {.screen = SceneScreen::PROMPT_SHOWING, .name = "prompt_showing", .action = SceneAction::POLL, .max_still = SCENE_STILL_WAIT},
    {.screen = SceneScreen::LOCKED_ON, .name = "locked_on", .action = SceneAction::END, .field = "stop_at_battle"},
    {.screen = SceneScreen::LOCK_ON, .name = "lock_on", .action = SceneAction::LOCK_ON, .max_still = SCENE_STILL_WAIT, .field = "stop_at_battle"},
    //  Its field depends on the config: SceneTimeline::fields_fired().
    {.screen = SceneScreen::ENDED_IN_CONTROL, .name = "ended_in_control", .action = SceneAction::END},
    {.screen = SceneScreen::AWAITING_SCENE, .name = "awaiting_scene", .action = SceneAction::POLL, .max_still = SCENE_STILL_WAIT, .field = "start_window"},
    {.screen = SceneScreen::NEVER_STARTED, .name = "never_started", .action = SceneAction::END, .field = "start_window"},
    {.screen = SceneScreen::QUIET_END, .name = "quiet_end", .action = SceneAction::END, .field = "quiet"},
    {.screen = SceneScreen::SKIP_CUTSCENE, .name = "skip_cutscene", .action = SceneAction::HOLD_PLUS, .on_overlay = true},
    {.screen = SceneScreen::BLIND_PRESS, .name = "blind_press", .action = SceneAction::PRESS_BLIND, .max_still = SCENE_STILL_WAIT},
    {.screen = SceneScreen::SCENE_MOVING, .name = "scene_moving", .action = SceneAction::POLL_TWICE},
    {.screen = SceneScreen::SCENE_STILL, .name = "scene_still", .action = SceneAction::WAIT_STILL, .max_still = SCENE_STILL_WAIT},
};

constexpr bool screen_rules_in_enum_order(){
    for (size_t index = 0; index < std::size(SCREEN_RULES); index++){
        if (SCREEN_RULES[index].screen != static_cast<SceneScreen>(index)){
            return false;
        }
    }
    return SCREEN_RULES[std::size(SCREEN_RULES) - 1].screen == SceneScreen::SCENE_STILL;
}
static_assert(screen_rules_in_enum_order(), "SCREEN_RULES must list every SceneScreen in its order.");

const SceneScreenRule& rule_of(SceneScreen screen){
    return SCREEN_RULES[static_cast<size_t>(screen)];
}

std::string_view scene_screen_name(SceneScreen screen){
    return rule_of(screen).name;
}

//  The screens classify_idle() gives short of a quiet end.
bool idle_screen(SceneScreen screen){
    return screen == SceneScreen::SKIP_CUTSCENE || screen == SceneScreen::BLIND_PRESS ||
        screen == SceneScreen::SCENE_MOVING || screen == SceneScreen::SCENE_STILL;
}


bool busy(const ScreenObservation& look){
    return text_shows(look) || fade_shows(look) || look.readings.contains(MISSION_COMPLETE_READING) ||
        look.readings.contains(TUTORIAL_CARD_READING);
}

bool scene_shows(const ScreenObservation& look){
    return text_shows(look) || look.events.has(ScreenEvent::CHOICE_BOX) || look.events.has(ScreenEvent::BATTLE_STARTED);
}

bool hud_shows(const ScreenObservation& look){
    return look.events.has(ScreenEvent::PLAYER_CAN_MOVE) || look.readings.contains(CONTROLS_HINT_READING) ||
        look.events.has(ScreenEvent::INTERACT_PROMPT);
}

//  Not when stopping at a choice or a battle: the talk shows them between lines.
bool party_bar_is_control(const SceneMachineConfig& config){
    return !config.stop_at_choice && !config.stop_at_battle;
}

bool control_prompt_shows(const SceneMachineConfig& config, const ScreenObservation& look){
    return party_bar_is_control(config) && look.events.has(ScreenEvent::INTERACT_PROMPT);
}

//  On a closing box B does nothing, where A restarts the talk (03.05's nurse).
Button default_text_button(const SceneMachineConfig& config, const ScreenObservation& look){
    return party_bar_is_control(config) && look.events.has(ScreenEvent::DIALOG_BOX) ? BUTTON_B : BUTTON_A;
}

Button dialog_button(const SceneMachineConfig& config, const ScreenObservation& look){
    return config.text_button.value_or(default_text_button(config, look));
}

void track_since(bool shows, WallClock when, std::optional<WallClock>& since){
    if (!shows){
        since.reset();
    }else if (!since){
        since = when;
    }
}

bool held_for(const std::optional<WallClock>& since, WallClock now, Milliseconds hold){
    return since && now - *since >= hold;
}

//  "a, b or c".
std::string join_with_or(const std::vector<std::string>& items){
    std::string text;
    for (size_t index = 0; index < items.size(); index++){
        const std::string_view separator = index == 0 ? "" : index + 1 == items.size() ? " or " : ", ";
        text += std::format("{}{}", separator, items[index]);
    }
    return text;
}

//  "the controls hint, the party bar, the A prompt or the mission banner".
std::string expected_end(const SceneMachineConfig& config){
    std::vector<std::string> ends;
    if (config.stop_at_battle){
        ends.push_back("the battle HUD");
    }
    if (config.stop_at_choice){
        ends.push_back("the choice box");
    }
    if (config.end_after_black_fades){
        ends.push_back(std::format("black fade {} settled", *config.end_after_black_fades));
    }
    ends.push_back("the controls hint");
    if (party_bar_is_control(config)){
        ends.push_back("the party bar");
        ends.push_back("the A prompt");
    }
    if (!config.end_after_black_fades){
        ends.push_back("the mission banner");
    }
    if (config.quiet_is_control){
        ends.push_back("quiet");
    }
    return join_with_or(ends);
}


//  The time-dependent part of the machine; a look's time is its `when`.
//  classify() reads the state; record() then adds the look to it.
class SceneTimeline{
public:
    SceneTimeline(SceneMachineConfig config, WallClock start);

    SceneScreen classify(const ScreenObservation& look) const;

    void record(const ScreenObservation& look);

    //  Since the last text or choice.
    size_t lock_ons() const{
        return m_lock_ons;
    }
    //  Every later look is LOCKED_ON.
    void lock_on_seen(){
        m_locked_on = true;
    }

    //  "fade 2/3" ("fade 2" without end_after_black_fades).
    std::string black_fade_count() const;

    bool banner_seen() const{
        return m_banner_seen;
    }

    //  The config fields that change what `screen`, classify()'s for
    //  `look`, does; before record(look).
    std::vector<std::string_view> fields_fired(SceneScreen screen, const ScreenObservation& look) const;

private:
    //  classify() past the fades, the text and the lock-on.
    SceneScreen classify_open(const ScreenObservation& look) const;
    //  classify_open() past the cards, the banner and the ends that fail.
    SceneScreen classify_control(const ScreenObservation& look) const;
    //  classify_control() with nothing busy and no sign of control.
    SceneScreen classify_idle(const ScreenObservation& look) const;
    SceneScreen classify_banner(const ScreenObservation& look) const;

    std::optional<WallClock> quiet_end_due() const;
    bool gap_press_due(WallClock now) const;
    bool blind_press_due(WallClock now) const;
    bool skip_pending(WallClock now) const;
    bool skip_due(const ScreenObservation& look) const;
    bool banner_press_due(WallClock now) const;
    bool lock_on_due(const ScreenObservation& look) const;
    //  A quiet end or a prompt end that only min_length holds off.
    bool held_by_min_length(SceneScreen screen, const ScreenObservation& look) const;

    //  The parts of record(), in the order it runs them.
    void record_presses(SceneScreen screen, WallClock now);
    void record_cutscene(const ScreenObservation& look);
    void record_battle(SceneScreen screen, const ScreenObservation& look);
    void record_control(const ScreenObservation& look);
    void record_banner(const ScreenObservation& look);
    void record_black_fades(const ScreenObservation& look);

    SceneMachineConfig m_config;

    //  Scene and quiet.
    WallClock m_start;
    //  min_length's end.
    WallClock m_quiet_from;
    WallClock m_last_busy;
    //  The last look busy or with a sign of control.
    WallClock m_last_cue;
    bool m_scene_seen = false;
    std::optional<WallClock> m_last_blind_press;

    //  Signs of control, each since it began to show.
    std::optional<WallClock> m_party_bar_since;
    std::optional<WallClock> m_hint_since;
    std::optional<WallClock> m_prompt_since;
    //  With stop_at_battle or stop_at_choice: the party bar with nothing busy.
    std::optional<WallClock> m_control_since;

    //  The mission banner. Looks in a row.
    size_t m_banner_looks = 0;
    bool m_banner_seen = false;
    std::optional<WallClock> m_banner_since;
    size_t m_banner_presses = 0;
    std::optional<WallClock> m_last_banner_press;

    //  end_after_black_fades.
    size_t m_black_fades = 0;
    //  Once end_after_black_fades' fades have all lifted.
    std::optional<WallClock> m_settled_since;

    //  stop_at_battle's lock-on. Since the last text or choice.
    bool m_after_fade = false;
    std::optional<WallClock> m_quiet_after_fade_since;
    size_t m_lock_ons = 0;
    bool m_locked_on = false;

    //  Cutscene skips. Moving with no text, fade or HUD.
    std::optional<WallClock> m_moving_since;
    //  The last look with text, a fade or the HUD: a skip press waits for
    //  one after the last, so a scene Plus does not skip gets one press.
    WallClock m_last_skip_cue;
    std::optional<WallClock> m_last_skip;
};


SceneTimeline::SceneTimeline(SceneMachineConfig config, WallClock start)
    : m_config(std::move(config))
    , m_start(start)
    , m_quiet_from(start + m_config.min_length)
    , m_last_busy(start)
    , m_last_cue(start)
    , m_last_skip_cue(start)
{}

std::string SceneTimeline::black_fade_count() const{
    if (!m_config.end_after_black_fades){
        return std::format("fade {}", m_black_fades);
    }
    return std::format("fade {}/{}", m_black_fades, *m_config.end_after_black_fades);
}


SceneScreen SceneTimeline::classify(const ScreenObservation& look) const{
    if (m_config.stop_at_battle && look.events.has(ScreenEvent::BATTLE_STARTED)){
        return SceneScreen::BATTLE;
    }
    if (m_locked_on){
        return SceneScreen::LOCKED_ON;
    }
    if (look.events.has(ScreenEvent::FADE_BLACK)){
        return SceneScreen::FADE_BLACK;
    }
    if (look.events.has(ScreenEvent::FADE_WHITE)){
        return SceneScreen::FADE_WHITE;
    }
    if (skip_pending(look.when) && look.events.has(ScreenEvent::CHOICE_BOX)){
        return SceneScreen::SKIP_PROMPT;
    }
    if (m_config.stop_at_choice && look.events.has(ScreenEvent::CHOICE_BOX)){
        return SceneScreen::CHOICE;
    }
    if (text_shows(look)){
        return SceneScreen::DIALOG;
    }
    if (look.readings.contains(MAP_READING)){
        return SceneScreen::MAP;
    }
    if (lock_on_due(look)){
        return SceneScreen::LOCK_ON;
    }
    return classify_open(look);
}

SceneScreen SceneTimeline::classify_open(const ScreenObservation& look) const{
    if (look.readings.contains(MISSION_COMPLETE_READING)){
        return SceneScreen::MISSION_COMPLETE;
    }
    if (look.readings.contains(TUTORIAL_CARD_READING)){
        return SceneScreen::TUTORIAL_CARD;
    }
    if (look.readings.contains(MISSION_BANNER_READING)){
        return classify_banner(look);
    }
    if (held_for(m_settled_since, look.when, FADE_SETTLE)){
        return SceneScreen::FADE_SETTLED;
    }
    if (look.events.has(ScreenEvent::PLAYER_CAN_MOVE) && held_for(m_control_since, look.when, CONTROL_INSTEAD_HOLD)){
        return SceneScreen::ENDED_IN_CONTROL;
    }
    if (m_config.start_window && !m_scene_seen && hud_shows(look)){
        const bool past_window = look.when - m_start >= *m_config.start_window;
        return past_window ? SceneScreen::NEVER_STARTED : SceneScreen::AWAITING_SCENE;
    }
    return classify_control(look);
}

SceneScreen SceneTimeline::classify_banner(const ScreenObservation& look) const{
    if (!m_config.end_after_black_fades && m_banner_looks >= 1){
        return SceneScreen::MISSION_BANNER;
    }
    return banner_press_due(look.when) ? SceneScreen::BANNER_PRESS : SceneScreen::BANNER_SHOWING;
}

//  Each sign of control ends the machine once it has held a while.
SceneScreen SceneTimeline::classify_control(const ScreenObservation& look) const{
    if (party_bar_is_control(m_config) && look.events.has(ScreenEvent::PLAYER_CAN_MOVE)){
        return held_for(m_party_bar_since, look.when, PARTY_BAR_HOLD) ? SceneScreen::CAN_MOVE : SceneScreen::PARTY_BAR;
    }
    if (look.readings.contains(CONTROLS_HINT_READING)){
        return held_for(m_hint_since, look.when, CONTROLS_HINT_HOLD) ? SceneScreen::CONTROLS_HINT : SceneScreen::HINT_SHOWING;
    }
    if (control_prompt_shows(m_config, look)){
        const bool held = held_for(m_prompt_since, look.when, PROMPT_HOLD) && look.when >= m_quiet_from;
        return held ? SceneScreen::TALK_PROMPT : SceneScreen::PROMPT_SHOWING;
    }
    return classify_idle(look);
}

SceneScreen SceneTimeline::classify_idle(const ScreenObservation& look) const{
    const std::optional<WallClock> due = quiet_end_due();
    if (due && look.when >= *due){
        return SceneScreen::QUIET_END;
    }
    if (skip_due(look)){
        return SceneScreen::SKIP_CUTSCENE;
    }
    if (blind_press_due(look.when)){
        return SceneScreen::BLIND_PRESS;
    }
    return look.events.has(ScreenEvent::SCREEN_STILL) ? SceneScreen::SCENE_STILL : SceneScreen::SCENE_MOVING;
}


std::optional<WallClock> SceneTimeline::quiet_end_due() const{
    if (!m_config.quiet){
        return std::nullopt;
    }
    return std::max(m_quiet_from, m_last_busy + *m_config.quiet);
}

bool SceneTimeline::gap_press_due(WallClock now) const{
    const bool pressed_since_cue = m_last_blind_press && *m_last_blind_press >= m_last_cue;
    return m_config.gap_press && m_scene_seen && !pressed_since_cue && now - m_last_cue >= *m_config.gap_press;
}

bool SceneTimeline::blind_press_due(WallClock now) const{
    if (gap_press_due(now)){
        return true;
    }
    if (!m_config.blind_press_after || now - m_last_cue < *m_config.blind_press_after){
        return false;
    }
    return !m_last_blind_press || now - *m_last_blind_press >= m_config.blind_press_every;
}

//  A Plus press with no cue since, whose skip prompt may still come.
bool SceneTimeline::skip_pending(WallClock now) const{
    return m_last_skip && now - *m_last_skip <= SKIP_PROMPT_WAIT && *m_last_skip >= m_last_skip_cue;
}

//  Not after a battle's fade: its effects move with no HUD while the camera
//  looks away (03.02b's Tepig battle). Not with blind presses: those are
//  for menus no detector reads (03.04's Pokedex), not cutscenes.
bool SceneTimeline::skip_due(const ScreenObservation& look) const{
    if ((m_config.stop_at_battle && m_after_fade) || m_config.blind_press_after || !held_for(m_moving_since, look.when, SKIP_AFTER)){
        return false;
    }
    return !m_last_skip || *m_last_skip < m_last_skip_cue;
}

bool SceneTimeline::banner_press_due(WallClock now) const{
    if (!m_config.end_after_black_fades || !held_for(m_banner_since, now, BANNER_PRESS_AFTER)){
        return false;
    }
    return m_banner_presses < BANNER_PRESSES && (!m_last_banner_press || now - *m_last_banner_press >= BANNER_PRESS_EVERY);
}

bool SceneTimeline::lock_on_due(const ScreenObservation& look) const{
    if (!m_config.stop_at_battle || !m_config.lock_on || !m_after_fade || m_lock_ons >= LOCK_ON_ATTEMPTS || busy(look)){
        return false;
    }
    return held_for(m_quiet_after_fade_since, look.when, LOCK_ON_AFTER) ||
        (look.events.has(ScreenEvent::PLAYER_CAN_MOVE) && held_for(m_control_since, look.when, PARTY_BAR_HOLD));
}

bool SceneTimeline::held_by_min_length(SceneScreen screen, const ScreenObservation& look) const{
    if (look.when >= m_quiet_from){
        return false;
    }
    if (screen == SceneScreen::PROMPT_SHOWING){
        return held_for(m_prompt_since, look.when, PROMPT_HOLD);
    }
    return idle_screen(screen) && m_config.quiet && look.when >= m_last_busy + *m_config.quiet;
}

std::vector<std::string_view> SceneTimeline::fields_fired(SceneScreen screen, const ScreenObservation& look) const{
    std::vector<std::string_view> fired;
    const auto add = [&fired](bool when, std::string_view field){
        if (when){
            fired.push_back(field);
        }
    };
    if (const std::optional<std::string_view> field = rule_of(screen).field){
        fired.push_back(*field);
    }
    const bool blind = screen == SceneScreen::BLIND_PRESS;
    const bool gap = blind && gap_press_due(look.when);
    add(screen == SceneScreen::QUIET_END && m_config.quiet_is_control, "quiet_is_control");
    add(screen == SceneScreen::ENDED_IN_CONTROL, m_config.stop_at_battle ? "stop_at_battle" : "stop_at_choice");
    //  Without it, the banner's second look ends the machine.
    add(screen == SceneScreen::BANNER_SHOWING && m_config.end_after_black_fades && m_banner_looks >= 1, "end_after_black_fades");
    add(gap, "gap_press");
    add(blind && !gap, "blind_press_after");
    add(blind && !gap && m_last_blind_press.has_value(), "blind_press_every");
    add(blind && m_config.blind_button != BUTTON_A, "blind_button");
    add(screen == SceneScreen::DIALOG && !look.events.has(ScreenEvent::CHOICE_BOX) &&
        dialog_button(m_config, look) != default_text_button(m_config, look), "text_button");
    add(held_by_min_length(screen, look), "min_length");
    return fired;
}


void SceneTimeline::record(const ScreenObservation& look){
    const SceneScreen screen = classify(look);
    record_presses(screen, look.when);
    record_cutscene(look);
    record_battle(screen, look);
    record_control(look);
    //  After record_presses(): a banner gone resets its presses.
    record_banner(look);
    //  After record_banner(): the fades settle only with the banner gone.
    record_black_fades(look);
    if (busy(look) || m_party_bar_since || m_hint_since || m_prompt_since || m_banner_looks > 0){
        m_last_cue = look.when;
    }
}

void SceneTimeline::record_presses(SceneScreen screen, WallClock now){
    if (screen == SceneScreen::BLIND_PRESS){
        m_last_blind_press = now;
    }
    if (screen == SceneScreen::SKIP_CUTSCENE){
        m_last_skip = now;
    }
    if (screen == SceneScreen::BANNER_PRESS){
        m_banner_presses++;
        m_last_banner_press = now;
    }
}

void SceneTimeline::record_cutscene(const ScreenObservation& look){
    if (busy(look) || hud_shows(look) || look.events.has(ScreenEvent::CHOICE_BOX)){
        m_last_skip_cue = look.when;
    }
    const bool cutscene = !busy(look) && !hud_shows(look) && !look.events.has(ScreenEvent::SCREEN_STILL) &&
        !look.readings.contains(MAP_READING) && !look.readings.contains(MISSION_BANNER_READING);
    track_since(cutscene, look.when, m_moving_since);
}

void SceneTimeline::record_battle(SceneScreen screen, const ScreenObservation& look){
    const bool talk = text_shows(look) || look.events.has(ScreenEvent::CHOICE_BOX);
    m_lock_ons = talk ? 0 : m_lock_ons + (screen == SceneScreen::LOCK_ON ? 1 : 0);
    m_after_fade = (m_after_fade && !talk) || look.events.has(ScreenEvent::FADE_BLACK);
    //  A cutscene (moving, no party bar) is not quiet.
    const bool settled = look.events.has(ScreenEvent::SCREEN_STILL) || look.events.has(ScreenEvent::PLAYER_CAN_MOVE);
    track_since(m_after_fade && !busy(look) && settled, look.when, m_quiet_after_fade_since);
}

void SceneTimeline::record_control(const ScreenObservation& look){
    if (busy(look)){
        m_last_busy = look.when;
    }
    m_scene_seen = m_scene_seen || scene_under_way(look);
    const bool box = look.events.has(ScreenEvent::DIALOG_BOX);
    track_since(party_bar_is_control(m_config) && party_bar_free(look), look.when, m_party_bar_since);
    const bool control_instead = !party_bar_is_control(m_config) && look.events.has(ScreenEvent::PLAYER_CAN_MOVE) && !busy(look);
    track_since(control_instead, look.when, m_control_since);
    track_since(look.readings.contains(CONTROLS_HINT_READING) && !box, look.when, m_hint_since);
    track_since(control_prompt_shows(m_config, look) && !box, look.when, m_prompt_since);
}

void SceneTimeline::record_banner(const ScreenObservation& look){
    const bool box = look.events.has(ScreenEvent::DIALOG_BOX);
    m_banner_looks = look.readings.contains(MISSION_BANNER_READING) && !box ? m_banner_looks + 1 : 0;
    m_banner_seen = m_banner_seen || m_banner_looks > 0;
    track_since(m_banner_looks > 0, look.when, m_banner_since);
    if (!m_banner_since){
        m_banner_presses = 0;
        m_last_banner_press.reset();
    }
}

void SceneTimeline::record_black_fades(const ScreenObservation& look){
    //  Each FADE_BLACK look is a new fade: its state waits until it lifts.
    m_black_fades += look.events.has(ScreenEvent::FADE_BLACK) ? 1 : 0;
    const bool fades_done = m_config.end_after_black_fades && m_black_fades >= *m_config.end_after_black_fades;
    track_since(fades_done && !busy(look) && m_banner_looks == 0, look.when, m_settled_since);
}


std::string describe_look(const ScreenObservation& look){
    const std::pair<ScreenEvent, std::string_view> events[] = {
        {ScreenEvent::FADE_BLACK, "black fade"}, {ScreenEvent::FADE_WHITE, "white fade"},
        {ScreenEvent::DIALOG_BOX, "text box"}, {ScreenEvent::CHOICE_BOX, "choice box"},
        {ScreenEvent::BATTLE_STARTED, "battle"}, {ScreenEvent::PLAYER_CAN_MOVE, "party bar"},
        {ScreenEvent::INTERACT_PROMPT, "A prompt"}, {ScreenEvent::SCREEN_STILL, "still"},
    };
    const std::pair<std::string_view, std::string_view> readings[] = {
        {CONTROLS_HINT_READING, "controls hint"}, {ADVANCE_ARROW_READING, "advance arrow"},
        {MISSION_BANNER_READING, "mission banner"}, {MISSION_COMPLETE_READING, "mission complete"},
        {TUTORIAL_CARD_READING, "tutorial card"}, {MAP_READING, "map"},
    };
    std::string text;
    const auto add = [&text](std::string_view name){
        text += std::format("{}{}", text.empty() ? "" : ", ", name);
    };
    for (const auto& [event, name] : events){
        if (look.events.has(event)){
            add(name);
        }
    }
    for (const auto& [reading, name] : readings){
        if (look.readings.contains(reading)){
            add(name);
        }
    }
    return text.empty() ? "nothing" : text;
}


//  How many states may take turns and still count as one run of looks.
constexpr std::ptrdiff_t TURN_PERIODS[] = {2, 3};

//  A line per change of state or readings ("[PLZA label] screen 3:
//  scene_still at 12.5s: still"); repeats and two or three states taking
//  turns are summed up every SUMMARY_EVERY ("scene_moving x46, blind_press x15
//  over 30.0s, 15 presses").
class SceneLog{
public:
    SceneLog(PlzaBackend& backend, std::string_view label, WallClock start)
        : m_backend(backend)
        , m_label(label)
        , m_start(start)
    {}

    void note(std::string_view text){
        flush();
        m_backend.log(std::format("[PLZA {}] {}", m_label, text));
    }

    //  A config field's first effect, a Plus press's outcome.
    void detail(std::string_view text){
        flush();
        m_backend.log_detail(text);
    }

    void look(SceneScreen screen, std::string_view readings, WallClock when, bool pressed){
        if (screen != m_screen){
            m_screen = screen;
            m_screen_number++;
        }
        if (gets_line(std::format("{}: {}", scene_screen_name(screen), readings))){
            flush();
            write_line(screen, readings, when);
            return;
        }
        count(screen, when, pressed);
        if (when - *m_counted_from >= SUMMARY_EVERY){
            write_summary();
        }
    }

    //  Drops the counted looks unless they took turns or pressed.
    void flush(){
        if (m_counted.size() > 1 || m_presses > 0){
            write_summary();
            return;
        }
        m_counted.clear();
        m_counted_from.reset();
    }

private:
    void write_line(SceneScreen screen, std::string_view readings, WallClock when){
        const double seconds = std::chrono::duration<double>(when - m_start).count();
        const std::string line = std::format(
            "[PLZA {}] screen {}: {} at {}s: {}",
            m_label, m_screen_number, scene_screen_name(screen), tostr_fixed(seconds, 1), readings
        );
        if (rule_of(screen).on_overlay){
            m_backend.log(line);
        }else{
            m_backend.log_detail(line);
        }
    }

    void write_summary(){
        std::string text;
        for (const auto& [screen, looks] : m_counted){
            text += std::format("{}{} x{}", text.empty() ? "" : ", ", scene_screen_name(screen), looks);
        }
        const double seconds = std::chrono::duration<double>(m_counted_to - *m_counted_from).count();
        m_backend.log_detail(std::format("[PLZA {}] {} over {}s, {} presses", m_label, text, tostr_fixed(seconds, 1), m_presses));
        m_counted.clear();
        m_counted_from.reset();
        m_presses = 0;
    }

    //  `key`: "scene_still: still".
    bool gets_line(std::string key){
        if (!m_changes.empty() && key == m_changes.back()){
            return false;
        }
        m_changes.push_back(std::move(key));
        if (m_changes.size() > MAX_CHANGES){
            m_changes.erase(m_changes.begin());
        }
        return !takes_turns();
    }

    //  The last 4 or 6 changes are one run of 2 or 3 repeated.
    bool takes_turns() const{
        for (const std::ptrdiff_t period : TURN_PERIODS){
            if ((std::ptrdiff_t)m_changes.size() >= 2 * period &&
                std::equal(m_changes.end() - period, m_changes.end(), m_changes.end() - 2 * period)
            ){
                return true;
            }
        }
        return false;
    }

    void count(SceneScreen screen, WallClock when, bool pressed){
        if (!m_counted_from){
            m_counted_from = when;
        }
        m_counted_to = when;
        m_presses += pressed ? 1 : 0;
        for (auto& [counted, looks] : m_counted){
            if (counted == screen){
                looks++;
                return;
            }
        }
        m_counted.emplace_back(screen, 1);
    }

    PlzaBackend& m_backend;
    std::string m_label;
    WallClock m_start;
    std::optional<SceneScreen> m_screen;
    size_t m_screen_number = 0;
    static constexpr size_t MAX_CHANGES = 6;
    std::vector<std::string> m_changes;
    std::vector<std::pair<SceneScreen, size_t>> m_counted;
    std::optional<WallClock> m_counted_from;
    WallClock m_counted_to;
    size_t m_presses = 0;
};


//  Logs each look, then does what SCREEN_RULES says for its screen.
class SceneRunner{
public:
    SceneRunner(PlzaBackend& backend, std::string_view label, const SceneMachineConfig& config, WallClock start, std::string_view step)
        : m_backend(backend)
        , m_config(config)
        , m_timeline(config, start)
        , m_log(backend, label, start)
        , m_step(step)
    {}

    const SceneTimeline& timeline() const{
        return m_timeline;
    }

    //  A Plus press still waiting for its outcome did nothing.
    void finish(){
        m_log.flush();
        if (m_skip_pressed){
            log_skip("nothing");
        }
    }

    void act(SceneScreen screen, const ScreenObservation& look){
        const SceneAction action = rule_of(screen).action;
        log_fields_fired(screen, look);
        note_skip_outcome(screen, look);
        m_timeline.record(look);
        std::string readings = describe_look(look);
        if (screen == SceneScreen::FADE_BLACK){
            readings = std::format("{}, {}", readings, m_timeline.black_fade_count());
        }
        m_log.look(screen, readings, look.when, action_presses(action));
        perform(action, look);
    }

private:
    void perform(SceneAction action, const ScreenObservation& look){
        switch (action){
        case SceneAction::END:
            return;
        case SceneAction::PRESS_A:
            m_backend.press(BUTTON_A, PRESS_HOLD, PRESS_RELEASE);
            return;
        case SceneAction::PRESS_B:
            m_backend.press(BUTTON_B, PRESS_HOLD, PRESS_RELEASE);
            return;
        case SceneAction::PRESS_TEXT:
            m_backend.press(text_button(look), PRESS_HOLD, PRESS_RELEASE);
            return;
        case SceneAction::PRESS_BLIND:
            m_backend.press(m_config.blind_button, PRESS_HOLD, PRESS_RELEASE);
            return;
        case SceneAction::HOLD_PLUS:
            m_backend.press(BUTTON_PLUS, SKIP_HOLD, PRESS_RELEASE);
            m_skip_pressed = look.when;
            return;
        case SceneAction::LOCK_ON:
            lock_on();
            return;
        default:
            wait(action);
            return;
        }
    }

    void wait(SceneAction action){
        switch (action){
        case SceneAction::WAIT_FADE:
            wait_until(FADE_WAIT, [](const ScreenObservation& look){
                return !fade_shows(look);
            });
            return;
        case SceneAction::WAIT_STILL:
            //  Also ends on a still screen with the chevron (01.01's subtitles).
            wait_until(STILL_WAIT, [this](const ScreenObservation& look){
                return m_timeline.classify(look) != SceneScreen::SCENE_STILL;
            });
            return;
        case SceneAction::POLL:
            m_backend.wait(POLL);
            return;
        default:
            m_backend.wait(2 * POLL);
            return;
        }
    }

    template <typename Done>
    void wait_until(Milliseconds limit, Done&& done){
        for (Milliseconds waited = 0ms; waited < limit; waited += POLL){
            m_backend.wait(POLL);
            if (done(m_backend.observe())){
                return;
            }
        }
    }

    void log_fields_fired(SceneScreen screen, const ScreenObservation& look){
        for (const std::string_view field : m_timeline.fields_fired(screen, look)){
            if (m_fields_logged.insert(field).second){
                m_log.detail(std::format("[SceneQuirk] {} {} fired", m_step, field));
            }
        }
    }

    //  The look after a Plus press: its skip prompt, the map (Plus on
    //  control), the cutscene still playing SKIP_PROMPT_WAIT on, or else
    //  the cutscene over.
    void note_skip_outcome(SceneScreen screen, const ScreenObservation& look){
        if (!m_skip_pressed){
            return;
        }
        if (screen == SceneScreen::SKIP_PROMPT){
            log_skip("prompt");
        }else if (screen == SceneScreen::MAP){
            log_skip("map");
        }else if (screen != SceneScreen::SCENE_MOVING && screen != SceneScreen::SCENE_STILL && screen != SceneScreen::SKIP_CUTSCENE){
            log_skip("skipped");
        }else if (look.when - *m_skip_pressed >= SKIP_PROMPT_WAIT){
            log_skip("nothing");
        }
    }

    void log_skip(std::string_view outcome){
        m_skip_pressed.reset();
        m_log.detail(std::format("[PlusSkip] {} pressed -> {}", m_step, outcome));
    }

    //  Before all but the first, a quarter turn and L put the camera behind
    //  the player (the backend has no camera stick).
    void lock_on(){
        const size_t attempt = m_timeline.lock_ons();
        if (attempt > 1){
            m_backend.stick(/*degrees=*/90, /*duration=*/150ms, /*release=*/100ms, Pokemon::StoryPace::WALK);
            m_backend.press(BUTTON_L, /*hold=*/100ms, /*release=*/150ms);
        }
        const bool symbols = (*m_config.lock_on)(LOCK_ON_HOLD);
        if (symbols){
            m_timeline.lock_on_seen();
        }
        const std::string seen = symbols ? "the move symbols showed" : std::format("no move symbols in {} ms", LOCK_ON_HOLD.count());
        m_log.note(std::format("lock-on {}/{}: {}ZL held: {}.", attempt, LOCK_ON_ATTEMPTS, attempt > 1 ? "turned 90 degrees right, L, " : "", seen));
    }

    //  A text box: dialog_button(). A choice: Up until the top row is
    //  highlighted (03.01's German lobby), then A.
    Button text_button(const ScreenObservation& look){
        if (!look.events.has(ScreenEvent::CHOICE_BOX)){
            return dialog_button(m_config, look);
        }
        const auto rows = look.readings.find(CHOICE_ROWS_READING);
        const auto selected = look.readings.find(CHOICE_SELECTED_READING);
        if (rows == look.readings.end() || selected == look.readings.end()){
            m_log.note("choice: rows not read: A");
            return BUTTON_A;
        }
        if (selected->second > 0){
            m_log.note(std::format("choice: {} rows, row {} highlighted: Up to row 0", rows->second, selected->second));
            return BUTTON_UP;
        }
        m_log.note(std::format("choice: {} rows, row 0 highlighted: A", rows->second));
        return BUTTON_A;
    }

    PlzaBackend& m_backend;
    SceneMachineConfig m_config;
    SceneTimeline m_timeline;
    SceneLog m_log;
    std::string m_step;
    std::set<std::string_view> m_fields_logged;
    std::optional<WallClock> m_skip_pressed;
};


//  One ScreenMachine state per SceneScreen; a state matches when the
//  timeline classifies the look as its screen, so exactly one matches.
std::vector<ScreenState> scene_states(const ScreenObservation& look, SceneRunner& runner){
    std::vector<ScreenState> states;
    for (const SceneScreenRule& rule : SCREEN_RULES){
        const SceneScreen screen = rule.screen;
        ScreenState state;
        state.name = std::string(rule.name);
        state.matches = [&runner, screen](const ScreenObservation& observation){
            return runner.timeline().classify(observation) == screen;
        };
        state.act = [&runner, &look, screen]{
            runner.act(screen, look);
        };
        state.max_visits = rule.max_visits;
        state.max_still = rule.max_still;
        state.terminal = rule.action == SceneAction::END;
        states.push_back(std::move(state));
    }
    return states;
}

bool banner_shows(PlzaBackend& backend){
    return backend.observe().readings.contains(MISSION_BANNER_READING);
}

//  So the next step starts with control.
void wait_banner_gone(PlzaBackend& backend, std::string_view label){
    for (Milliseconds waited = 0ms; waited < BANNER_GONE_WAIT; waited += POLL){
        if (!banner_shows(backend)){
            backend.log(std::format("[PLZA {}] the mission banner has gone after {} ms.", label, waited.count()));
            return;
        }
        backend.wait(POLL);
    }
    backend.log(std::format("[PLZA {}] the mission banner still shows after {} ms; pressing A.", label, BANNER_GONE_WAIT.count()));
    for (size_t press = 1; press <= BANNER_PRESSES; press++){
        backend.press(BUTTON_A, PRESS_HOLD, PRESS_RELEASE);
        backend.wait(BANNER_PRESS_EVERY);
        if (!banner_shows(backend)){
            backend.log(std::format("[PLZA {}] the mission banner has gone after A press {}.", label, press));
            return;
        }
    }
    backend.log(std::format("[PLZA {}] the mission banner still shows after {} A presses; going on.", label, BANNER_PRESSES));
}

//  The failure a terminal screen stands for, if any.
std::optional<Pokemon::StoryFailure> terminal_failure(
    std::string_view label, const SceneMachineConfig& config, std::string_view terminal, std::string_view expected
){
    if (terminal == scene_screen_name(SceneScreen::NEVER_STARTED)){
        return Pokemon::StoryFailure{.code = "scene_never_started", .detail = std::format(
            "{}: the scene never started (player not at its trigger?): the HUD up and no text, choice, battle or cutscene for {} ms (expecting {}).",
            label, config.start_window ? config.start_window->count() : 0, expected
        )};
    }
    if (terminal == scene_screen_name(SceneScreen::ENDED_IN_CONTROL)){
        return Pokemon::StoryFailure{.code = "scene_ended_in_control", .detail = std::format(
            "{}: the scene ended in control, the party bar up {} ms with nothing busy (expecting {}).",
            label, CONTROL_INSTEAD_HOLD.count(), expected
        )};
    }
    if (terminal == scene_screen_name(SceneScreen::QUIET_END) && !config.quiet_is_control){
        //  A cutscene with no text, or control the detectors miss.
        return Pokemon::StoryFailure{.code = "scene_quiet", .detail = std::format(
            "{}: nothing busy for {} ms and no sign of control (expecting {}).", label, config.quiet ? config.quiet->count() : 0, expected
        )};
    }
    return std::nullopt;
}

std::optional<Pokemon::StoryFailure> scene_outcome(
    PlzaBackend& backend, std::string_view label, const SceneMachineConfig& config, const ScreenMachineResult& result
){
    const std::string expected = expected_end(config);
    if (const ScreenMachineFailure* failure = std::get_if<ScreenMachineFailure>(&result)){
        const std::string_view kind = failure->reason.starts_with("Did not finish") ? "scene_timeout" : "scene_stuck";
        return Pokemon::StoryFailure{.code = std::string(kind), .detail = std::format("{}: {} (expecting {}).", label, failure->reason, expected)};
    }
    const std::string& terminal = std::get<ScreenMachineTerminal>(result).state_name;
    if (std::optional<Pokemon::StoryFailure> failure = terminal_failure(label, config, terminal, expected)){
        return failure;
    }
    if (terminal == scene_screen_name(SceneScreen::QUIET_END)){
        backend.log(std::format("[PLZA {}] done: quiet, taken as control.", label));
        return std::nullopt;
    }
    if (terminal == scene_screen_name(SceneScreen::MISSION_BANNER)){
        wait_banner_gone(backend, label);
    }
    backend.log(std::format("[PLZA {}] done on {}.", label, terminal));
    return std::nullopt;
}

} // namespace


std::string describe_scene_look(const ScreenObservation& look){
    return describe_look(look);
}

bool text_shows(const ScreenObservation& look){
    return look.events.has(ScreenEvent::DIALOG_BOX) || look.readings.contains(ADVANCE_ARROW_READING);
}

bool fade_shows(const ScreenObservation& look){
    return look.events.has(ScreenEvent::FADE_BLACK) || look.events.has(ScreenEvent::FADE_WHITE);
}

bool party_bar_free(const ScreenObservation& look){
    return look.events.has(ScreenEvent::PLAYER_CAN_MOVE) && !look.events.has(ScreenEvent::DIALOG_BOX);
}

bool scene_under_way(const ScreenObservation& look){
    if (scene_shows(look) || look.readings.contains(TUTORIAL_CARD_READING)){
        return true;
    }
    if (hud_shows(look)){
        return false;
    }
    return fade_shows(look) || !look.events.has(ScreenEvent::SCREEN_STILL);
}

std::optional<Pokemon::StoryFailure> run_scene_machine(
    PlzaBackend& backend, std::string_view label, const SceneMachineConfig& config, Milliseconds timeout, std::string_view step
){
    backend.log(std::format("[PLZA {}] start, expecting {}.", label, expected_end(config)));
    //  The machine's first look is the one the timeline starts from.
    ScreenObservation look = backend.observe();
    bool first = true;
    SceneRunner runner(backend, label, config, look.when, step);
    const std::function<ScreenObservation()> observe = [&backend, &look, &first]() -> ScreenObservation{
        if (!first){
            look = backend.observe();
        }
        first = false;
        return look;
    };
    const ScreenMachineResult result = run_screen_machine(observe, backend.scope(), scene_states(look, runner), timeout, /*log=*/nullptr);
    runner.finish();
    if (config.end_after_black_fades){
        backend.log(std::format(
            "[PLZA {}] the mission banner {}.", label, runner.timeline().banner_seen() ? "showed" : "did not show (the detector may have missed it)"
        ));
    }
    return scene_outcome(backend, label, config, result);
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
