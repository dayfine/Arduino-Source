/*  Center Visit
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <optional>
#include <string>
#include <utility>
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

//  The settle after a door, as the visit's `failure` unless it ended
//  quiet.
std::optional<Outcome> settle_door(SettleKind kind, std::string_view label, std::string_view failure, CenterVisitIO& io){
    const SettleEnd end = io.settle(kind, label);
    if (end.result == SettleEnd::Result::QUIET){
        return std::nullopt;
    }
    return outcome_failed(std::string(failure), end.text);
}

} // namespace


Outcome enter_center(std::string_view label, CenterVisitIO& io){
    //  Every way in settles before the walk to the nurse: the inside can
    //  still be fading in whether or not the walk saw the fade. With no
    //  fade, the walk can have gone on in as far as the counter (BDSP lab-B
    //  2026-11-08, Oreburgh).
    //  A lineup that saw no fade either settles AFTER_WARP: it can return
    //  on the mat before the fade starts.
    const WarpSeen seen = io.walk_in();
    std::string how = describe(seen);
    SettleKind kind = SettleKind::DOOR;
    if (!seen.any()){
        if (io.place() == CenterPlace::INSIDE){
            how = "no fade, but inside";
        }else{
            const WarpSeen lined_up = io.line_up_and_walk_in();
            how = std::format("no fade; lined up by sight: {}", describe(lined_up));
            if (!lined_up.any()){
                kind = SettleKind::AFTER_WARP;
            }
        }
    }
    if (std::optional<Outcome> failed = settle_door(kind, std::format("{}: into the Center", label), CENTER_NOT_ENTERED, io)){
        return std::move(*failed);
    }
    const CenterPlace place = io.place();
    io.log(std::format("[Center] {}: in: {}; reads {}.", label, how, place_name(place)));
    if (place == CenterPlace::OUTSIDE){
        return outcome_failed(std::string(CENTER_NOT_ENTERED), "not in a Pokemon Center after its door");
    }
    return outcome_ok();
}

Outcome leave_center(std::string_view label, CenterVisitIO& io){
    //  With no fade seen yet, the fade can still be to come: the settle
    //  waits for it, and fails when none shows.
    const WarpSeen seen = io.walk_out();
    const SettleKind kind = seen.any() ? SettleKind::DOOR : SettleKind::AFTER_WARP;
    if (std::optional<Outcome> failed = settle_door(kind, std::format("{}: out of the Center", label), CENTER_NOT_LEFT, io)){
        return std::move(*failed);
    }
    const CenterPlace place = io.place();
    io.log(std::format("[Center] {}: out: {}; reads {}.", label, describe(seen), place_name(place)));
    if (place == CenterPlace::INSIDE){
        return outcome_failed(std::string(CENTER_NOT_LEFT), "still inside the Pokemon Center after its door");
    }
    return outcome_ok();
}

Outcome run_center_visit(std::string_view label, CenterVisitIO& io){
    const Outcome in = enter_center(label, io);
    if (!in.succeeded()){
        return in;
    }
    io.heal();
    io.log(std::format("[Center] {}: healed.", label));
    return leave_center(label, io);
}



} // namespace Pokemon
} // namespace PokemonAutomation
