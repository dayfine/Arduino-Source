/*  PokemonLZA AutoStory - Scene machine replay Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Console runs of the scene machine replayed from their logs: each
 *  recorded look in order, whatever the machine presses (open loop). A
 *  guard or a press the screens do not call for shows as a wrong end here
 *  before it reaches the console.
 *
 */

#include <algorithm>
#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryFakeBackend.h"
#include "PokemonLZA_AutoStoryReadings.h"
#include "PokemonLZA_AutoStorySceneMachine.h"
#include "PokemonLZA_AutoStorySceneReplay_Tests.h"

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

Observation<ZonePose> look_at(Milliseconds at, ScreenEvents events = {}, std::vector<std::string_view> readings = {}){
    Observation<ZonePose> look;
    look.when = WallClock::min() + at;
    look.events = events;
    for (std::string_view reading : readings){
        look.readings[std::string(reading)] = 1;
    }
    return look;
}

//  01.01 arrival_scenes' config (mash_to_move_hints, end_after_black_fades
//  3, the action's defaults: a minute quiet).
SceneMachineConfig arrival_scenes_config(){
    SceneMachineConfig config;
    config.quiet = 60000ms;
    config.end_after_black_fades = 3;
    return config;
}

//  01.02 battle_area's config (mash_to_move_hints, min_length_ms 20000).
SceneMachineConfig battle_area_config(){
    SceneMachineConfig config;
    config.quiet = 60000ms;
    config.min_length = 20000ms;
    return config;
}

//  A "scene" step's config (scene_step(): the scene must start within 45 s).
SceneMachineConfig scene_step_config(){
    SceneMachineConfig config;
    config.start_window = 45000ms;
    return config;
}


