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
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "NintendoSwitch/Inference/NintendoSwitch_CheckOnlineDetector.h"
#include "NintendoSwitch/Inference/NintendoSwitch_HomeMenuDetector.h"
#include "Pokemon/AutoStory/Engine/Pokemon_ScreenObservation.h"
#include "Pokemon/AutoStory/Pokemon_StorySwitchPrompts.h"
#include "Pokemon/AutoStory/Types/Pokemon_GridPose.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_LoadMenuDetector.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldWalkMoves.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Settings.h"
#include "PokemonFRLG_AutoStoryPresses.h"
#include "PokemonFRLG_AutoStoryScreenEvents.h"
#include "PokemonFRLG_AutoStoryLaunch.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using namespace std::chrono_literals;
using Pokemon::GridPose;
using Pokemon::StoryLaunchPolicy;
using Pokemon::StoryLaunchPress;
using Pokemon::StoryLaunchPrompt;


namespace{

//  How long mash_until_overworld() may take after the launch: the recap's
//  pages, then the START menu's check.
constexpr Milliseconds RECAP_TIMEOUT = 20000ms;

bool shows(const ScreenObservation& look, std::string_view reading){
    const auto it = look.readings.find(reading);
    return it != look.readings.end() && it->second != 0;
}

//  Nothing on screen: no fade, box or battle.
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

//  Whether the launch chose CONTINUE on the load menu: the screens after
//  it are the save loading and the overworld, not the GBA's title, so a
//  quiet one is no longer pressed. Set by the load menu's prompt once it
//  matches, as the driver presses each prompt it matches.
struct ContinueChosen{
    bool chosen = false;
};

std::function<bool(const ScreenObservation&)> showing(std::string_view reading){
    return [reading](const ScreenObservation& look){
        return shows(look, reading);
    };
}

//  The online license check: as the RNG programs' hard reset, a second on
//  it, then A.
StoryLaunchPrompt license_check_prompt(){
    return {.name = "license check", .matches = showing(FrlgLaunchReadings::CHECK_ONLINE),
            .press = {.button = BUTTON_A, .hold = 160ms, .release = 1840ms}};
}


//  The console, for the launch alone: presses, and looks that read HOME,
//  the license check and FRLG's screens into the launch's readings (the
//  Switch's prompts read the look's frame themselves).
//  code-health: allow public_methods -- the StoryBackend interface; nothing to split
class FrlgLaunchBackend : public Pokemon::StoryBackend<GridPose>{
public:
    FrlgLaunchBackend(ConsoleHandle& console, ProControllerContext& context)
        : m_console(console)
        , m_context(context)
        , m_home(console, COLOR_RED)
        , m_load_menu(COLOR_BLUE)
    {}

    virtual Pokemon::StoryBackendKind kind() const override{
        return Pokemon::StoryBackendKind::CONSOLE;
    }
    virtual void press(Button button, Milliseconds hold, Milliseconds release) override{
        pbf_press_button(m_context, button, hold, release);
    }
    virtual void tap(Pokemon::Direction direction) override{
        pbf_press_dpad(m_context, OverworldWalkSteps::dpad(direction), 80ms, 400ms);
    }
    virtual void hold(Pokemon::Direction direction, Milliseconds duration, Pokemon::StoryPace) override{
        pbf_press_dpad(m_context, OverworldWalkSteps::dpad(direction), duration, 0ms);
    }
    virtual void stick(double, Milliseconds, Milliseconds, Pokemon::StoryPace) override{
        //  FRLG walks by the D-pad: a launch never moves the stick.
    }
    virtual void wait(Milliseconds duration) override{
        m_context.wait_for(duration);
    }
    virtual Pokemon::Observation<GridPose> observe() override;
    virtual Pokemon::Outcome battle(const Pokemon::StoryActionArgs&) override{
        return Pokemon::outcome_failed("unsupported_step", "a launch fights no battle");
    }
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

Pokemon::Observation<GridPose> FrlgLaunchBackend::observe(){
    m_context.wait_for_all_requests();
    VideoSnapshot frame = m_console.video().snapshot();
    Pokemon::Observation<GridPose> look;
    if (frame && m_home.detect(*frame.frame)){
        //  Alone: FRLG's detectors may read HOME's plain screen as quiet.
        look.events.set(ScreenEvent::SWITCH_HOME_MENU, true);
    }else if (frame){
        static_cast<ScreenObservation&>(look) = Pokemon::screen_observation(m_observer.observe(frame, current_time()));
        read_prompts(*frame.frame, look);
    }else{
        //  No frame: nothing to read, so not a quiet overworld either.
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
    //  The GBA's boot and title, read only after the launch, as the
    //  overworld is quiet too.
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
    //  The title takes A through the boot's logos too, a second apart; HOME
    //  takes A through the license check's seconds on it.
    policy.presses_per_screen = settings.handle_online_license_check ? 24 : 12;
    return policy;
}

Pokemon::Outcome launch_frlg_save(ConsoleHandle& console, ProControllerContext& context){
    FrlgLaunchBackend backend(console, context);
    const StoryLaunchPolicy policy = frlg_launch_policy(
        Pokemon::switch_launch_prompts(console),
        {.handle_online_license_check = GameSettings::instance().HANDLE_ONLINE_LICENSE_CHECK}
    );
    const Pokemon::StoryLaunchRequest request{
        .save = Pokemon::StoryLaunchRequest::Save::LATEST,
        .running = Pokemon::StoryLaunchRequest::Running::RESTART,
    };
    Pokemon::Outcome launched = Pokemon::launch_story_game(backend, policy, request);
    if (!launched.succeeded()){
        return launched;
    }
    try{
        mash_until_overworld(console, context, RECAP_TIMEOUT);
    }catch (OperationFailedException& e){
        return Pokemon::outcome_failed(std::string(Pokemon::STORY_LAUNCH_FAILED), std::format("frlg: no START menu after the load: {}", e.message()));
    }
    return Pokemon::outcome_ok();
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
