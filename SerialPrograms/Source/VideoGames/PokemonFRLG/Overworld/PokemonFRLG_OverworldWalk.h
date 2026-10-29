/*  Overworld Walk
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Walks a planned overworld route with the D-pad. FRLG moves one tile per
 *  16 frames and finishes a step once it starts, so a leg is one hold timed
 *  to land mid-way between the last tile and one more. Warps are waited out
 *  by their fade; Surf goes through its dialog.
 *
 *  A message or a wild battle can stop the player part-way through a leg.
 *  Each kind is an OverworldInterrupt: the walker watches for all of them
 *  while it holds the D-pad, and when one shows it stops, has it cleared and
 *  walks the rest of the leg.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_OverworldWalk_H
#define PokemonAutomation_PokemonFRLG_OverworldWalk_H

#include <memory>
#include <optional>
#include <vector>
#include "Common/Cpp/Time.h"
#include "CommonTools/InferenceCallbacks/VisualInferenceCallback.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "PokemonFRLG_OverworldRoute.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Something that stops the player part-way through a leg.
class OverworldInterrupt{
public:
    virtual ~OverworldInterrupt() = default;

    virtual const char* name() const = 0;

    //  A new watcher for the screen the interruption brings up.
    virtual std::unique_ptr<VisualInferenceCallback> make_watcher() const = 0;

    //  How long after the step that set it off the watcher fires. The game
    //  only interrupts between steps, so from this the walker works out how
    //  many tiles of an open leg were walked.
    virtual Milliseconds detection_delay() const = 0;

    //  Clears it, leaving the player standing on the overworld, on the same
    //  tile and facing the same way.
    virtual void clear(ConsoleHandle& console, ProControllerContext& context) const = 0;
};

using OverworldInterrupts = std::vector<std::unique_ptr<OverworldInterrupt>>;

//  A wild battle, run from, and a message (e.g. "REPEL's effect wore off..."),
//  closed with B.
const OverworldInterrupts& default_overworld_interrupts();


//  Walks `route` from where it was planned. The player must be standing still
//  on the overworld, not running and not on the bike. `facing` is the way the
//  player faces at the start, if known. Throws OperationFailedException if a
//  warp's fade or the Surf prompt doesn't show, or a leg is interrupted too
//  many times.
void walk_overworld_route(
    ConsoleHandle& console, ProControllerContext& context,
    const std::vector<RouteStep>& route,
    std::optional<MapDirection> facing = std::nullopt,
    const OverworldInterrupts& interrupts = default_overworld_interrupts()
);



}
}
}
#endif
