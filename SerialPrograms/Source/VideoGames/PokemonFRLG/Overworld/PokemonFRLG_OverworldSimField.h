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
    //  Surf onto the water the player faces, from ground beside it: the
    //  question, "<MON> used SURF!" and the jump (SimTiming::surf_start).
    //  Each step on the water then takes SimTiming::surf_tile, and a step
    //  onto land gets the player off.
    SURF,
    //  Not A: Fly from the party menu to the town numbered `floor` in
    //  kanto_fly_towns(), outdoors: the fade, then the player on its landing
    //  tile facing south, still for SimTiming::fly.
    FLY,
    //  The card-key gate the player faces (a `G` tile of a map's `gates`),
    //  with SimScript::card_key_held: its message, and the gate is open.
    OPEN_GATE,
    //  The statue the player faces (a map's `switches`): the prompt, YES, and
    //  the switch flips, with every gate that follows it on every map.
    PRESS_SWITCH,
};

//  Register the tests of the Bicycle's taps and SELECT on a made-up map.
void add_tests_OverworldBike(UnitTestDatabase& database);


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
