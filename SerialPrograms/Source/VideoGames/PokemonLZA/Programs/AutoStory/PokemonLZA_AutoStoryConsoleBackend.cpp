/*  PokemonLZA AutoStory - Console backend
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cmath>
#include <numbers>
#include <optional>
#include <string>
#include <utility>
#include "Common/Cpp/Color.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/VideoPipeline/VideoOverlay.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "Pokemon/AutoStory/Engine/Pokemon_ScreenObservation.h"
#include "VideoGames/PokemonLZA/Inference/Battles/PokemonLZA_MoveEffectivenessSymbol.h"
#include "VideoGames/PokemonLZA/Programs/PokemonLZA_BasicNavigation.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryDialogChoiceBox.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryTutorialCardDetector.h"
#include "PokemonLZA_AutoStoryMenuReadings.h"
#include "PokemonLZA_AutoStoryReadings.h"
#include "PokemonLZA_AutoStoryConsoleBackend.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;


PlzaConsoleBackend::PlzaConsoleBackend(StoryEnv& env)
    : m_env(env)
    , m_home(env.env.console, COLOR_RED)
    , m_shadow_dialog_box({.white = WhiteDialogPart::BOX_ONLY})
    , m_save_button(COLOR_GREEN, nullptr, SelectionArrowType::RIGHT, main_menu_save_arrow_box())
{}

void PlzaConsoleBackend::press(Button button, Milliseconds hold, Milliseconds release){
    m_run_end.reset();
    pbf_press_button(m_env.context, button, hold, release);
}

//  Console B 2026-11-19, measured on the full map at 03.04's start: walking
//  covers 16.6 px/s and running 27.0 px/s, linear from 0.4 s to 4 s.
constexpr double RUN_SPEEDUP = 1.65;

void PlzaConsoleBackend::stick(double degrees, Milliseconds duration, Milliseconds release, Pokemon::StoryPace pace){
    const double radians = degrees * std::numbers::pi / 180.0;
    const JoystickPosition position{std::sin(radians), std::cos(radians)};
    if (pace != Pokemon::StoryPace::RUN || m_env.walk_only){
        m_run_end.reset();
        pbf_move_left_joystick(m_env.context, position, duration, release);
        return;
    }
    const Milliseconds run(static_cast<int64_t>(duration.count() / RUN_SPEEDUP));
    const WallClock now = current_time();
    const WallClock start = m_run_end && *m_run_end > now ? *m_run_end : now;
    pbf_controller_state(m_env.context, BUTTON_B, DPAD_NONE, position, /*right_joystick=*/{}, run);
    if (release > 0ms){
        pbf_wait(m_env.context, release);
        m_run_end.reset();
    }else{
        m_run_end = start + run;
    }
}

void PlzaConsoleBackend::wait(Milliseconds duration){
    m_env.context.wait_for_all_requests();
    m_run_end.reset();
    m_env.context.wait_for(duration);
}

void PlzaConsoleBackend::wait_for_inputs(){
    //  Look before the queued run ends, with time left to queue the next slice.
    if (m_run_end && *m_run_end - 80ms > current_time()){
        m_env.context.wait_until(*m_run_end - 80ms);
    }else if (!m_run_end || *m_run_end <= current_time()){
        m_env.context.wait_for_all_requests();
        m_run_end.reset();
    }
}

ScreenObservation PlzaConsoleBackend::observe(){
    wait_for_inputs();
    VideoSnapshot frame = m_env.env.console.video().snapshot();
    ScreenObservation look;
    if (frame && m_home.detect(*frame.frame)){
        look.events.set(ScreenEvent::SWITCH_HOME_MENU, true);
    }else if (frame){
        look = Pokemon::screen_observation(m_observer.read(*frame.frame));
        if (m_autosave.detect(*frame.frame)){
            look.readings[std::string(AUTOSAVE_READING)] = 1;
        }
        if (m_controls_hint.detect(*frame.frame)){
            look.readings[std::string(CONTROLS_HINT_READING)] = 1;
        }
        if (m_advance_arrow.detect(*frame.frame)){
            look.readings[std::string(ADVANCE_ARROW_READING)] = 1;
        }
        if (m_shadow_dialog_box.detect(*frame.frame)){
            look.readings[std::string(SHADOW_DIALOG_BOX_READING)] = 1;
        }
        if (m_mission_complete.detect(*frame.frame)){
            look.readings[std::string(MISSION_COMPLETE_READING)] = 1;
        }else if (m_mission_banner.detect(*frame.frame)){
            look.readings[std::string(MISSION_BANNER_READING)] = 1;
        }
        if (tutorial_card_shows(*frame.frame)){
            look.readings[std::string(TUTORIAL_CARD_READING)] = 1;
        }
        read_menus(*frame.frame, look);
        if (look.events.has(ScreenEvent::CHOICE_BOX)){
            const std::optional<DialogChoiceRows> rows = read_dialog_choice_rows(*frame.frame);
            if (rows){
                look.readings[std::string(CHOICE_ROWS_READING)] = (int)rows->count;
                look.readings[std::string(CHOICE_SELECTED_READING)] = (int)rows->selected;
            }
        }
        if (same_screen(m_previous, frame)){
            look.events.set(ScreenEvent::SCREEN_STILL, true);
        }
    }
    look.when = current_time();
    m_previous = frame;
    look.frame = std::move(frame);
    return look;
}

void PlzaConsoleBackend::read_menus(const ImageViewRGB32& frame, ScreenObservation& look){
    if (m_main_menu.detect(frame)){
        look.readings[std::string(MAIN_MENU_READING)] = 1;
    }
    if (m_save_button.detect(frame)){
        look.readings[std::string(MENU_SAVE_BUTTON_READING)] = 1;
    }
    if (m_map.detect(frame)){
        look.readings[std::string(MAP_READING)] = 1;
    }
}

void PlzaConsoleBackend::log(std::string_view line){
    m_env.env.console.log(std::string(line), COLOR_BLUE);
    m_env.env.console.overlay().add_log(std::string(line));
}

void PlzaConsoleBackend::log_detail(std::string_view line){
    m_env.env.console.log(std::string(line), COLOR_BLUE);
}

CancellableScope& PlzaConsoleBackend::scope(){
    return m_env.context;
}


MinimapArrowReader minimap_arrow(StoryEnv& env){
    return [&env](){
        return get_facing_direction(env.env.console, env.context);
    };
}

LockOnWatch move_symbols_lock_on(StoryEnv& env){
    return [&env](Milliseconds hold){
        MoveEffectivenessSymbolWatcher symbols(COLOR_RED, &env.env.console.overlay(), /*hold_duration=*/100ms);
        env.context.wait_for_all_requests();
        const int fired = run_until<ProControllerContext>(
            env.env.console, env.context,
            [hold](ProControllerContext& context){
                pbf_press_button(context, BUTTON_ZL, hold, /*release=*/0ms);
            },
            {symbols}
        );
        env.context.wait_for_all_requests();
        return fired == 0;
    };
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