//  01.01 arrival_scenes, passed (console B 2026-11-15 21:29, Italian).
constexpr std::string_view PASS_0101_IT_A = R"(
screen 1: fade_white at 0.0s: white fade, still
screen 2: dialog at 4.6s: text box, still
dialog x4 over 1.2s, 4 presses
screen 2: dialog at 6.6s: text box
dialog x11 over 4.1s, 11 presses
screen 2: dialog at 11.5s: text box, still
dialog x20 over 7.8s, 20 presses
screen 3: fade_white at 20.1s: white fade
screen 4: scene_moving at 20.9s: nothing
screen 5: blind_press at 25.4s: nothing
screen 6: scene_moving at 25.8s: nothing
scene_moving x50, blind_press x12 over 29.8s, 12 presses
screen 31: dialog at 56.6s: text box, advance arrow
screen 32: scene_moving at 57.0s: nothing
screen 33: dialog at 57.5s: text box
screen 33: dialog at 57.9s: text box, advance arrow
screen 34: scene_moving at 58.4s: nothing
screen 35: dialog at 61.4s: text box, advance arrow
screen 37: dialog at 62.8s: text box
screen 37: dialog at 63.2s: text box, advance arrow
screen 38: scene_moving at 63.6s: nothing
scene_moving x2, dialog x1 over 1.0s, 1 presses
screen 40: scene_moving at 65.5s: nothing
scene_moving x3, dialog x3 over 2.3s, 3 presses
screen 41: dialog at 68.8s: text box, advance arrow
screen 41: dialog at 69.2s: text box
dialog x4 over 1.2s, 4 presses
screen 42: scene_moving at 71.2s: nothing
screen 43: dialog at 73.3s: text box
dialog x10 over 3.7s, 10 presses
screen 44: scene_moving at 77.8s: nothing
scene_moving x16, dialog x6 over 10.2s, 6 presses
screen 49: dialog at 88.8s: text box, advance arrow
screen 49: dialog at 89.2s: text box
dialog x5 over 1.6s, 5 presses
screen 50: scene_moving at 91.7s: nothing
screen 51: dialog at 94.2s: text box
dialog x3 over 0.8s, 3 presses
screen 51: dialog at 95.8s: text box, advance arrow
screen 52: scene_moving at 96.2s: nothing
screen 53: dialog at 97.3s: text box
dialog x10, scene_moving x1 over 4.2s, 10 presses
screen 56: scene_moving at 102.2s: nothing
scene_moving x14, dialog x5 over 8.7s, 5 presses
screen 59: dialog at 111.9s: text box, advance arrow
screen 60: scene_moving at 112.3s: nothing
screen 61: fade_black at 113.3s: black fade, fade 1/3
screen 62: scene_still at 115.6s: still
screen 63: scene_moving at 116.1s: nothing
screen 64: scene_still at 117.7s: still
screen 65: blind_press at 118.4s: still
screen 66: scene_moving at 118.8s: nothing
screen 67: blind_press at 120.9s: nothing
screen 68: scene_moving at 121.3s: nothing
scene_moving x7, blind_press x1 over 3.4s, 1 presses
screen 71: dialog at 125.7s: advance arrow
screen 72: scene_moving at 126.1s: nothing
scene_moving x16, dialog x1 over 8.0s, 1 presses
screen 75: blind_press at 135.2s: nothing
screen 76: scene_moving at 135.6s: nothing
screen 77: dialog at 137.6s: advance arrow
screen 78: scene_moving at 138.0s: nothing
screen 79: blind_press at 143.1s: nothing
screen 80: scene_moving at 143.5s: nothing
scene_moving x6, blind_press x1 over 2.9s, 1 presses
screen 83: dialog at 147.5s: advance arrow
screen 84: scene_moving at 147.9s: nothing
scene_moving x19, dialog x1 over 9.6s, 1 presses
screen 87: blind_press at 158.5s: nothing
screen 88: scene_moving at 158.9s: nothing
scene_moving x13, blind_press x3 over 7.3s, 3 presses
screen 95: dialog at 167.2s: advance arrow
screen 96: scene_moving at 167.7s: nothing
scene_moving x37, dialog x3 over 19.5s, 3 presses
screen 103: blind_press at 188.2s: nothing
screen 104: scene_moving at 188.6s: nothing
screen 105: dialog at 189.1s: advance arrow
dialog x2 over 0.4s, 2 presses
screen 106: scene_moving at 190.3s: nothing
scene_moving x13, dialog x1 over 6.5s, 1 presses
screen 109: scene_still at 197.8s: still
screen 110: blind_press at 199.6s: still
screen 111: dialog at 200.0s: advance arrow
screen 112: scene_moving at 200.4s: nothing
screen 113: fade_black at 201.4s: black fade, fade 2/3
screen 114: blind_press at 211.4s: still
screen 115: dialog at 211.8s: text box
dialog x3 over 0.8s, 3 presses
screen 115: dialog at 213.5s: text box, advance arrow
screen 116: scene_moving at 213.9s: nothing
screen 117: dialog at 215.9s: text box
dialog x4 over 1.2s, 4 presses
screen 117: dialog at 217.9s: text box, advance arrow
scene_moving x1, dialog x2 over 0.9s, 2 presses
screen 120: scene_moving at 219.7s: nothing
screen 121: blind_press at 224.8s: nothing
screen 122: scene_moving at 225.2s: nothing
scene_moving x17, blind_press x4 over 9.8s, 4 presses
screen 131: dialog at 235.9s: text box
dialog x1 over 0.0s, 1 presses
screen 131: dialog at 236.8s: text box, advance arrow
screen 131: dialog at 237.2s: text box
screen 132: scene_moving at 237.6s: nothing
screen 133: dialog at 239.6s: text box
dialog x5, scene_moving x4 over 3.7s, 5 presses
screen 135: dialog at 244.1s: text box, advance arrow
screen 136: scene_moving at 244.5s: nothing
screen 137: banner_showing at 245.5s: mission banner
screen 138: banner_press at 247.6s: mission banner
screen 139: scene_moving at 248.0s: nothing
screen 140: fade_black at 252.6s: black fade, fade 3/3
screen 141: scene_moving at 255.4s: nothing
screen 142: hint_showing at 256.4s: controls hint
screen 143: controls_hint at 257.4s: controls hint
)";

