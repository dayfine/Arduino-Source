/*  PokemonLZA AutoStory - Catch stop
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The console-free choices of one stop on Wild Zone 1's catch patrol:
 *  whether to stop at a spot at all, what to do after a throw, and when to
 *  reset the camera's pitch (the target choice is in
 *  PokemonLZA_AutoStoryCatchPlan.h).
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryCatchStop_H
#define PokemonAutomation_PokemonLZA_AutoStoryCatchStop_H

#include <optional>
#include <span>
#include "PokemonLZA_AutoStoryCatchPlan.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  What the patrol does at a catch spot.
enum class SpotVisit{
    //  Stops there to catch.
    CATCH,
    //  Turns back to the other spot at once: the walk there is only to
    //  bring the other spot's Pokemon back.
    TURN_BACK,
};

//  At `spot`, with new species caught at the spots in `caught_at`: turn
//  back when one was caught there, as its species is the one there
//  (Scatterbug at 100; the user, console B 2026-11-14); else catch.
SpotVisit spot_visit(int spot, std::span<const int> caught_at);

//  What a catch stop does after a throw at a locked target.
enum class AfterThrow{
    //  A new species (ThrowSeen::new_species()).
    STOP_ENDS,
    //  It broke out into a wild battle: weaken it, then throw.
    WEAKEN,
    //  A miss or a break-free, and a plate not known caught locks on again
    //  with ZL held: throw again at once, before it wanders off. A sweep
    //  after a miss at 60 found nothing (console B, 2026-11-14 17:38).
    THROW_AGAIN,
    //  No plate locks on, or one of a species already caught: look again.
    LOOK_AGAIN,
};

//  What follows a throw that showed `seen`, with a wild battle on
//  (`battle`) or not, and `relocked`: the plate that locks on with ZL held
//  after it (nothing: none).
AfterThrow after_throw(const ThrowSeen& seen, bool battle, const std::optional<LockTarget>& relocked);

//  Throws in a row at one target before the stop looks again, so a target
//  that keeps dodging does not take all of the patrol's throws.
inline constexpr int THROWS_AT_ONE_TARGET = 3;

//  Whether the camera needs its pitch reset (level_camera()) before a
//  sweep for brackets or the walk on: a ZL lock-on or throw leaves it
//  looking down at the ground, while a walk and a sweep's turns keep the
//  pitch. A reset at every stop's start was a needless look down and up
//  (the user, console B 2026-11-14).
class CameraPitch{
public:
    //  ZL was held: a lock-on, a throw or a fight.
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
