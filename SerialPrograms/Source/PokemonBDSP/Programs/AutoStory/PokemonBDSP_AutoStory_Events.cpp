/*  BDSP AutoStory - Events
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <utility>
#include "PokemonBDSP_AutoStory_Events.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


const StoryEvents& story_events(){
    static const StoryEvents events = []{
        StoryEvents ret;
        add_lake_events(ret);
        add_sandgem_events(ret);
        add_jubilife_events(ret);
        add_route203_events(ret);
        add_oreburgh_events(ret);
        add_route204_events(ret);
        return ret;
    }();
    return events;
}

StoryEvent console_event(std::function<void(VideoStream& stream, ProControllerContext& context)> action){
    return [action = std::move(action)](StoryExecutor& executor, Pokemon::RunState&, const Pokemon::StoryStep&){
        action(executor.stream(), executor.context());
        return Pokemon::outcome_ok();
    };
}



}
}
}
