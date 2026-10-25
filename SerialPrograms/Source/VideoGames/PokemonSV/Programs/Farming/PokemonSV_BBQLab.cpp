/*  BBQ Lab Hooks
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "PokemonSV_BBQLab.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{


BBQLabFlags& bbq_lab_flags(){
    static BBQLabFlags flags;
    return flags;
}

namespace{
BBQLabEventHook& event_hook(){
    static BBQLabEventHook hook;
    return hook;
}
}

void set_bbq_lab_event_hook(BBQLabEventHook hook){
    event_hook() = std::move(hook);
}
void bbq_lab_event(VideoStream& stream, ProControllerContext& context, const std::string& event){
    if (event_hook()){
        event_hook()(stream, context, event);
    }
}



}
}
}
