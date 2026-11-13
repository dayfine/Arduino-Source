/*  Story Pose
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Split out of Pokemon_StoryEvents.h, unchanged; it includes this header.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryPose_H
#define PokemonAutomation_Pokemon_StoryPose_H

#include <cstddef>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/MoveOnlyFunction.h"
#include "Pokemon/Navigation/Pokemon_GatedTypes.h"
#include "Pokemon/Navigation/Pokemon_GateState.h"
#include "Pokemon_StoryLog.h"

namespace PokemonAutomation{
namespace Pokemon{


//  How much of the pose is known. COLUMN and ROW pin one axis (a wall, a
//  door); SET means one of the positions the pose holds.
enum class PoseCertainty{
    TILE,
    COLUMN,
    ROW,
    SET,
    UNKNOWN,
};
// Returns the string label for a pose certainty level.
std::string certainty_name(PoseCertainty certainty);

//  Whether a pose known to `have` is good enough for an event that needs `need`.
bool certainty_covers(PoseCertainty have, PoseCertainty need);

//  Where on a map the player is, in the game's own terms: tiles and a
//  facing in a 2D game (Pokemon_TilePose.h), a map point or a fly point in
//  an open-world one. The engine only carries and prints it; the game's
//  actions read it back.
struct Position{
    virtual ~Position() = default;
    //  The text after the map in a log line, e.g. "(10, 20) facing north";
    //  none for a game that has nothing to say beyond the map.
    virtual std::string describe() const{
        return "";
    }
};

//  Where the player is believed to be. The screen is read; the pose is
//  tracked, and only an anchor (a wall, a door, an NPC who answered) makes
//  it certain. `where` is null until the game learns a position and after
//  the pose is lost; it is shared because poses are copied (an event's
//  learned pose into the run) and never changed in place.
struct Pose{
    std::string map;
    PoseCertainty certainty = PoseCertainty::UNKNOWN;
    std::shared_ptr<const Position> where;
};
// Formats a Pose as a human-readable string.
std::string to_string(const Pose& pose);


//  What a run knows. Flags are set only by confirmed effects and are lost
//  on a reload.
struct RunState{
    Pose pose;
    std::set<std::string> flags;
    //  The gates open and the switches on (Pokemon_GateState.h): from the
    //  checkpoint's start_gates, then each step's gates_after as it is done.
    GateState gates;
};

//  The rules a plan over `state`'s gates follows: the flag "card_key_held"
//  (set after the Card Key is picked up) lets it open card-key gates.
inline GateRules gate_rules_of(const RunState& state){
    return GateRules{.card_key_held = state.flags.contains("card_key_held")};
}



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
