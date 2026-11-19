/*  Story Pose
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Where the player is believed to be, and what a run knows.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryPose_H
#define PokemonAutomation_Pokemon_StoryPose_H

#include <memory>
#include <set>
#include <string>

namespace PokemonAutomation{
namespace Pokemon{


//  Where on a map the player is, in the game's own terms; each game derives
//  its own.
struct Position{
    virtual ~Position() = default;
    //  The text after the map in a log line; empty for none.
    virtual std::string describe() const{
        return "";
    }
};

//  Where the player is believed to be.
struct Pose{
    std::string map = {};
    //  Null while the position is unknown. Never changed in place.
    std::shared_ptr<const Position> where = {};
};
//  The map, then the position's description: for log lines.
std::string to_string(const Pose& pose);


//  What a run knows; a reload empties it.
struct RunState{
    Pose pose = {};
    std::set<std::string> flags = {};
};



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
