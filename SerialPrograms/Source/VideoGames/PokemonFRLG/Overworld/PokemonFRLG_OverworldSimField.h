/*  Overworld Sim Field Actions
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What the overworld sim does for A (or SELECT) on the field, and the
 *  tests of the Bicycle's taps and SELECT in the sim.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_OverworldSimField_H
#define PokemonAutomation_PokemonFRLG_OverworldSimField_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  What A does on the overworld, facing something (OverworldSim::use_field()).
enum class SimField{
    //  Cuts the tree the player faces (an object named "cut_tree") with
    //  YES to the game's question: it is gone until the player next warps,
    //  as its hide flag is a FLAG_TEMP.
    CUT_TREE,
    //  The poster the player faces (an object named "poster"): the switch
    //  behind it opens the stairs, which takes away the objects named
    //  "poster_stairs" for good (pokefirered
    //  CeladonCity_GameCorner_EventScript_Poster's setmetatile). The sim
    //  adds both to the Game Corner from code (add_coded_objects()).
    POSTER_SWITCH,
    //  The panel of the lift the player stands in (SimScript::lift_floors):
    //  the floor picked, the lift's door now lets out there.
    LIFT_FLOOR,
    //  Not A: SELECT, which with the Bicycle registered gets on it where
    //  running is allowed, or off it, in about
    //  MovementMechanics::BIKE_SELECT_TIME. (PokemonFRLG_OverworldSimTaps.cpp)
    BIKE_SELECT,
};

//  Register the tests of the Bicycle's taps and SELECT on a made-up map.
void add_tests_OverworldBike(UnitTestDatabase& database);


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
