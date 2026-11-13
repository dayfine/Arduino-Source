/*  AutoStory RNG Returns
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "NintendoSwitch/Inference/NintendoSwitch_HomeMenuDetector.h"
#include "NintendoSwitch/Programs/NintendoSwitch_GameEntry.h"
#include "PokemonFRLG_AutoStoryPresses.h"
#include "PokemonFRLG_AutoStoryRngReturns.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using namespace std::chrono_literals;


namespace{

//  After the RNG program `program`: from HOME, if it went there, back to
//  the game, then B until the overworld.
void back_to_overworld_after(ConsoleHandle& console, ProControllerContext& context, const std::string& program){
    context.wait_for_all_requests();
    VideoSnapshot screen = console.video().snapshot();
    if (screen && HomeMenuDetector(console, COLOR_RED).detect(screen)){
        console.log("After " + program + ": on HOME, back to the game.");
        resume_game_from_home(console, context);
    }
    mash_until_overworld(console, context, 20000ms);
}

} // namespace

void close_trainer_card(ConsoleHandle& console, ProControllerContext& context){
    //  B closes the card, then the START menu.
    back_to_overworld_after(console, context, "SID Helper");
}

void leave_gift_summary(ConsoleHandle& console, ProControllerContext& context){
    //  B closes the summary, the party, then the START menu.
    back_to_overworld_after(console, context, "Gift RNG");
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
