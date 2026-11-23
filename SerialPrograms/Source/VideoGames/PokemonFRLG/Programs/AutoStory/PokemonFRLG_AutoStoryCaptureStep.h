/*  AutoStory Capture Step
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The CAPTURE step's plan, its args in the story table, and the PC step.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryCaptureStep_H
#define PokemonAutomation_PokemonFRLG_AutoStoryCaptureStep_H

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "CommonFramework/Language.h"
#include "CommonFramework/Tools/VideoStream.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "Pokemon/Navigation/Grid/Maps/Pokemon_Grid.h"
#include "PokemonFRLG_AutoStoryPcStorage.h"
#include "PokemonFRLG_AutoStoryStepTypes.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{



//  What a capture step catches, and its bounds.
struct CapturePlan{
    //  The species' slug, e.g. "rattata".
    std::string species = {};
    //  How many to catch.
    size_t catches = 1;
    //  Wild battles, of any species, before the step gives up.
    size_t max_encounters = 20;
    //  Poke Balls thrown before the step gives up.
    size_t max_balls = 5;
    //  The way the player steps to look for a battle, then back: the table
    //  stands the player in the patch with a free tile that way.
    Pokemon::Direction pace = Pokemon::Direction::EAST;
    //  The FIGHT list's move (0-3) to weaken the target with: one that
    //  can't knock it out from green HP. None: only balls.
    std::optional<size_t> weaken_move = std::nullopt;
    //  The most turns spent weakening each target.
    size_t max_weaken_turns = 2;
};

//  The GAME action of a capture step: species WORD (required); catches
//  INTEGER (1); encounters INTEGER (20); balls INTEGER (5); pace DIRECTION
//  (east); move INTEGER (optional, from 1), the move that weakens; turns
//  INTEGER (2).
inline const std::string CAPTURE_ACTION = "capture";
//  `plan` as its action's args.
FrlgArgMap capture_args(const CapturePlan& plan);
//  The plan a CAPTURE_ACTION step's args give; nothing if one is out of
//  range.
std::optional<CapturePlan> capture_plan_of(const FrlgArgMap& args);

//  The table's step that runs `plan`.
StoryStep capture_step(std::string name, std::string why, const CapturePlan& plan);
//  The table's step that runs `transfer` at the PC the player faces.
StoryStep pc_step(std::string name, std::string why, const PcTransfer& transfer);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
