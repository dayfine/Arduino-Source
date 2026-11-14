/*  PokemonLZA AutoStory - Catch plan
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The console-free choices behind Wild Zone 1's catches: which lock-on
 *  target to throw at, whether to walk toward it first, and where along
 *  the road to look (the brackets to turn to are in
 *  PokemonLZA_AutoStoryBracketWatch.h).
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryCatchPlan_H
#define PokemonAutomation_PokemonLZA_AutoStoryCatchPlan_H

#include <optional>
#include <span>
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

//  Wild Zone 1's two catch spots, as distances from the objective marker,
//  where the user caught on a walk by hand (console B, 2026-11-13): 96,
//  the plaza by the green advertising column (Scatterbug on the road), and
//  57, just before the far gate (Mareep in the grass right of the road and
//  on it).
inline constexpr int CATCH_SPOTS[] = {96, 57};

//  `visits` stops going back and forth over `spots`: {96, 57, 96, ...}.
std::vector<int> alternating_stops(std::span<const int> spots, size_t visits);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
