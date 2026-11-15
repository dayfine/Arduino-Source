/*  BDSP AutoStory - Launch
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <chrono>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/VisualDetectors/BlackScreenDetector.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Inference/NintendoSwitch_HomeMenuDetector.h"
#include "Pokemon/AutoStory/Engine/Pokemon_ScreenObservation.h"
#include "Pokemon/AutoStory/Pokemon_StorySwitchPrompts.h"
#include "Pokemon/AutoStory/Types/Pokemon_GridPose.h"
#include "Detect/PokemonBDSP_AutoStory_ScreenEvents.h"
#include "Detect/PokemonBDSP_AutoStory_TitleDetector.h"
#include "PokemonBDSP_AutoStory_Launch.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace std::chrono_literals;
using Pokemon::GridPose;
using Pokemon::StoryLaunchPolicy;
using Pokemon::StoryLaunchPrompt;


namespace{

//  The names HOME shows above the game's tile, in the console's language
//  (console B's is English). Either version of the game.
constexpr std::string_view DIAMOND_TILE = "Pokémon Brilliant Diamond";
constexpr std::string_view PEARL_TILE = "Pokémon Shining Pearl";
constexpr Language HOME_LANGUAGE = Language::English;

constexpr std::string_view NO_LAUNCH_WALK = "a launch never walks";


//  The console, for the launch alone: presses, and looks that read HOME,
//  the boot's black, BDSP's screen events and its title (the Switch's
//  prompts read the look's frame themselves).
//  code-health: allow public_methods -- the StoryBackend interface; nothing to split
class BdspLaunchBackend : public Pokemon::StoryBackend<GridPose>{
public:
    BdspLaunchBackend(ConsoleHandle& console, ProControllerContext& context)
        : m_console(console)
        , m_context(context)
        , m_home(console, COLOR_RED)
        , m_black(COLOR_BLUE, {0.2, 0.2, 0.6, 0.6})
        , m_observer(BattleCue::WILD_MENU, "")
    {}

    virtual Pokemon::StoryBackendKind kind() const override{
        return Pokemon::StoryBackendKind::CONSOLE;
    }
    virtual void press(Button button, Milliseconds hold, Milliseconds release) override{
        pbf_press_button(m_context, button, hold, release);
    }
    virtual void tap(Pokemon::Direction) override{
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::string(NO_LAUNCH_WALK));
    }
    virtual void hold(Pokemon::Direction, Milliseconds, Pokemon::StoryPace) override{
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::string(NO_LAUNCH_WALK));
    }
    virtual void stick(double, Milliseconds, Milliseconds, Pokemon::StoryPace) override{
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::string(NO_LAUNCH_WALK));
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
    ConsoleHandle& m_console;
    ProControllerContext& m_context;
    HomeMenuDetector m_home;
    //  The boot's and the load's black, as gamemenu_to_ingame() reads it.
    BlackScreenDetector m_black;
    BdspScreenObserver m_observer;
};

Pokemon::Observation<GridPose> BdspLaunchBackend::observe(){
    m_context.wait_for_all_requests();
    VideoSnapshot frame = m_console.video().snapshot();
    Pokemon::Observation<GridPose> look;
    if (frame && m_black.detect(*frame.frame)){
        //  The game booting or loading: never HOME, whose dark theme is
        //  dark too.
        look.events.set(ScreenEvent::FADE_BLACK, true);
    }else if (frame && m_home.detect(*frame.frame)){
        //  Alone: BDSP's detectors may read HOME's plain screen as quiet.
        look.events.set(ScreenEvent::SWITCH_HOME_MENU, true);
    }else if (frame){
        static_cast<ScreenObservation&>(look) = Pokemon::screen_observation(m_observer.observe(frame, current_time()));
        if (title_screen_shown(*frame.frame)){
            look.readings[std::string(TITLE_READING)] = 1;
        }
    }else{
        //  No frame: nothing to read, so not a quiet title either.
        look.events.set(ScreenEvent::FADE_BLACK, true);
    }
    look.when = current_time();
    look.frame = std::move(frame);
    return look;
}

} // namespace


StoryLaunchPolicy bdsp_launch_policy(std::vector<StoryLaunchPrompt> switch_prompts){
    StoryLaunchPolicy policy;
    policy.game = "bdsp";
    policy.tile_selected = [
        diamond = Pokemon::switch_home_tile_named(HOME_LANGUAGE, std::string(DIAMOND_TILE)),
        pearl = Pokemon::switch_home_tile_named(HOME_LANGUAGE, std::string(PEARL_TILE))
    ](const ScreenObservation& look){
        return diamond(look) || pearl(look);
    };
    policy.title = [](const ScreenObservation& look){
        return look.readings.contains(TITLE_READING);
    };
    //  Before the launch, a title on screen is left with HOME and the game
    //  closed, as any screen of a game in play.
    policy.title_after_launch = true;
    policy.overworld_after_load = true;
    policy.title_press = {.button = BUTTON_ZL, .hold = 160ms, .release = 1000ms};
    policy.prompts = std::move(switch_prompts);
    //  The title takes ZL through the boot's logos too, a second apart,
    //  and through the frames of the load before the overworld shows.
    policy.presses_per_screen = 20;
    return policy;
}

Pokemon::Outcome launch_bdsp_save(ConsoleHandle& console, ProControllerContext& context){
    BdspLaunchBackend backend(console, context);
    const StoryLaunchPolicy policy = bdsp_launch_policy(Pokemon::switch_launch_prompts(console));
    const Pokemon::StoryLaunchRequest request{
        .save = Pokemon::StoryLaunchRequest::Save::LATEST,
        .running = Pokemon::StoryLaunchRequest::Running::RESTART,
    };
    return Pokemon::launch_story_game(backend, policy, request);
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
