/*  AutoStory Launch
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <chrono>
#include <format>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/VisualDetectors/BlackScreenDetector.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "NintendoSwitch/Inference/NintendoSwitch_CheckOnlineDetector.h"
#include "NintendoSwitch/Inference/NintendoSwitch_HomeMenuDetector.h"
#include "Pokemon/AutoStory/Pokemon_StorySwitchPrompts.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_LoadMenuDetector.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Settings.h"
#include "PokemonFRLG_AutoStoryPresses.h"
#include "PokemonFRLG_AutoStoryScreenEvents.h"
#include "PokemonFRLG_AutoStoryLaunch.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using namespace std::chrono_literals;
using Pokemon::StoryLaunchPolicy;
using Pokemon::StoryLaunchPrompt;


namespace{

constexpr Milliseconds RECAP_TIMEOUT = 20000ms;

bool shows(const ScreenObservation& look, std::string_view reading){
    const auto it = look.readings.find(reading);
    return it != look.readings.end() && it->second != 0;
}

bool quiet(const ScreenObservation& look){
    for (ScreenEvent event : {
        ScreenEvent::FADE_BLACK, ScreenEvent::FADE_WHITE, ScreenEvent::DIALOG_BOX,
        ScreenEvent::CHOICE_BOX, ScreenEvent::BATTLE_STARTED,
    }){
        if (look.events.has(event)){
            return false;
        }
    }
    return true;
}

struct ContinueChosen{
    bool chosen = false;
};

std::function<bool(const ScreenObservation&)> showing(std::string_view reading){
    return [reading](const ScreenObservation& look){
        return shows(look, reading);
    };
}

StoryLaunchPrompt license_check_prompt(){
    return {.name = "license check", .matches = showing(FrlgLaunchReadings::CHECK_ONLINE),
            .press = {.button = BUTTON_A, .hold = 160ms, .release = 1840ms}};
}


//  The console, for the launch alone (#648 concrete StoryBackend).
class FrlgLaunchBackend : public Pokemon::StoryBackend{
public:
    FrlgLaunchBackend(ConsoleHandle& console, ProControllerContext& context)
        : m_console(console)
        , m_context(context)
        , m_home(console, COLOR_RED)
        , m_black(COLOR_BLUE, {0.1, 0.15, 0.8, 0.7})
        , m_load_menu(COLOR_BLUE)
    {}

    virtual void press(Button button, Milliseconds hold, Milliseconds release) override{
        pbf_press_button(m_context, button, hold, release);
    }
    virtual void stick(double, Milliseconds, Milliseconds, Pokemon::StoryPace) override{}
    virtual void wait(Milliseconds duration) override{
        m_context.wait_for(duration);
    }
    virtual ScreenObservation observe() override;
    virtual void log(std::string_view line) override{
        m_console.log(std::string(line), COLOR_BLUE);
    }
    virtual CancellableScope& scope() override{
        return m_context;
    }

private:
    void read_prompts(const ImageViewRGB32& frame, ScreenObservation& look);

    ConsoleHandle& m_console;
    ProControllerContext& m_context;
    HomeMenuDetector m_home;
    BlackScreenDetector m_black;
    CheckOnlineDetector m_check_online;
    LoadMenuDetector m_load_menu;
    FrlgScreenObserver m_observer;
};

void FrlgLaunchBackend::read_prompts(const ImageViewRGB32& frame, ScreenObservation& look){
    const std::pair<std::string_view, bool> readings[] = {
        {FrlgLaunchReadings::CHECK_ONLINE, m_check_online.detect(frame)},
        {FrlgLaunchReadings::LOAD_MENU, m_load_menu.detect(frame)},
    };
    for (const auto& [reading, shown] : readings){
        if (shown){
            look.readings[std::string(reading)] = 1;
        }
    }
}

ScreenObservation FrlgLaunchBackend::observe(){
    m_context.wait_for_all_requests();
    VideoSnapshot frame = m_console.video().snapshot();
    ScreenObservation look;
    if (frame && m_black.detect(*frame.frame)){
        look.events.set(ScreenEvent::FADE_BLACK, true);
    }else if (frame && m_home.detect(*frame.frame)){
        look.events.set(ScreenEvent::SWITCH_HOME_MENU, true);
    }else if (frame){
        look.events = m_observer.observe(frame, current_time());
        read_prompts(*frame.frame, look);
    }else{
        look.events.set(ScreenEvent::FADE_BLACK, true);
    }
    look.when = current_time();
    look.frame = std::move(frame);
    return look;
}

} // namespace


StoryLaunchPolicy frlg_launch_policy(
    std::vector<StoryLaunchPrompt> switch_prompts,
    const FrlgLaunchSettings& settings
){
    const auto chosen = std::make_shared<ContinueChosen>();
    StoryLaunchPolicy policy;
    policy.game = "frlg";
    policy.tile_selected = Pokemon::switch_home_tile_named(Language::English, "Pokémon FireRed Version");
    policy.title = [chosen](const ScreenObservation& look){
        return !chosen->chosen && quiet(look);
    };
    policy.title_after_launch = true;
    policy.title_press = {.button = BUTTON_A, .hold = 160ms, .release = 1000ms};
    policy.prompts = std::move(switch_prompts);
    if (settings.handle_online_license_check){
        policy.prompts.push_back(license_check_prompt());
    }
    policy.prompts.push_back({
        .name = "load menu",
        .matches = [chosen](const ScreenObservation& look){
            if (!shows(look, FrlgLaunchReadings::LOAD_MENU)){
                return false;
            }
            chosen->chosen = true;
            return true;
        },
        .press = {.button = BUTTON_A, .hold = 160ms, .release = 1000ms},
    });
    policy.prompts.push_back({
        .name = "recap",
        .matches = [chosen](const ScreenObservation& look){
            return chosen->chosen && look.events.has(ScreenEvent::DIALOG_BOX);
        },
        .press = {.button = BUTTON_B, .hold = 160ms, .release = 400ms},
        .after_launch = true,
        .max_presses = 20,
    });
    policy.overworld = [chosen](const ScreenObservation& look){
        return chosen->chosen && quiet(look);
    };
    policy.settled_looks = 4;
    policy.presses_per_screen = 12;
    return policy;
}

namespace{

std::optional<Pokemon::StoryFailure> launch_frlg_save_emulator(ConsoleHandle& console, ProControllerContext& context){
    console.log("Emulator device selected: soft-resetting GBA to reload save.", COLOR_BLUE);
    //  GBA soft reset: A + B + Start + Select.
    pbf_press_button(context, BUTTON_B | BUTTON_A | BUTTON_MINUS | BUTTON_PLUS, 360ms, 1440ms);
    LoadMenuWatcher load_menu(COLOR_BLUE);
    int ls = run_until<ProControllerContext>(
        console, context,
        [](ProControllerContext& ctx){
            pbf_mash_button(ctx, BUTTON_A, 20000ms);
        },
        {load_menu}
    );
    if (ls != 0){
        return Pokemon::StoryFailure{
            .code = std::string(Pokemon::STORY_LAUNCH_FAILED),
            .detail = "frlg (emulator): load menu did not appear after soft reset",
            .repeats = true,
        };
    }
    context.wait_for_all_requests();
    pbf_wait(context, 500ms);
    context.wait_for_all_requests();
    VideoSnapshot after_settle = console.video().snapshot();
    //  At 2x speed, the final mash A may already have selected CONTINUE; only wait for
    //  the black fade if the load menu is still on screen.
    if (after_settle && LoadMenuDetector(COLOR_BLUE).detect(after_settle)){
        pbf_press_button(context, BUTTON_A, 160ms, 320ms);
        BlackScreenOverWatcher black_over(COLOR_RED);
        int entered = wait_until(
            console, context,
            GameSettings::instance().ENTER_GAME_WAIT0,
            {black_over}
        );
        if (entered != 0){
            return Pokemon::StoryFailure{
                .code = std::string(Pokemon::STORY_LAUNCH_FAILED),
                .detail = "frlg (emulator): timed out waiting to enter game",
                .repeats = true,
            };
        }
    }
    return std::nullopt;
}

} // namespace

std::optional<Pokemon::StoryFailure> launch_frlg_save(ConsoleHandle& console, ProControllerContext& context){
    std::optional<Pokemon::StoryFailure> failed;
    if (GameSettings::instance().DEVICE == GameSettings::Device::GbaEmulator){
        failed = launch_frlg_save_emulator(console, context);
    }else{
        FrlgLaunchBackend backend(console, context);
        const StoryLaunchPolicy policy = frlg_launch_policy(
            Pokemon::switch_launch_prompts(console),
            {.handle_online_license_check = GameSettings::instance().HANDLE_ONLINE_LICENSE_CHECK}
        );
        const Pokemon::StoryLaunchRequest request{
            .save = Pokemon::StoryLaunchRequest::Save::LATEST,
            .running = Pokemon::StoryLaunchRequest::Running::RESTART,
        };
        failed = Pokemon::launch_story_game(backend, policy, request);
    }
    if (failed){
        return failed;
    }
    try{
        mash_until_overworld(console, context, RECAP_TIMEOUT);
    }catch (OperationFailedException& e){
        return Pokemon::StoryFailure{
            .code = std::string(Pokemon::STORY_LAUNCH_FAILED),
            .detail = std::format("frlg: no START menu after the load: {}", e.message()),
            .repeats = true,
        };
    }
    return std::nullopt;
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
