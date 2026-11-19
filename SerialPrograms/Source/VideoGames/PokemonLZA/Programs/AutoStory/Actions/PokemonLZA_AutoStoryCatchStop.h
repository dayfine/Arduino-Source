/*  PokemonLZA AutoStory - Catch stop
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The console-free choices of one stop on Wild Zone 1's catch patrol:
 *  whether to stop at a spot at all, how a throw watches for the Pokedex's
 *  "Registered!" banner, what to do after a throw, and when to reset the
 *  camera's pitch (the target choice is in PokemonLZA_AutoStoryCatchPlan.h).
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryCatchStop_H
#define PokemonAutomation_PokemonLZA_AutoStoryCatchStop_H

#include <optional>
#include <span>
#include <vector>
#include "Common/Cpp/Time.h"
#include "PokemonLZA_AutoStoryCatchPlan.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  What the patrol does at a catch spot.
enum class SpotVisit{
    CATCH,
    //  The walk there only brings the other spot's Pokemon back.
    TURN_BACK,
};

//  Turn back where a new species was caught: its species is the one there.
SpotVisit spot_visit(int spot, std::span<const int> caught_at);

//  How long the watcher must see the "Registered!" banner before it fires.
inline constexpr Milliseconds REGISTERED_BANNER_HOLD(100);

//  Watched past the throw's own 8 s: the banner showed 6.9 s in, with
//  frames a second apart.
inline constexpr Milliseconds REGISTERED_BANNER_TAIL(2000);

//  What a catch stop does after a throw at a locked target.
enum class AfterThrow{
    STOP_ENDS,
    //  It broke out into a wild battle: weaken it, then throw.
    WEAKEN,
    //  A miss or a break-free, and a plate not known caught locks on again:
    //  throw again at once, before it wanders off.
    THROW_AGAIN,
    LOOK_AGAIN,
    //  A sweep at the same stop finds the same target again.
    MOVE_ON,
};

//  So a target that keeps dodging does not take all of the patrol's throws.
inline constexpr int THROWS_AT_ONE_TARGET = 3;

//  `relocked`: the plate that locks on with ZL held after the throw.
AfterThrow after_throw(const ThrowSeen& seen, bool battle, const std::optional<LockTarget>& relocked, int throws_at_lock);

//  What a catch stop does after its lock-on sweep.
enum class StopAim{
    MOVE_ON,
    LOOK_AGAIN,
    //  Every plate read is of a species already caught: turn further.
    LOOK_FURTHER,
    THROW,
};

//  `plates`: read with ZL held after the sweep.
StopAim stop_aim(bool locked_on, const std::vector<LockTarget>& plates);

//  A ZL lock-on or throw leaves the camera looking down; a walk and a
//  sweep's turns keep the pitch, so it is reset only after ZL.
class CameraPitch{
public:
    void zl_held(){
        m_tipped = true;
    }

    //  Whether to level the camera now; it counts as level after.
    bool level_now();

private:
    bool m_tipped = false;
};



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