//  01.01 arrival_scenes, passed (console B 2026-11-15 21:55, Italian).
constexpr std::string_view PASS_0101_IT_B = R"(
screen 1: fade_white at 0.0s: white fade, still
screen 2: dialog at 4.3s: text box, still
dialog x5 over 1.6s, 5 presses
screen 2: dialog at 6.8s: text box
dialog x6 over 2.0s, 6 presses
screen 2: dialog at 9.7s: text box, still
dialog x24 over 9.4s, 24 presses
screen 3: fade_white at 19.9s: white fade
screen 4: dialog at 20.6s: text box, still
screen 5: scene_moving at 21.0s: nothing
screen 6: blind_press at 26.1s: nothing
screen 7: scene_moving at 26.5s: nothing
scene_moving x48, blind_press x12 over 28.8s, 12 presses
screen 32: dialog at 56.3s: text box
dialog x3 over 0.8s, 3 presses
screen 33: scene_moving at 57.9s: nothing
dialog x2, scene_moving x6 over 3.3s, 2 presses
screen 36: dialog at 62.3s: text box, advance arrow
screen 37: scene_moving at 62.7s: nothing
screen 38: dialog at 63.7s: text box
screen 38: dialog at 64.1s: text box, advance arrow
screen 39: scene_moving at 64.5s: nothing
scene_moving x2, dialog x1 over 1.0s, 1 presses
screen 41: scene_moving at 66.4s: nothing
scene_moving x3, dialog x4 over 2.7s, 4 presses
screen 42: dialog at 70.1s: text box, advance arrow
screen 42: dialog at 70.5s: text box
dialog x3 over 0.8s, 3 presses
screen 43: scene_moving at 72.1s: nothing
screen 44: dialog at 74.2s: text box
dialog x10 over 3.7s, 10 presses
screen 45: scene_moving at 78.6s: nothing
scene_moving x16, dialog x7 over 10.6s, 7 presses
screen 50: dialog at 90.1s: text box, advance arrow
screen 50: dialog at 90.5s: text box
dialog x4 over 1.2s, 4 presses
screen 51: scene_moving at 92.6s: nothing
screen 52: dialog at 95.1s: text box
dialog x3 over 0.8s, 3 presses
screen 53: scene_moving at 96.7s: nothing
scene_moving x1, dialog x7 over 3.0s, 7 presses
screen 54: dialog at 100.6s: text box, advance arrow
screen 55: scene_moving at 101.0s: nothing
screen 56: dialog at 101.5s: text box
dialog x3 over 0.8s, 3 presses
screen 57: scene_moving at 103.2s: nothing
scene_moving x16, dialog x6 over 10.1s, 6 presses
screen 62: fade_black at 114.2s: black fade, fade 1/3
screen 63: scene_still at 116.5s: still
screen 64: blind_press at 119.4s: still
screen 65: scene_moving at 119.8s: nothing
screen 66: blind_press at 121.8s: nothing
screen 67: scene_moving at 122.2s: nothing
scene_moving x7, blind_press x1 over 3.5s, 1 presses
screen 70: dialog at 126.7s: advance arrow
screen 71: scene_moving at 127.1s: nothing
scene_moving x16, dialog x1 over 8.0s, 1 presses
screen 74: blind_press at 136.1s: nothing
screen 75: scene_moving at 136.6s: nothing
screen 76: dialog at 138.6s: advance arrow
screen 77: scene_moving at 139.0s: nothing
screen 78: blind_press at 143.6s: nothing
screen 79: scene_moving at 144.0s: nothing
scene_moving x6, blind_press x1 over 3.0s, 1 presses
screen 82: dialog at 148.0s: advance arrow
screen 83: scene_moving at 148.4s: nothing
screen 84: blind_press at 153.1s: nothing
screen 85: dialog at 153.5s: advance arrow
screen 86: scene_moving at 153.9s: nothing
scene_moving x8, blind_press x1 over 4.1s, 1 presses
screen 88: scene_moving at 159.0s: nothing
screen 89: dialog at 159.5s: advance arrow
screen 90: scene_moving at 159.9s: nothing
screen 91: blind_press at 164.5s: nothing
screen 92: scene_moving at 164.9s: nothing
scene_moving x5, blind_press x1 over 2.5s, 1 presses
screen 95: dialog at 168.4s: advance arrow
screen 96: scene_moving at 168.9s: nothing
scene_moving x16, dialog x1 over 8.2s, 1 presses
screen 99: blind_press at 178.0s: nothing
screen 100: scene_moving at 178.5s: nothing
scene_moving x7, blind_press x2 over 4.0s, 2 presses
screen 104: dialog at 183.4s: advance arrow
screen 105: scene_moving at 183.8s: nothing
screen 106: blind_press at 188.4s: nothing
screen 107: scene_moving at 188.8s: nothing
screen 108: dialog at 189.8s: advance arrow
dialog x2 over 0.4s, 2 presses
screen 109: scene_moving at 191.1s: nothing
scene_moving x13, dialog x1 over 6.6s, 1 presses
screen 112: scene_still at 198.7s: still
screen 113: blind_press at 200.2s: still
screen 114: scene_still at 200.7s: still
screen 115: dialog at 200.9s: still, advance arrow
screen 116: scene_moving at 201.3s: nothing
screen 117: fade_black at 202.3s: black fade, fade 2/3
screen 118: blind_press at 212.2s: still
screen 119: dialog at 212.6s: text box
dialog x3 over 0.8s, 3 presses
screen 120: scene_moving at 214.2s: nothing
screen 121: dialog at 216.3s: text box
dialog x2 over 0.4s, 2 presses
screen 121: dialog at 217.5s: text box, advance arrow
screen 121: dialog at 217.9s: text box
dialog x1 over 0.0s, 1 presses
screen 122: scene_moving at 218.8s: nothing
screen 123: dialog at 219.3s: text box
dialog x1 over 0.0s, 1 presses
screen 123: dialog at 220.1s: text box, advance arrow
dialog x1 over 0.0s, 1 presses
screen 124: scene_moving at 220.9s: nothing
screen 125: blind_press at 225.6s: nothing
screen 126: scene_moving at 226.0s: nothing
scene_moving x16, blind_press x4 over 9.5s, 4 presses
screen 135: dialog at 236.5s: text box
screen 135: dialog at 236.9s: text box, advance arrow
screen 135: dialog at 237.3s: text box
dialog x3 over 0.8s, 3 presses
screen 136: scene_moving at 239.0s: nothing
screen 137: dialog at 241.0s: text box
dialog x5, scene_moving x6 over 4.6s, 5 presses
screen 141: banner_showing at 246.6s: mission banner
screen 142: banner_press at 248.7s: mission banner
screen 143: scene_moving at 249.1s: nothing
screen 144: fade_black at 253.7s: black fade, fade 3/3
screen 145: scene_still at 256.5s: still
screen 146: hint_showing at 257.1s: still, controls hint
screen 146: hint_showing at 257.4s: controls hint
screen 147: controls_hint at 258.1s: controls hint
)";

