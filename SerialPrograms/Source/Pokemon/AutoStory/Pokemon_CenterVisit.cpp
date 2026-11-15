/*  Center Visit
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <string>
#include "Pokemon_CenterVisit.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

std::string_view place_name(CenterPlace place){
    switch (place){
    case CenterPlace::INSIDE:   return "inside";
    case CenterPlace::OUTSIDE:  return "outside";
    case CenterPlace::UNKNOWN:  return "unknown";
    }
    return "?";
}

} // namespace


Outcome enter_center(CenterVisitIO& io, std::string_view label){
    //  Every way in settles before the walk to the nurse: the inside can
    //  still be fading in whether or not the walk saw the fade. With no
    //  fade, the walk can have gone on in as far as the counter (BDSP lab-B
    //  2026-11-08, Oreburgh).
    const WarpSeen seen = io.walk_in();
    std::string how = describe(seen);
    if (!seen.any()){
        if (io.place() == CenterPlace::INSIDE){
            how = "no fade, but inside";
        }else{
            io.line_up_and_walk_in();
            how = "no fade; lined up by sight";
        }
    }
    io.settle(SettleKind::DOOR, std::format("{}: into the Center", label));
    const CenterPlace place = io.place();
    io.log(std::format("[Center] {}: in: {}; reads {}.", label, how, place_name(place)));
    if (place == CenterPlace::OUTSIDE){
        return outcome_failed(std::string(CENTER_NOT_ENTERED), "not in a Pokemon Center after its door");
    }
    return outcome_ok();
}

Outcome leave_center(CenterVisitIO& io, std::string_view label){
    //  With no fade seen yet, the fade can still be to come: the settle
    //  waits for it, and fails when none shows.
    const WarpSeen seen = io.walk_out();
    io.settle(seen.any() ? SettleKind::DOOR : SettleKind::AFTER_WARP, std::format("{}: out of the Center", label));
    const CenterPlace place = io.place();
    io.log(std::format("[Center] {}: out: {}; reads {}.", label, describe(seen), place_name(place)));
    if (place == CenterPlace::INSIDE){
        return outcome_failed(std::string(CENTER_NOT_LEFT), "still inside the Pokemon Center after its door");
    }
    return outcome_ok();
}

Outcome run_center_visit(CenterVisitIO& io, std::string_view label){
    Outcome in = enter_center(io, label);
    if (!in.succeeded()){
        return in;
    }
    io.heal();
    io.log(std::format("[Center] {}: healed.", label));
    return leave_center(io, label);
}



} // namespace Pokemon
} // namespace PokemonAutomation
