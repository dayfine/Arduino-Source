/*  PokemonLZA AutoStory - Catch plan
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The console-free choices behind Wild Zone 1's catches: which lock-on
 *  target to throw at, whether to walk toward it first, and where along
 *  the road to look.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryCatchPlan_H
#define PokemonAutomation_PokemonLZA_AutoStoryCatchPlan_H

#include <optional>
#include <vector>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  A wild Pokemon locked on to while ZL is held, seen by its name plate's
//  HP bar: the bar's middle and width as shares of the screen (0 to 1),
//  and whether its species is already caught (nothing when the mark did
//  not read).
struct LockTarget{
    double x = 0;
    double y = 0;
    double width = 0;
    std::optional<bool> caught;
};

//  A plate whose bar sits above this share of the screen's height is a far
//  target: walk toward it before throwing. Nearer Pokemon stand lower on
//  the screen (the ground rises toward the horizon), and their plates with
//  them. A guess from console B's frames (2026-11-13): the Mareep a throw
//  fell short of stood at about 0.40 of the height, so its plate higher.
inline constexpr double FAR_PLATE_Y = 0.30;

//  Whether `target` is far enough to walk toward before a throw.
bool lock_target_far(const LockTarget& target);

//  Whether `a` and `b` are the same plate where it stood: within 0.03 of
//  the screen both ways.
bool same_lock_target(const LockTarget& a, const LockTarget& b);

//  The nearest target not known caught among `targets`: the one whose
//  plate is lowest on the screen; nothing when all are caught.
std::optional<size_t> nearest_uncaught(const std::vector<LockTarget>& targets);

//  The targets seen while flicking the right stick right with ZL held,
//  one view after each flick. It stops at a view equal to the last (no
//  other target that way) or to the first (the targets went round), or at
//  `max_views`.
class LockTargetCycle{
public:
    explicit LockTargetCycle(size_t max_views);

    //  Adds the target seen after the last flick (or the first lock).
    //  Returns whether to flick right again.
    bool add(const LockTarget& target);

    const std::vector<LockTarget>& targets() const{
        return m_targets;
    }

    //  Flicks from the target locked now to `index`: right when positive,
    //  left when negative.
    int flicks_to(size_t index) const;

private:
    size_t m_max_views;
    std::vector<LockTarget> m_targets;
    //  Where the lock is now, among m_targets.
    size_t m_current = 0;
    bool m_done = false;
};

//  The distances from the objective marker to look for wild Pokemon at:
//  from `near` back to `far` and forward to `near` again, every `step`,
//  `laps` times. Species stand at different distances from Wild Zone 1's
//  far gate (the user, 2026-11-13).
std::vector<int> patrol_stops(int near, int far, int step, int laps);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