//  01.01 arrival_scenes up to the scene_loop guard's false stop at
//  Jacinthe's subtitles (console B 2026-11-15 22:27, Italian).
constexpr std::string_view FAIL_0101_IT_SUBTITLES = R"(
screen 1: fade_white at 0.0s: white fade, still
screen 2: dialog at 4.3s: text box, still
dialog x3 over 0.8s, 3 presses
screen 2: dialog at 6.0s: text box
screen 2: dialog at 6.4s: text box, still
dialog x32 over 12.7s, 32 presses
screen 3: fade_white at 19.9s: white fade
screen 4: scene_still at 20.7s: still
screen 5: scene_moving at 22.0s: nothing
screen 6: blind_press at 25.0s: nothing
screen 7: scene_moving at 25.4s: nothing
scene_moving x50, blind_press x12 over 30.0s, 12 presses
screen 32: dialog at 56.4s: text box
dialog x2 over 0.4s, 2 presses
screen 32: dialog at 57.7s: text box, advance arrow
screen 33: scene_moving at 58.1s: nothing
screen 34: dialog at 58.6s: text box
dialog x1 over 0.0s, 1 presses
screen 35: scene_moving at 59.4s: nothing
screen 36: dialog at 62.4s: text box, advance arrow
screen 37: scene_moving at 62.9s: nothing
screen 38: dialog at 63.9s: text box
screen 38: dialog at 64.3s: text box, advance arrow
screen 39: scene_moving at 64.7s: nothing
scene_moving x2, dialog x1 over 1.0s, 1 presses
screen 41: scene_moving at 66.6s: nothing
scene_moving x3, dialog x4 over 2.8s, 4 presses
screen 42: dialog at 70.3s: text box, advance arrow
screen 42: dialog at 70.7s: text box
dialog x3 over 0.8s, 3 presses
screen 43: scene_moving at 72.3s: nothing
screen 44: dialog at 74.4s: text box
dialog x17, scene_moving x17 over 15.2s, 17 presses
screen 50: dialog at 90.4s: text box, advance arrow
screen 50: dialog at 90.8s: text box
dialog x4 over 1.2s, 4 presses
screen 51: scene_moving at 92.8s: nothing
screen 52: dialog at 95.3s: text box
dialog x3 over 0.8s, 3 presses
screen 53: scene_moving at 97.0s: nothing
scene_moving x1, dialog x7 over 3.0s, 7 presses
screen 54: dialog at 100.8s: text box, advance arrow
screen 55: scene_moving at 101.3s: nothing
screen 56: dialog at 101.8s: text box
dialog x3 over 0.8s, 3 presses
screen 57: scene_moving at 103.4s: nothing
scene_moving x16, dialog x6 over 10.0s, 6 presses
screen 62: fade_black at 114.4s: black fade, fade 1/3
screen 63: scene_moving at 116.8s: nothing
screen 64: scene_still at 118.3s: still
screen 65: blind_press at 119.6s: still
screen 66: scene_moving at 120.0s: nothing
screen 67: blind_press at 122.0s: nothing
screen 68: scene_moving at 122.4s: nothing
scene_moving x7, blind_press x1 over 3.5s, 1 presses
screen 71: dialog at 126.9s: advance arrow
screen 72: scene_moving at 127.3s: nothing
scene_moving x16, dialog x1 over 8.0s, 1 presses
screen 75: blind_press at 136.3s: nothing
screen 76: scene_moving at 136.7s: nothing
screen 77: dialog at 138.8s: advance arrow
screen 78: scene_moving at 139.2s: nothing
screen 79: blind_press at 144.3s: nothing
screen 80: scene_moving at 144.7s: nothing
scene_moving x6, blind_press x1 over 2.9s, 1 presses
screen 83: dialog at 148.6s: advance arrow
screen 84: scene_moving at 149.1s: nothing
screen 85: blind_press at 153.7s: nothing
screen 86: dialog at 154.1s: advance arrow
)";

