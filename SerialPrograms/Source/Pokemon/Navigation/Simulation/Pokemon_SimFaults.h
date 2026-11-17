/*  Sim Faults
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The faults the games' overworld simulations inject, shared by BDSP's
 *  and FRLG's: the classes of failure the consoles showed, and a counter
 *  per class that says which of a fault's chances to fault. A chance is
 *  each time the sim reaches a point where the game could fail that way
 *  (a tap, a warp, a step's end); a test names the chances it faults by
 *  number, from 1 in the order the sim reaches them.
 *
 *  How a class is injected is each game's (its sim's knobs and its fault
 *  sweep); a game may inject only some of the classes.
 *
 */

#ifndef PokemonAutomation_Pokemon_SimFaults_H
#define PokemonAutomation_Pokemon_SimFaults_H

#include <map>
#include <set>
#include <string_view>
#include <vector>

namespace PokemonAutomation{
namespace Pokemon{


//  A class of failure a console showed, which a sim can inject.
enum class FaultClass{
    //  A step whose end the game sets (a talk, a scene, a battle) leaves the
    //  player on another walkable tile next to the expected one.
    SCENE_END_POSE,
    //  A scene starts on the last tiles of the walk before it.
    SCENE_EARLY,
    //  A battle's opening is still playing when a step starts.
    BATTLE_OPENING,
    //  Wild battles while walking, more than a clean run meets.
    WILD_BATTLES,
    //  A warp's fade shows late.
    LATE_FADE,
    //  Every warp's fade-in runs long: a move right after it goes nowhere.
    SLOW_FADE_IN,
    //  A step onto a warp shows the fade's black but does not warp.
    WARP_NO_OP,
    //  A dark cave walked without Flash.
    DARK_CAVE,
    //  A tap the game drops.
    LOST_TAP,
    //  A step tap that moved the player, read as not moved.
    UNSEEN_STEP,
    //  A leg that rides (the Bicycle) starts with the player off the mount,
    //  walking at foot pace.
    MOUNT_LOST,
};

//  Every fault class, in the enum's order.
const std::vector<FaultClass>& fault_classes();

//  The class's name for logs: "scene end pose".
std::string_view fault_class_name(FaultClass fault);


//  Which chances of one fault to fault: those numbered in `faulted`,
//  counted from 1 as next() reaches them.
class FaultOccurrences{
public:
    FaultOccurrences() = default;
    explicit FaultOccurrences(std::set<int> faulted);

    //  Counts one more chance: whether it is one to fault.
    bool next();

    //  The chances counted so far.
    int chances() const{
        return m_chances;
    }
    //  How many of them were faulted.
    int faulted() const{
        return m_faulted;
    }

private:
    std::set<int> m_numbers;
    int m_chances = 0;
    int m_faulted = 0;
};


//  A FaultOccurrences per fault class: a class not given counts its
//  chances and faults none.
class SimFaults{
public:
    SimFaults() = default;
    explicit SimFaults(std::map<FaultClass, std::set<int>> faulted);

    //  Counts one more chance of `fault`: whether it is one to fault.
    bool next(FaultClass fault);

    //  The chances of `fault` counted so far, and how many were faulted.
    int chances(FaultClass fault) const;
    int faulted(FaultClass fault) const;

private:
    std::map<FaultClass, FaultOccurrences> m_classes;
};



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
