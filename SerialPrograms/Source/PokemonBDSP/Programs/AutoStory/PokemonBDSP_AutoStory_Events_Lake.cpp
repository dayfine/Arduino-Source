/*  BDSP AutoStory - Events: the lake and the Pokedex (00.02 to 00.04)
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Adapted from PokemonAutomation/Arduino-Source#1717 by Developer-Butters.
 *
 *  The scenes of the story's start that need scripted input: the briefcase
 *  at Lake Verity, the starter, the Starly battle and the scene after it,
 *  the Pokedex in Rowan's lab, Mom's and Barry's mother's lines, and Dawn's
 *  catching tutorial. The button sequences are Butters' (#1717); they ran on
 *  the console in his recordings, but not yet from the planned walks around
 *  them.
 *
 */

#include <memory>
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/VisualDetectors/ImageMatchDetector.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "PokemonBDSP/Inference/PokemonBDSP_DialogDetector.h"
#include "PokemonBDSP_AutoStory_Actions.h"
#include "PokemonBDSP_AutoStory_WildBattle.h"
#include "PokemonBDSP_AutoStory_Events.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace std::chrono_literals;


namespace{

//  00.02: the lake's scene, from Barry's first line by the entrance to the
//  starters' briefcase: B until the briefcase shows, then until it is open.
void starter_briefcase(VideoStream& stream, ProControllerContext& context){
    wait_for_dialogue(stream, context, "Barry before the starter selection");
    pbf_mash_button(context, BUTTON_A, 1000ms);
    pbf_wait(context, 500ms);
    std::shared_ptr<const ImageRGB32> briefcase = std::make_shared<const ImageRGB32>(
        RESOURCE_PATH() + "PokemonBDSP/StarterBriefcase.png"
    );
    ImageMatchWatcher detector(briefcase, {0.5, 0.1, 0.5, 0.7}, 100, true);
    const int ret = run_until<ProControllerContext>(
        stream, context,
        [](ProControllerContext& context){
            pbf_mash_button(context, BUTTON_B, 120000ms);
        },
        {{detector}}
    );
    if (ret != 0){
        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
            "Failed to detect the starters' briefcase.", stream);
    }
    stream.log("Detected briefcase! Mashing B for it to open.");
    pbf_mash_button(context, BUTTON_B, 2000ms);
}

//  00.02: the starter from the program's option, in the briefcase: Turtwig
//  is first, Chimchar a press right, Piplup two; ZL picks, then Up and ZL
//  answer "this one?" with Yes.
Pokemon::Outcome choose_starter(StoryExecutor& executor, Pokemon::RunState&, const Pokemon::StoryStep&){
    VideoStream& stream = executor.stream();
    ProControllerContext& context = executor.context();
    size_t rights = 0;
    switch (executor.env().starter){
    case StarterChoice::TURTWIG:
        rights = 0;
        break;
    case StarterChoice::CHIMCHAR:
        rights = 1;
        break;
    case StarterChoice::PIPLUP:
        rights = 2;
        break;
    }
    for (size_t i = 0; i < rights; i++){
        pbf_press_dpad(context, DPAD_RIGHT, 160ms, 840ms);
    }
    pbf_press_button(context, BUTTON_ZL, 160ms, 240ms);
    context.wait_for_all_requests();
    ShortDialogWatcher watcher(COLOR_CYAN);
    int ret = wait_until(stream, context, 5000ms, {{watcher}});
    if (ret != 0){
        ShortDialogWatcher watcher_red(COLOR_RED);
        ret = wait_until(stream, context, 5000ms, {{watcher_red}});
    }
    stream.log(ret == 0 ? "[Starter] Confirmation dialogue detected." : "[Starter] Dialogue detection failed, proceeding anyway.", ret == 0 ? COLOR_BLACK : COLOR_ORANGE);
    pbf_wait(context, 400ms);
    pbf_press_dpad(context, DPAD_UP, 80ms, 400ms);
    pbf_press_button(context, BUTTON_ZL, 80ms, 5000ms);
    context.wait_for_all_requests();
    return Pokemon::outcome_ok();
}

//  00.01: BDSP puts an Azure Flute on the bedroom floor at (7, 4) when the console
//  has Legends: Arceus save data, and nothing there otherwise. Facing that tile:
//  A, and if text follows ("You found an Azure Flute!", then the pocket line), B
//  through it. Without the ball the A does nothing and we go on.
void azure_flute_if_there(VideoStream& stream, ProControllerContext& context){
    pbf_press_button(context, BUTTON_A, 80ms, 0ms);
    context.wait_for_all_requests();
    for (int presses = 0; presses < 6; presses++){
        ShortDialogWatcher dialog(COLOR_YELLOW);
        if (wait_until(stream, context, 2000ms, {{dialog}}) != 0){
            return;
        }
        pbf_press_button(context, BUTTON_B, 80ms, 1000ms);
        context.wait_for_all_requests();
    }
}

//  00.02: after the Starly battle, Rowan's lines and the walk out of the
//  lake, B for a minute.
void after_starly(VideoStream&, ProControllerContext& context){
    pbf_mash_button(context, BUTTON_B, 60000ms);
    context.wait_for_all_requests();
}

//  00.03: Dawn walks us into Rowan's lab: his lines, "give a nickname?"
//  (B: No), and the Pokedex, taken with A.
void rowan_gives_pokedex(VideoStream& stream, ProControllerContext& context){
    wait_for_dialogue(stream, context, "Dawn at Sandgem's entrance");
    pbf_mash_button(context, BUTTON_B, 50000ms);
    wait_for_dialogue(stream, context, "Nickname: no");
    pbf_mash_button(context, BUTTON_B, 40000ms);
    wait_for_dialogue(stream, context, "The Pokedex");
    pbf_mash_button(context, BUTTON_A, 40000ms);
    context.wait_for_all_requests();
}

//  00.04: Barry's mother stands in the door with his parcel: four A, with
//  the waits of Butters' recording between them.
void barrys_mother(VideoStream&, ProControllerContext& context){
    pbf_press_button(context, BUTTON_A, 180ms, 946ms);
    pbf_press_button(context, BUTTON_A, 180ms, 9092ms);
    pbf_press_button(context, BUTTON_A, 180ms, 740ms);
    pbf_press_button(context, BUTTON_A, 180ms, 2998ms);
    context.wait_for_all_requests();
}

//  00.04: Dawn's catching tutorial on Route 202: her lines and her demo
//  battle, B for 70 s.
void catch_tutorial(VideoStream& stream, ProControllerContext& context){
    wait_for_dialogue(stream, context, "Dawn's catching tutorial");
    pbf_mash_button(context, BUTTON_B, 70000ms);
    context.wait_for_all_requests();
    pbf_wait(context, 2000ms);
}

}


void add_lake_events(StoryEvents& events){
    events.emplace("azure_flute_if_there", console_event(azure_flute_if_there));
    events.emplace("starter_briefcase", console_event(starter_briefcase));
    events.emplace("choose_starter", choose_starter);
    events.emplace("fight_starly", console_event(fight_starly));
    events.emplace("after_starly", console_event(after_starly));
    events.emplace("rowan_gives_pokedex", console_event(rowan_gives_pokedex));
    events.emplace("barrys_mother", console_event(barrys_mother));
    events.emplace("catch_tutorial", console_event(catch_tutorial));
    events.emplace("catch_pokemon", console_event(catch_1_pokemon));
}



}
}
}