//  01.02 battle_area's first 50 s, blind A at Ryon's "A Parla" prompt
//  restarting his talk (console B 2026-11-15 21:56, Italian).
constexpr std::string_view LOOP_0102_IT_TALK_PROMPT = R"(
screen 1: dialog at 0.0s: text box
dialog x2 over 0.4s, 2 presses
screen 1: dialog at 1.2s: text box, advance arrow
screen 2: scene_moving at 1.7s: nothing
screen 3: dialog at 2.2s: text box
dialog x1 over 0.0s, 1 presses
screen 3: dialog at 3.0s: text box, advance arrow
scene_moving x1, dialog x4 over 1.8s, 4 presses
screen 6: scene_moving at 5.6s: nothing
screen 7: fade_black at 7.1s: black fade, fade 1
screen 8: blind_press at 12.8s: still
screen 9: scene_moving at 13.2s: nothing
screen 9: scene_moving at 13.7s: A prompt
screen 10: blind_press at 15.3s: A prompt
screen 11: scene_moving at 15.7s: nothing
screen 12: dialog at 16.2s: text box
dialog x1 over 0.0s, 1 presses
screen 12: dialog at 17.0s: text box, advance arrow
screen 13: scene_moving at 17.4s: nothing
screen 13: scene_moving at 18.4s: A prompt
screen 14: blind_press at 22.0s: A prompt
screen 15: scene_moving at 22.4s: nothing
screen 16: dialog at 22.9s: text box
dialog x1 over 0.0s, 1 presses
screen 16: dialog at 23.8s: text box, advance arrow
screen 17: scene_moving at 24.2s: nothing
screen 17: scene_moving at 25.2s: A prompt
screen 18: blind_press at 28.8s: A prompt
screen 19: scene_moving at 29.3s: nothing
screen 20: dialog at 29.8s: text box
dialog x1 over 0.0s, 1 presses
screen 20: dialog at 30.6s: text box, advance arrow
screen 21: scene_moving at 31.1s: nothing
screen 21: scene_moving at 32.1s: A prompt
screen 22: blind_press at 35.7s: A prompt
screen 23: scene_moving at 36.2s: nothing
screen 24: dialog at 36.7s: text box
dialog x1 over 0.0s, 1 presses
screen 24: dialog at 37.5s: text box, advance arrow
screen 25: scene_moving at 37.9s: nothing
)";

