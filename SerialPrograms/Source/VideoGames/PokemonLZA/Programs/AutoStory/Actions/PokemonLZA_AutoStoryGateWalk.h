/*  PokemonLZA AutoStory - Walk to a wild zone's gate
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The walk up the street to a wild zone's entrance gate, steered by the
 *  gate's pillars on screen (read_zone_gate()) until the gate's A prompt
 *  shows: 03.06's walk into Wild Zone 1. Timed turns from the Pokemon
 *  Center ended pointing along the sidewalk, and a straight walk from
 *  there ran 10 s into the zone's wall (console B 2026-11-14).
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryGateWalk_H
#define PokemonAutomation_PokemonLZA_AutoStoryGateWalk_H

#include <functional>
#include <optional>
#include <string_view>
#include "Common/Cpp/Time.h"
#include "Pokemon/AutoStory/Pokemon_StoryOutcome.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryBackend.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The reading (ScreenObservation::readings) of a wild zone's entrance
//  gate's middle on a look (read_zone_gate()), in thousandths of the
//  screen's width; none when no gate shows.
inline constexpr std::string_view ZONE_GATE_X_READING = "walk.zone_gate_x";
//  The reading of the gate's width, as ZONE_GATE_X_READING.
inline constexpr std::string_view ZONE_GATE_WIDTH_READING = "walk.zone_gate_width";

//  What sets a look's readings from its frame.
using LookReader = std::function<void(Pokemon::Observation<Pokemon::ZonePose>& look)>;


//  What walk_to_zone_gate_entrance() does.
struct GateWalk{
    //  The minimap heading to face first (face_heading(), by the arrow
    //  reader), along the street to the gate; none: no turn first.
    std::optional<double> heading = std::nullopt;
    //  Walking this long with no A prompt fails the walk.
    Milliseconds timeout = Milliseconds(15000);
};

//  Faces `walk`'s heading by `arrow`, then walks in slices toward the gate, the stick
//  turned toward it on each look the gate shows (`gate` sets its
//  readings): 0 within 2% of the middle, else the gate's offset from the
//  middle times the screen's ~60 degrees of view (a guess, NEEDS CONSOLE),
//  rounded, 30 at most at the edges. On until the gate's A prompt (INTERACT_PROMPT) shows; none
//  when it shows already. Once the gate is lost (too close: its pillars
//  leave the screen), the last angle holds. Fails as "gate_not_seen" when
//  the first second of looks shows no gate (the player faces away from
//  it; walking on ran into the zone's wall), and as "no_prompt" after the
//  timeout, each with how many looks saw the gate and its last reading.
//  A wild battle (BATTLE_STARTED; a Mareep at the zone's pillar, console
//  B 2026-11-16) is walked on through at the last angle, its looks counted
//  toward neither; still in battle after 20 s fails as "wild_battle".
//  The heading at the prompt is logged when `arrow` is set.
Pokemon::Outcome walk_to_zone_gate_entrance(
    PlzaBackend& backend, const MinimapArrowReader& arrow, const LookReader& gate, const GateWalk& walk
);

//  walk_to_zone_gate_entrance() on the console `env`: the minimap arrow
//  (minimap_arrow()) and the gate's pillars on its frames (read_zone_gate()).
Pokemon::Outcome walk_to_zone_gate(StoryEnv& env, PlzaBackend& backend, const GateWalk& walk);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
