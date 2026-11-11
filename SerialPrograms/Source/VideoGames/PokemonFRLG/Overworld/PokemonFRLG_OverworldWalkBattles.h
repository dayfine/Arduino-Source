/*  Overworld Walk Battles
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The walker's checks for a wild battle it didn't see start: after a leg,
 *  before one, and after a warp's fade.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_OverworldWalkBattles_H
#define PokemonAutomation_PokemonFRLG_OverworldWalkBattles_H

#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "PokemonFRLG_OverworldRoute.h"
#include "PokemonFRLG_OverworldWalkMoves.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{
namespace OverworldWalkSteps{


//  After `step`'s hold and the watch after it, which saw nothing: if the
//  screen shows a wild battle on its way anyway (its text box before the HP
//  boxes, or black), watches on for it with `wild` (the interrupts for a
//  step where a wild battle can start) and clears what shows. Returns how
//  many it cleared: the leg may have stopped part-way. After any walk not
//  onto a warp, whose fade check_warp_was_no_battle() covers, also where
//  the maps see no wild tiles (RouteStep::wild): they miss some. Mt. Moon
//  B2F's tiles closed on one side are cave floor too, and a battle on
//  "east 7 (to wall)" there left the screen black after the leg, which
//  was taken as walked (console B, 2026-11-03, row 00.13).
size_t clear_unseen_battle(
    ConsoleHandle& console, ProControllerContext& context,
    const OverworldInterrupts& wild, const RouteStep& step
);

//  Before `step`: if the screen shows a wild battle on its way (as
//  clear_unseen_battle()), set off before the step started, watches on for
//  it with `wild` and clears what shows. Returns how many it cleared: the
//  step before may have stopped part-way.
size_t clear_battle_before(
    ConsoleHandle& console, ProControllerContext& context,
    const OverworldInterrupts& wild, const RouteStep& step
);

//  After a warp's fade: if it was a wild battle's, runs from the battle and
//  returns true.
bool run_from_battle_after_warp(ConsoleHandle& console, ProControllerContext& context);

//  Whether `screen` is the overworld of a cave dark without Flash (Rock
//  Tunnel): black around a lit circle on the player in the middle. The
//  battle text box detector and the black screen check read its black as
//  a battle on its way (console B, 2026-11-09, 00.28: the check before
//  each step ran a 15 s battle loop on it, "not a battle", and an open
//  leg's stop was placed by it 0 to 3 tiles in, where the battle came at
//  the leg's end). A battle's wipe is black in the middle too; its text
//  box and HP boxes aren't black in the corners.
bool dark_cave_screen(const ImageViewRGB32& screen);

//  After `step`'s warp: fails, once the battle is fled, if its fade was a
//  wild battle's.
void check_warp_was_no_battle(ConsoleHandle& console, ProControllerContext& context, const RouteStep& step);



} // namespace OverworldWalkSteps
} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
