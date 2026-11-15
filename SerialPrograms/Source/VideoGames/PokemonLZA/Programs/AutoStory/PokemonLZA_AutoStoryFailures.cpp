/*  PokemonLZA AutoStory - Failures
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <utility>
#include "PokemonLZA_AutoStoryFailures.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using Pokemon::StoryFailureClass;


const std::vector<PlzaFailure>& plza_failures(){
    using C = StoryFailureClass;
    static const std::vector<PlzaFailure> failures{
        //  The checkpoint's start and the reload.
        {"not_at_start", C::WRONG_START},
        {"older_save", C::WRONG_START},
        {"no_reload", C::RELOAD_FAILED},
        //  Walks. A walk that ends where the gate is out of view, or facing
        //  away from the heading it turned to, is a pose not as planned.
        {"gate_not_seen", C::POSE_LOST},
        {"heading_missed", C::POSE_LOST},
        //  walk_to_zone_1_start's end check: the zone's far gate read short
        //  of 127 (by 2, console B 2026-11-14) or the arrow outside the
        //  zone's green.
        {"zone_start_missed", C::WALK_SHORT},
        //  walk_back_to_zone_road's stops: a catch spot on Wild Zone 1's
        //  road the objective's distance never reached (03.08).
        {"zone_road_missed", C::WALK_SHORT},
        //  A walk held until a fade that never showed.
        {"walk_no_end", C::LATE_FADE},
        //  A walk that never got free control: a banner, text or a fade
        //  that stayed.
        {"walk_not_free", C::INTERRUPT_FAILED},
        //  A screen the step waits for, not seen: the A prompt a walk ends
        //  on (a missed gate, console B 2026-11-14 13:35), the compass, the
        //  platform, a scene's end.
        {"no_prompt", C::SCREEN_UNREAD},
        {"heading_unread", C::SCREEN_UNREAD},
        {"platform_not_seen", C::SCREEN_UNREAD},
        {"scene_quiet", C::SCREEN_UNREAD},
        {"scene_timeout", C::SCREEN_UNREAD},
        {"scene_stuck", C::SCREEN_UNREAD},
        {"scene_unknown", C::SCREEN_UNREAD},
        {"scene_never_started", C::SCREEN_UNREAD},
        //  Battles.
        {"battle_failed", C::BATTLE_LOST},
    };
    return failures;
}

std::optional<StoryFailureClass> plza_failure_class(std::string_view name){
    const std::vector<PlzaFailure>& failures = plza_failures();
    const auto found = std::find_if(failures.begin(), failures.end(), [name](const PlzaFailure& failure){
        return failure.name == name;
    });
    if (found == failures.end()){
        return std::nullopt;
    }
    return found->failure_class;
}

Pokemon::Outcome plza_failed(std::string name, std::string detail){
    const std::optional<StoryFailureClass> failure_class = plza_failure_class(name);
    return Pokemon::outcome_failed(std::move(name), std::move(detail), failure_class);
}

std::string_view console_step_failure(std::string_view action){
    if (action == "walk_to_zone_1_start"){
        return "zone_start_missed";
    }
    if (action == "walk_back_to_zone_road"){
        return "zone_road_missed";
    }
    return "operation_failed";
}




} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
