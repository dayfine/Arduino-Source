/*  PokemonLZA AutoStory - Catch plan
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Pure target selection, lock-on cycling, throw-outcome, and stop-control
 *  decisions for Wild Zone 1 catches.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryCatchPlan_H
#define PokemonAutomation_PokemonLZA_AutoStoryCatchPlan_H

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Locked-on wild Pokemon name-plate geometry and caught-mark reading.
struct LockTarget{
    double x = 0;
    double y = 0;
    double width = 0;
    std::optional<bool> caught;

    std::string_view caught_text() const;
};

//  True when the player should step closer to `target` given prior `misses` and `steps`.
bool step_toward_target(const LockTarget& target, int misses, int steps);

//  True when two lock-on plates refer to the same target within position tolerance.
bool same_lock_target(const LockTarget& a, const LockTarget& b);

//  Index of the nearest uncaught target in `targets`, or std::nullopt if all are caught.
std::optional<size_t> nearest_uncaught(const std::vector<LockTarget>& targets);

//  Evidence that a throw registered a new species.
enum class NewSpeciesSign{
    BANNER,
    PLATE_NOT_CAUGHT,
};

//  Observations collected around a single throw at a locked target.
struct ThrowSeen{
    bool registered = false;
    std::optional<bool> plate_caught;
    bool target_gone = false;

    std::optional<NewSpeciesSign> new_species() const;
    std::string_view new_species_text() const;
};

//  Records targets seen while cycling right under ZL until wrap-around or `max_views`.
class LockTargetCycle{
public:
    explicit LockTargetCycle(size_t max_views);

    //  Records `target`; returns true if cycling should continue.
    bool add(const LockTarget& target);

    const std::vector<LockTarget>& targets() const{
        return m_targets;
    }

    //  Signed flick count from current target to `index` (positive = right).
    int flicks_to(size_t index) const;

private:
    size_t m_max_views;
    std::vector<LockTarget> m_targets;
    size_t m_current = 0;
    bool m_done = false;
};

//  Patrol decision at a catch spot.
enum class SpotVisit{
    CATCH,
    TURN_BACK,
};

//  Returns TURN_BACK if `spot` is already in `caught_at`, otherwise CATCH.
SpotVisit spot_visit(int spot, std::span<const int> caught_at);

//  Next action after a throw at a locked target.
enum class AfterThrow{
    STOP_ENDS,
    WEAKEN,
    THROW_AGAIN,
    LOOK_AGAIN,
    MOVE_ON,
};

//  Decides next action from throw observations, battle state, relock plate, and throw count.
AfterThrow after_throw(const ThrowSeen& seen, bool battle, const std::optional<LockTarget>& relocked, int throws_at_lock);

//  Next action after a lock-on sweep at a catch stop.
enum class StopAim{
    MOVE_ON,
    LOOK_AGAIN,
    LOOK_FURTHER,
    THROW,
};

//  Decides whether to throw, look again, turn further, or move on from sweep `plates`.
StopAim stop_aim(bool locked_on, const std::vector<LockTarget>& plates);

//  Tracks whether ZL has tipped camera pitch down and needs leveling.
class CameraPitch{
public:
    void zl_held(){
        m_tipped = true;
    }

    //  Returns true once if camera pitch needs leveling, then clears the flag.
    bool level_now();

private:
    bool m_tipped = false;
};



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
