/*  PokemonLZA AutoStory - Catch plan
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The console-free choices behind Wild Zone 1's catches: which lock-on
 *  target to throw at and whether to walk toward it first (the stops
 *  along the road are in PokemonLZA_AutoStoryZone1Road.h).
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryCatchPlan_H
#define PokemonAutomation_PokemonLZA_AutoStoryCatchPlan_H

#include <optional>
#include <string_view>
#include <vector>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  A wild Pokemon locked on to with ZL held, by its name plate's HP bar:
//  middle and width as shares of the screen; `caught` unset when the mark
//  did not read.
struct LockTarget{
    double x = 0;
    double y = 0;
    double width = 0;
    std::optional<bool> caught;

    std::string_view caught_text() const;
};

//  Only while its plate reads far (its bar above 0.45 of the screen's
//  height): 4 steps at most before the first throw and 2 after each miss.
bool step_toward_target(const LockTarget& target, int misses, int steps);

//  Within 0.03 of the screen both ways.
bool same_lock_target(const LockTarget& a, const LockTarget& b);

//  The one whose plate is lowest on the screen; nothing when all are caught.
std::optional<size_t> nearest_uncaught(const std::vector<LockTarget>& targets);

//  Why a throw at a locked target counts as a new species.
enum class NewSpeciesSign{
    BANNER,
    //  No banner, but the plate read "not caught" before the throw, and after
    //  it no battle is on and no plate locks on. The banner can be missed while
    //  the capture lags.
    PLATE_NOT_CAUGHT,
};

//  What a throw at a locked target showed.
struct ThrowSeen{
    bool registered = false;
    //  Read before the throw.
    std::optional<bool> plate_caught;
    //  After it, no wild battle is on and no plate locks on.
    bool target_gone = false;

    //  Nothing when it did not, or may not have.
    std::optional<NewSpeciesSign> new_species() const;

    std::string_view new_species_text() const;
};

//  The targets seen while flicking right with ZL held. It stops at a view
//  equal to the last (no other target that way) or to the first (the
//  targets went round), or at `max_views`.
class LockTargetCycle{
public:
    explicit LockTargetCycle(size_t max_views);

    //  Returns whether to flick right again.
    bool add(const LockTarget& target);

    const std::vector<LockTarget>& targets() const{
        return m_targets;
    }

    //  Positive: right.
    int flicks_to(size_t index) const;

private:
    size_t m_max_views;
    std::vector<LockTarget> m_targets;
    size_t m_current = 0;
    bool m_done = false;
};



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
