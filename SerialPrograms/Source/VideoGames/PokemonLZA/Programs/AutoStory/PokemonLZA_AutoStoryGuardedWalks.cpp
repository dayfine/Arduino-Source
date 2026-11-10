/*  PokemonLZA AutoStory - Guarded walks
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <string>
#include <string_view>
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "Pokemon/AutoStory/Pokemon_ScreenEvents.h"
#include "PokemonLZA_AutoStoryScreenEvents.h"
#include "PokemonLZA_AutoStoryGuardedWalks.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;
using Pokemon::ScreenEvent;
using Pokemon::ScreenEvents;


namespace{

//  What no walk may start on.
const ScreenEvents& not_free_control(){
    static const ScreenEvents events{
        ScreenEvent::DIALOG_BOX, ScreenEvent::CHOICE_BOX, ScreenEvent::BATTLE_STARTED,
        ScreenEvent::FADE_BLACK, ScreenEvent::FADE_WHITE,
    };
    return events;
}

} // namespace


void check_can_walk(ConsoleHandle& console, ProControllerContext& context, std::string_view walk){
    context.wait_for_all_requests();
    PlzaScreenObserver observer;
    for (int read = 0; read < 2; read++){
        if (read > 0){
            context.wait_for(150ms);
        }
        const VideoSnapshot frame = console.video().snapshot();
        const ScreenEvents events = frame ? observer.read(*frame.frame) : ScreenEvents{};
        if (!frame || events.any_of(not_free_control())){
            OperationFailedExceptionWithScreenshot::fire(
                ErrorReportMode::SEND_ERROR_REPORT,
                std::format("{}: the walk would start on \"{}\", not free control.",
                    walk, frame ? Pokemon::describe(events) : std::string("no video")),
                console
            );
        }
    }
}

void walk_until(
    ConsoleHandle& console, ProControllerContext& context,
    std::string_view walk, const StoryMove& move, ScreenEvent end
){
    StoryMove head = move;
    Milliseconds hold = 0ms;
    if (!head.legs.empty()){
        hold = head.legs.back().walk;
        head.legs.back().walk = 0ms;
    }
    run_story_move(console, context, head);

    PlzaScreenObserver observer;
    Pokemon::ScreenEventWatcher watcher(observer, ScreenEvents{end});
    const WallClock start = current_time();
    const int ret = run_until<ProControllerContext>(
        console, context,
        [hold](ProControllerContext& subcontext){
            pbf_move_left_joystick(subcontext, {0, 1}, hold, 0ms);
        },
        {watcher}
    );
    const long long held = std::chrono::duration_cast<Milliseconds>(current_time() - start).count();
    if (ret < 0){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            std::format("{}: no {} within {} ms of the last leg.",
                walk, Pokemon::screen_event_name(end), hold.count()),
            console
        );
    }
    console.log(std::format("{}: {} after {} ms of the last leg.", walk, Pokemon::screen_event_name(end), held));
}




} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
