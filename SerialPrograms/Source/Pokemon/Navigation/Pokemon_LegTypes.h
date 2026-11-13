/*  Leg Types
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A leg of a route walk: what to walk (LegSpec) and what the walk found (LegResult).
 *  Pokemon_LegWalker.h builds on it.
 *
 */

#ifndef PokemonAutomation_Pokemon_LegTypes_H
#define PokemonAutomation_Pokemon_LegTypes_H

#include <algorithm>
#include <atomic>
#include <format>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>
#include "Common/Cpp/Time.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/InferenceCallbacks/VisualInferenceCallback.h"
#include "Pokemon_Grid.h"

namespace PokemonAutomation{
namespace Pokemon{


//  Something that stops the player part-way through a leg.
template <typename Console, typename Context>
class LegInterrupt{
public:
    virtual ~LegInterrupt() = default;

    virtual std::string name() const = 0;

    //  A new watcher for the screen the interruption brings up.
    virtual std::unique_ptr<VisualInferenceCallback> make_watcher() const = 0;

    //  How long after the step that set it off the watcher fires. The games
    //  only interrupt between steps, so from this the walker works out how
    //  many tiles of an open leg were walked.
    virtual Milliseconds detection_delay() const = 0;

    //  Clears it, leaving the player standing on the overworld, on the same
    //  tile and facing the same way.
    virtual void clear(Console& console, Context& context) const = 0;
};

// Collection of interrupt detectors active during leg movement.
template <typename Console, typename Context>
using LegInterrupts = std::vector<std::unique_ptr<LegInterrupt<Console, Context>>>;


//  How the game moves the player: holds `direction` for `duration`, then
//  lets go.
template <typename Context>
using LegMover = std::function<void(Context& context, Direction direction, Milliseconds duration)>;


//  A straight leg, for a player standing still and facing its way.
struct LegSpec{
    Direction direction = Direction::NORTH;
    int tiles = 0;
    //  A wall stops the player after the last tile.
    bool to_wall = false;
    //  How long one tile takes, for this leg's direction and way of moving.
    Milliseconds tile_time{0};
    //  An open leg stops at its first interrupt, once cleared, instead of
    //  walking on from an estimate; the caller finds the player again.
    bool stop_on_interrupt = false;
    //  A leg to a wall stops when, held again after an interrupt, it is
    //  interrupted again right away: something on the way, like a sign,
    //  interrupts every hold, so the wall is never reached.
    bool stop_if_blocked = false;
    //  The steps (from 1) the game holds the player up a tile before, as
    //  FireRed does on a step onto a connected map: each makes a hold over
    //  it a tile longer.
    std::vector<int> stalled_steps = {};
};
// Formats a LegSpec into a descriptive string.
std::string to_string(const LegSpec& leg);

// Execution summary of a completed or stopped leg movement.
struct LegResult{
    //  Tiles walked, which is `tiles` unless the leg gave up.
    int tiles = 0;
    //  Whether `tiles` is certain: an interrupted open leg is an estimate.
    bool exact = true;
    size_t interruptions = 0;
    //  The leg stopped at an interrupt (stop_on_interrupt, stop_if_blocked):
    //  the player is somewhere along it. For an open leg `tiles` is then how
    //  many the interrupt's timing says were walked (tiles_before_interrupt()),
    //  for the caller to look around; for a leg to a wall it is 0.
    bool stopped = false;
};


//  How long to hold for `tiles` tiles: an open leg lets go half a tile before
//  the last step would end, so the step under way finishes and no other
//  starts; a leg to a wall is held two tiles longer than it takes. The last
//  `tiles` of the leg are held for, a tile more for each of its stalled
//  steps among them.
Milliseconds leg_hold(const LegSpec& leg, int tiles);

//  How many tiles an open leg had walked when an interrupt was seen
//  `elapsed` into its hold, given the interrupt's detection delay. Steps end
//  a whole number of tiles into the hold, so this rounds to the nearest.
int tiles_before_interrupt(const LegSpec& leg, Milliseconds elapsed, Milliseconds detection_delay, int remaining);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
