/*  PokemonLZA AutoStory - Bracket watch
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The console-free choice of which wild Pokemon to turn to at a catch
 *  stop, from the lock-on brackets seen there (LockOnBracketDetector).
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryBracketWatch_H
#define PokemonAutomation_PokemonLZA_AutoStoryBracketWatch_H

#include <chrono>
#include <optional>
#include <vector>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Brackets seen around a wild Pokemon (find_lock_on_brackets()): their
//  middle and height as shares of the screen.
struct BracketSighting{
    double x = 0;
    double y = 0;
    double height = 0;
};

//  The brackets seen while standing at a stop, one frame at a time. The
//  game moves them between Pokemon as they walk about, so the watch goes
//  on `settle` after the first sighting, to see the others, and ends at
//  `limit` when there is none.
class BracketWatch{
public:
    BracketWatch(std::chrono::milliseconds settle, std::chrono::milliseconds limit);

    //  What a frame `elapsed` after the start showed: nothing when no
    //  brackets.
    void add(std::chrono::milliseconds elapsed, const std::optional<BracketSighting>& sighting);

    //  Whether to stop watching.
    bool done() const;

    //  The tallest brackets seen: the nearest Pokemon, the likeliest to be
    //  caught (the user, 2026-11-13). Nothing when none showed.
    std::optional<BracketSighting> nearest() const;

    const std::vector<BracketSighting>& sightings() const{
        return m_sightings;
    }

private:
    std::chrono::milliseconds m_settle;
    std::chrono::milliseconds m_limit;
    std::chrono::milliseconds m_last{0};
    std::optional<std::chrono::milliseconds> m_first_seen;
    std::vector<BracketSighting> m_sightings;
};

//  A turn of the camera with the right stick: `stick_x` (right when
//  positive) held for `time`.
struct CameraTurn{
    double stick_x = 0;
    std::chrono::milliseconds time{0};
};

//  The turn that brings what is at `x` (0 to 1 across the screen) near the
//  middle: none within 0.06 of it, else 0.4 toward it for 80 ms plus up to
//  400 ms (a quarter of the screen off or more), as the walk turns to the
//  objective marker.
CameraTurn camera_turn_toward(double x);

//  One look of a catch stop's camera sweep: the turn before it, and whether
//  to watch for brackets after it.
struct SweepLook{
    CameraTurn turn;
    bool watch = true;
};

//  A catch stop's sweep from facing along the road: a look straight on,
//  then `steps` turns of `step` toward the grass right of the road with a
//  look after each, then `steps` turns back with no look, so the camera
//  ends where it started. From mid-road, the user's catches were in that
//  grass (Mareep) or on the road (Scatterbug), the camera turned right
//  (console B, 2026-11-13); the patrol facing along the road saw none.
std::vector<SweepLook> camera_sweep(CameraTurn step, int steps);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
