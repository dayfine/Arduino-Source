/*  BDSP AutoStory - Events: the Ravaged Path (01.09)
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The Ravaged Path's Rock Smash, as it ran on the console (2026-10-31).
 *
 */

#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "PokemonBDSP/Inference/PokemonBDSP_DialogDetector.h"
#include "PokemonBDSP/Inference/PokemonBDSP_SelectionArrow.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_RouteWalk.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Walk.h"
#include "PokemonBDSP_AutoStory_Talks.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_Walks.h"
#include "PokemonBDSP_AutoStory_WildBattle.h"
#include "PokemonBDSP_AutoStory_Events.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace std::chrono_literals;
using Pokemon::Direction;


namespace{

//  "This rock seems breakable with a hidden move. Would you like to use Rock
//  Smash?" is open, from an A at a smashable rock. Its Yes/No box comes only
//  once the text is advanced again (by hand, 2026-10-31: the text complete
//  1.2 s after the first A, the box 1.2 s after a second), and r237a's timed
//  A, A, B put its B on that box: No. So press A until the box's cursor
//  shows, then once more for Yes, and wait out the move. False if no box
//  shows: whatever is open is not this prompt.
bool answer_rock_smash_prompt(VideoStream& stream, ProControllerContext& context, const std::string& label){
    static const ImageFloatBox YES_NO_ARROW_BOX{0.700, 0.620, 0.070, 0.120};
    SelectionArrowFinder yes_no(stream.overlay(), YES_NO_ARROW_BOX, COLOR_GREEN);
    for (size_t press = 0; press < 5; press++){
        context.wait_for(1200ms);
        context.wait_for_all_requests();
        const VideoSnapshot screen = stream.video().snapshot();
        if (screen && yes_no.detect(*screen.frame)){
            stream.log("[Route] " + label + ": Rock Smash offered; Yes.");
            pbf_press_button(context, BUTTON_A, 80ms, 0ms);
            context.wait_for(5000ms);
            if (dialogue_opens(stream, context, 500ms)){
                finish_dialogue(stream, context, BUTTON_B, label + ": after Rock Smash");
            }
            return true;
        }
        pbf_press_button(context, BUTTON_A, 80ms, 0ms);
    }
    stream.log("[Route] " + label + ": no Yes/No box for Rock Smash.", COLOR_RED);
    return false;
}

//  01.09: Rock Smash on the rock east of us, from the tile the route ends on
//  facing it. Only the prompt's Yes/No box tells the rock from someone in
//  the way (r237a: the walker took one for a person and said No).
void smash_the_rock(VideoStream& stream, ProControllerContext& context){
    route_turn(stream, context, Direction::EAST);
    if (!talk_with_nudges(stream, context, run_from_wild_battle, Direction::EAST, "the rock east of us", 2000ms)){
        fail_step(stream, "Ravaged Path: nothing answered facing the rock east of us.");
    }
    if (!answer_rock_smash_prompt(stream, context, "the rock east of us")){
        fail_step(stream, "Ravaged Path: no Rock Smash offer facing the rock east of us.");
    }
}

} // namespace


void add_route204_events(StoryEvents& events){
    events.emplace("smash_the_rock", console_event(smash_the_rock));
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
