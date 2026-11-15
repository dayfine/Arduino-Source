/*  PokemonLZA AutoStory - Catch plan
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The console-free choices behind Wild Zone 1's catches: which lock-on
 *  target to throw at and whether to walk toward it first (the brackets
 *  to turn to are in PokemonLZA_AutoStoryBracketWatch.h, the stops along
 *  the road in PokemonLZA_AutoStoryZone1Road.h).
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryCatchPlan_H
#define PokemonAutomation_PokemonLZA_AutoStoryCatchPlan_H

#include <optional>
#include <string_view>
#include <vector>
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckLevel.h"

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

    //  The caught mark, for the log.
    std::string_view caught_text() const;
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

//  Why a throw at a locked target counts as a new species.
enum class NewSpeciesSign{
    //  The Pokedex's "Registered!" banner showed.
    BANNER,
    //  No banner, but the target's plate read "not caught" before the
    //  throw, and after it no wild battle is on and no plate locks on: the
    //  ball caught it. The banner was missed once while the capture lagged
    //  (03.08 stop 1, console B 2026-11-14), though the next stops read
    //  that species' plate as caught.
    PLATE_NOT_CAUGHT,
};

//  What a throw at a locked target showed.
struct ThrowSeen{
    //  The banner showed.
    bool registered = false;
    //  The target's caught mark, read before the throw (nothing: unread).
    std::optional<bool> plate_caught;
    //  After it, no wild battle is on and no plate locks on.
    bool target_gone = false;

    //  Whether the throw caught a new species, and by which sign; nothing
    //  when it did not, or may not have.
    std::optional<NewSpeciesSign> new_species() const;

    //  What new_species() says, for the log.
    std::string_view new_species_text() const;
};

//  What catch_in_zone()'s stops did: new species caught (`found_by_plate`
//  of them by NewSpeciesSign::PLATE_NOT_CAUGHT, the rest by the banner),
//  throws, and locks on that found only species already caught.
struct GrassCatches{
    int found = 0;
    int found_by_plate = 0;
    int throws = 0;
    int caught_seen = 0;

    GrassCatches& operator+=(const GrassCatches& other){
        found += other.found;
        found_by_plate += other.found_by_plate;
        throws += other.throws;
        caught_seen += other.caught_seen;
        return *this;
    }
};

//  How sure catch_in_zone()'s own count of new species is.
enum class SpeciesTally{
    //  All wanted, each by the Pokedex's "Registered!" banner.
    BANNERS,
    //  All wanted, some only by a plate read "not caught" before the throw;
    //  or none caught and none thrown at, only species already caught
    //  locking on (a rerun from a save after the catches): taken as done.
    INFERRED,
    //  Fewer than wanted (a banner may have been missed).
    SHORT,
};

//  How sure `patrol`'s count of new species is, `wanted` of them asked.
SpeciesTally species_tally(const GrassCatches& patrol, int wanted);

//  Whether to open the Pokedex and read its count: at THOROUGH and TESTING
//  checks always, at the others when the tally is not all banners.
bool read_dex_count(Pokemon::StoryCheckLevel checks, SpeciesTally tally);

//  Whether the zone's quest is done: the Pokedex's count, when read, has
//  `dex_needed`; unread, the tally is not SHORT.
bool enough_species(SpeciesTally tally, std::optional<int> dex_count, int dex_needed);

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



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
