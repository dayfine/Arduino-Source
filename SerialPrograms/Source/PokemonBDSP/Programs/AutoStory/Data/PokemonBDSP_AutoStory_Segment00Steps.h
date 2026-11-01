/*  BDSP AutoStory - Segment 00 as story steps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Segment 00's checkpoints written as the shared story steps
 *  (Pokemon/Navigation/Pokemon_StorySteps.h), plain data for the tests to
 *  plan. The first is 00.04b, the half of 00.04 after the catch; the rest
 *  of the segment is still one wrapped function per checkpoint.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_Segment00Steps_H
#define PokemonAutomation_PokemonBDSP_AutoStory_Segment00Steps_H

#include "Pokemon/Navigation/Pokemon_StorySteps.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  The variants 00.04b's steps name, done by Segment 00's own code.
//  GAME: with Potions in the Bag, the run resumes from the save after
//  them: flags "has_potions" and "potions_saved", and walks back to
//  Route 202's save spot if that save is in Sandgem Town.
extern const char* const STEP_LOCATE_SAVE;
//  SHOP: Potions from the Poke Mart lady on Route 201, who walks us on to
//  Route 202's save spot and saves there (flag "potions_saved"), or after
//  two misses the clerk in Sandgem's Poke Mart.
extern const char* const STEP_SANDGEM_POTIONS;
//  GAME: from Sandgem's front row north through the gap between the
//  signs to Route 202's save spot.
extern const char* const STEP_SIGN_GAP;
//  STEP: north into Jubilife, a tile west at a time until the stairs take
//  us and Dawn's scene starts.
extern const char* const STEP_JUBILIFE_STAIRS;

//  00.04b: from the save after the catch on Route 202 to Dawn in Jubilife:
//  train, heal in Sandgem, Potions, save on Route 202, north past its
//  trainers, Dawn's scene. Poses are in world tiles of story_maps().
const Pokemon::StoryCheckpoint& segment_00_after_catch();



}
}
}
#endif
