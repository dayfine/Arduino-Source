/*  Story Pose
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Where the player is believed to be.
 *  This is the event runner's pose (RunState, Outcome). The story engine
 *  carries a typed pose of its own (the StoryPose concept,
 *  Engine/Pokemon_StoryPoseConcept.h, e.g. ZonePose); a game converts it.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryPose_H
#define PokemonAutomation_Pokemon_StoryPose_H

#include <memory>
#include <set>
#include <string>

namespace PokemonAutomation{
namespace Pokemon{


//  Where on a map the player is, in the game's own terms (a game derives
//  its own: tiles, a spot in a zone, and how sure it is of them). The engine only carries and prints it; the game's
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
    std::string map = {};
    std::shared_ptr<const Position> where = {};
};
//  The map, then the position's description: for log lines.
std::string to_string(const Pose& pose);


//  What a run knows. Flags are set only by confirmed effects and are lost
//  on a reload.
struct RunState{
    Pose pose = {};
    std::set<std::string> flags = {};
};



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
