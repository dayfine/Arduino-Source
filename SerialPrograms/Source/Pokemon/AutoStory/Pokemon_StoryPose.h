/*  Story Pose
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Where the player is believed to be, and how sure the run is of it.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryPose_H
#define PokemonAutomation_Pokemon_StoryPose_H

#include <memory>
#include <set>
#include <string>

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
//  "tile", "column", ...: for log lines.
std::string certainty_name(PoseCertainty certainty);

//  Whether a pose known to `have` is good enough for an event that needs `need`.
bool certainty_covers(PoseCertainty have, PoseCertainty need);

//  Where on a map the player is, in the game's own terms: tiles and a
//  facing in a 2D game, a map point or a fly point in
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
//  "Route 201 (10, 20) facing north (tile)": for log lines.
std::string to_string(const Pose& pose);


//  What a run knows. Flags are set only by confirmed effects and are lost
//  on a reload.
struct RunState{
    Pose pose;
    std::set<std::string> flags;
};



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