//  01.02 battle_area, passed (console B 2026-11-13 12:02, English). That
//  day's log put the readings on lines of their own, without the hint's:
//  these lines carry them, the hint's from its state.
constexpr std::string_view PASS_0102_EN = R"(
screen 1: dialog at 0.0s: text box
screen 2: scene_moving at 1.8s: nothing
screen 3: dialog at 2.4s: text box
screen 4: scene_moving at 3.3s: nothing
screen 5: dialog at 3.8s: text box
screen 6: scene_moving at 5.1s: nothing
screen 7: fade_black at 6.7s: black fade, fade 1
screen 8: blind_press at 12.4s: nothing
screen 9: hint_showing at 12.9s: controls hint
screen 10: controls_hint at 14.0s: controls hint
)";

//  02.02 street_scene, failed as scene_never_started (console B 2026-11-15
//  23:23, English): a Pokemon with a red staff drives the trainers off in
//  a cutscene with no text and no HUD for 41 s after the black fade.
constexpr std::string_view FAIL_0202_STREET_CUTSCENE = R"(
screen 1: fade_black at 0.0s: black fade, still, fade 1
screen 2: scene_still at 3.8s: still
screen 3: scene_moving at 4.9s: nothing
scene_moving x61 over 30.5s, 0 presses
screen 4: never_started at 45.0s: nothing
)";

//  The looks of one screen in a scene machine's log: the state's events
//  and readings by their names in the log ("text box", "advance arrow";
//  describe_look()). The fade count ("fade 1/3") is the machine's own.
Observation<ZonePose> look_from_log(Milliseconds at, std::string_view readings){
    const std::pair<std::string_view, ScreenEvent> events[] = {
        {"black fade", ScreenEvent::FADE_BLACK}, {"white fade", ScreenEvent::FADE_WHITE},
        {"text box", ScreenEvent::DIALOG_BOX}, {"choice box", ScreenEvent::CHOICE_BOX},
        {"battle", ScreenEvent::BATTLE_STARTED}, {"party bar", ScreenEvent::PLAYER_CAN_MOVE},
        {"A prompt", ScreenEvent::INTERACT_PROMPT}, {"still", ScreenEvent::SCREEN_STILL},
    };
    const std::pair<std::string_view, std::string_view> named_readings[] = {
        {"controls hint", CONTROLS_HINT_READING}, {"advance arrow", ADVANCE_ARROW_READING},
        {"mission banner", MISSION_BANNER_READING}, {"mission complete", MISSION_COMPLETE_READING},
    };
    Observation<ZonePose> look = look_at(at);
    size_t from = 0;
    while (from < readings.size()){
        const size_t comma = std::min(readings.find(", ", from), readings.size());
        const std::string_view name = readings.substr(from, comma - from);
        from = comma + 2;
        for (const auto& [event_name, event] : events){
            if (name == event_name){
                look.events.set(event, true);
            }
        }
        for (const auto& [reading_name, reading] : named_readings){
            if (name == reading_name){
                look.readings[std::string(reading)] = 1;
            }
        }
    }
    return look;
}

