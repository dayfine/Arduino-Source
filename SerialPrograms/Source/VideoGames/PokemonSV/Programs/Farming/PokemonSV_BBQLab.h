/*  BBQ Lab Hooks
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Lab only. Flags that let the BBQ Routine Bench switch a quest routine between two
 *  variants rep by rep, and events the routines report so the bench can time phases
 *  and save screenshots. The farmer leaves the flags at their defaults and sets no hook.
 *
 */

#ifndef PokemonAutomation_PokemonSV_BBQLab_H
#define PokemonAutomation_PokemonSV_BBQLab_H

#include <functional>
#include <string>
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"

namespace PokemonAutomation{
    class VideoStream;
namespace NintendoSwitch{
namespace PokemonSV{


struct BBQLabFlags{
    //  Tera-self at the Chargestone battle court (R11/R12) instead of Pride Rock.
    bool tera_self_at_chargestone = false;

    //  Overrides BBQ_OPTIONS.FLY_DIRECTLY when set: 0 off, 1 on.
    int fly_directly_override = -1;

    //  With direct flights: fly to the closest fly point before the day skip, so the
    //  spot reloads, and flee a wild Pokemon that attacks right after the skip.
    bool leave_area_before_day_skip = false;
};
BBQLabFlags& bbq_lab_flags();


//  Called by quest routines at points of interest ("route_done", "battle",
//  "tera_turn", "battle_end", "day_skip_done"). Does nothing unless the bench set a hook.
using BBQLabEventHook = std::function<void(VideoStream& stream, ProControllerContext& context, const std::string& event)>;
void set_bbq_lab_event_hook(BBQLabEventHook hook);
void bbq_lab_event(VideoStream& stream, ProControllerContext& context, const std::string& event);



}
}
}
#endif
