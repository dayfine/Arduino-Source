/*  AutoStory Switch Launch Prompts
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <chrono>
#include <functional>
#include <memory>
#include <utility>
#include "NintendoSwitch/NintendoSwitch_Settings.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "NintendoSwitch/Inference/NintendoSwitch_CloseGameDetector.h"
#include "NintendoSwitch/Inference/NintendoSwitch_StartGameUserSelectDetector.h"
#include "NintendoSwitch/Inference/NintendoSwitch_UpdatePopupDetector.h"
#include "Pokemon_StorySwitchPrompts.h"

namespace PokemonAutomation{
namespace Pokemon{

using namespace std::chrono_literals;
using namespace NintendoSwitch;


namespace{

//  Whether `detector` fires on the look's frame; false without one.
template <typename Detector>
std::function<bool(const ScreenObservation& look)> frame_shows(std::shared_ptr<Detector> detector){
    return [detector = std::move(detector)](const ScreenObservation& look){
        return look.frame && look.frame->frame && detector->detect(*look.frame->frame);
    };
}

} // namespace


std::vector<StoryLaunchPrompt> switch_launch_prompts(ConsoleHandle& console){
    const StoryLaunchPress scroll_up{.button = BUTTON_UP, .hold = 40ms, .release = 40ms};
    return {
        {.name = "close software", .matches = frame_shows(std::make_shared<CloseGameDetector>(console)),
         .press = {.button = BUTTON_A, .hold = 160ms, .release = ConsoleSettings::instance().CLOSE_GAME_DELAY}},
        {.name = "user select", .matches = frame_shows(std::make_shared<StartGameUserSelectDetector>(console)),
         .press = {.button = BUTTON_A, .hold = 160ms}},
        {.name = "update prompt", .matches = frame_shows(std::make_shared<UpdatePopupDetector>(console)),
         .press = {.button = BUTTON_A, .hold = 160ms}, .before = {scroll_up, scroll_up}},
    };
}



} // namespace Pokemon
} // namespace PokemonAutomation