//  What a look summed up as `state` showed: the screen the state is named
//  for ("dialog" a text box).
std::string_view readings_of_state(std::string_view state){
    const std::pair<std::string_view, std::string_view> states[] = {
        {"dialog", "text box"}, {"scene_still", "still"}, {"fade_black", "black fade"}, {"fade_white", "white fade"},
        {"banner_showing", "mission banner"}, {"banner_press", "mission banner"}, {"hint_showing", "controls hint"},
        {"prompt_showing", "A prompt"}, {"party_bar", "party bar"}, {"mission_complete", "mission complete"},
    };
    for (const auto& [name, readings] : states){
        if (name == state){
            return readings;
        }
    }
    return "nothing";
}

Milliseconds seconds_in_log(std::string_view text){
    return Milliseconds((int64_t)(std::stod(std::string(text)) * 1000 + 0.5));
}

//  The looks of a console run from its scene machine's log, one line each
//  ("screen 3: scene_still at 12.5s: still"), the looks a line sums up
//  ("dialog x3, scene_moving x2 over 1.2s, 3 presses") spread over its time
//  in turns. Opened loop: the run's presses are not replayed, the looks
//  come as recorded whatever the machine presses.
std::vector<Observation<ZonePose>> replay_log(std::string_view log){
    std::vector<Observation<ZonePose>> looks;
    Milliseconds last = 0ms;
    size_t from = 0;
    while (from < log.size()){
        const size_t end = std::min(log.find('\n', from), log.size());
        const std::string_view line = log.substr(from, end - from);
        from = end + 1;
        if (line.starts_with("screen ")){
            const size_t name = line.find(": ") + 2;
            const size_t at = line.find(" at ", name) + 4;
            const size_t seconds_end = line.find("s: ", at);
            last = seconds_in_log(line.substr(at, seconds_end - at));
            looks.push_back(look_from_log(last, line.substr(seconds_end + 3)));
            continue;
        }
        const size_t over = line.find(" over ");
        if (over == std::string_view::npos){
            continue;
        }
        std::vector<std::pair<std::string_view, size_t>> counts;
        size_t total = 0;
        size_t item = 0;
        while (item < over){
            const size_t item_end = std::min(line.find(", ", item), over);
            const size_t times = line.find(" x", item);
            const size_t count = (size_t)std::stoul(std::string(line.substr(times + 2, item_end - times - 2)));
            counts.emplace_back(line.substr(item, times - item), count);
            total += count;
            item = item_end + 2;
        }
        const Milliseconds span = seconds_in_log(line.substr(over + 6, line.find("s, ", over) - over - 6));
        const Milliseconds step = std::max(span / (int64_t)std::max<size_t>(total, 1), 10ms);
        for (size_t turn = 0; turn < total;){
            for (auto& [state, left] : counts){
                if (left > 0){
                    left--;
                    turn++;
                    last += step;
                    looks.push_back(look_from_log(last, readings_of_state(state)));
                }
            }
        }
    }
    return looks;
}

//  The looks of `log`, then `then`.
std::vector<Observation<ZonePose>> replay_then(std::string_view log, std::vector<Observation<ZonePose>> then){
    std::vector<Observation<ZonePose>> looks = replay_log(log);
    for (Observation<ZonePose>& look : then){
        looks.push_back(std::move(look));
    }
    return looks;
}

//  Recorded console runs replayed: the machine must reach the state the
//  plan's step ends on, with no press the screen did not call for.
struct ReplayCase{
    std::string name;
    struct Input{
        std::vector<Observation<ZonePose>> looks;
        SceneMachineConfig config;
    };
    Input input;
    struct Expectation{
        //  "done", or "failed: <failure>".
        std::string verdict;
        //  The run's last line: "[PLZA test] done on controls_hint.".
        std::string end;
    };
    Expectation expectation;
};

const ReplayCase REPLAY_CASES[] = {
    {
        .name = "01.01 arrival_scenes, Italian pass at 21:29",
        .input = {.looks = replay_log(PASS_0101_IT_A), .config = arrival_scenes_config()},
        .expectation = {.verdict = "done", .end = "[PLZA test] done on controls_hint."},
    },
    {
        .name = "01.01 arrival_scenes, Italian pass at 21:55",
        .input = {.looks = replay_log(PASS_0101_IT_B), .config = arrival_scenes_config()},
        .expectation = {.verdict = "done", .end = "[PLZA test] done on controls_hint."},
    },
    {
        //  The scene_loop guard stopped it at 154.5 s; the run goes on to
        //  the controls hint.
        .name = "01.01 arrival_scenes, Italian, Jacinthe's subtitles each after a quiet spell, then the controls hint",
        .input = {
            .looks = replay_then(FAIL_0101_IT_SUBTITLES, {
                look_at(/*at=*/155000ms, /*events=*/{}, {CONTROLS_HINT_READING}),
                look_at(/*at=*/156100ms, /*events=*/{}, {CONTROLS_HINT_READING}),
            }),
            .config = arrival_scenes_config(),
        },
        .expectation = {.verdict = "done", .end = "[PLZA test] done on controls_hint."},
    },
    {
        .name = "01.02 battle_area, Italian, the A Parla prompt past 20 s ends the step",
        .input = {.looks = replay_log(LOOP_0102_IT_TALK_PROMPT), .config = battle_area_config()},
        .expectation = {.verdict = "done", .end = "[PLZA test] done on talk_prompt."},
    },
    {
        .name = "01.02 battle_area, English pass on 2026-11-13: the controls hint ends the step",
        .input = {.looks = replay_log(PASS_0102_EN), .config = battle_area_config()},
        .expectation = {.verdict = "done", .end = "[PLZA test] done on controls_hint."},
    },
    {
        //  The run stopped at 45.0 s; the controls hint after the cutscene
        //  is synthetic (the run never got there).
        .name = "02.02 street_scene, English, a textless cutscene past the start window, then the controls hint",
        .input = {
            .looks = replay_then(FAIL_0202_STREET_CUTSCENE, {
                look_at(/*at=*/46000ms, /*events=*/{}, {CONTROLS_HINT_READING}),
                look_at(/*at=*/47100ms, /*events=*/{}, {CONTROLS_HINT_READING}),
            }),
            .config = scene_step_config(),
        },
        .expectation = {.verdict = "done", .end = "[PLZA test] done on controls_hint."},
    },
};

UnitTestResult check_replay(const ReplayCase& test, CancellableScope& scope){
    Pokemon::FakeStoryBackend<ZonePose> backend({.looks = test.input.looks}, scope);
    const Pokemon::Outcome outcome = run_scene_machine(backend, "test", test.input.config, 360s);
    const std::string verdict = outcome.succeeded() ? "done" : std::format("failed: {}", outcome.failure);
    if (verdict != test.expectation.verdict){
        return std::format("expected \"{}\", got \"{}\"; log:\n{}", test.expectation.verdict, verdict, joined(backend.lines(), "\n"));
    }
    if (backend.lines().empty() || backend.lines().back() != test.expectation.end){
        return std::format("expected the run to end with \"{}\"; log:\n{}", test.expectation.end, joined(backend.lines(), "\n"));
    }
    return true;
}

class ReplayTest : public UnitTest{
public:
    explicit ReplayTest(const ReplayCase& test)
        : UnitTest(std::format("PokemonLZA::AutoStorySceneMachine replay - {}", test.name))
        , m_test(test)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope& scope) const override{
        return check_replay(m_test, scope);
    }
private:
    const ReplayCase& m_test;
};

} // namespace


void add_tests_AutoStorySceneReplay(UnitTestDatabase& database){
    for (const ReplayCase& test : REPLAY_CASES){
        database.add<ReplayTest>(test);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
