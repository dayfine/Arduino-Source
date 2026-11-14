/*  AutoStory Grind Step
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The GRIND step's plan and its args in the story table.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryGrindStep_H
#define PokemonAutomation_PokemonFRLG_AutoStoryGrindStep_H

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "CommonFramework/Language.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepExecutor.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryActionSpec.h"
#include "Pokemon/Navigation/Pokemon_Grid.h"
#include "PokemonFRLG_AutoStoryBattle.h"
#include "PokemonFRLG_AutoStoryBattleData.h"
#include "PokemonFRLG_AutoStoryCapture.h"
#include "PokemonFRLG_AutoStoryMember.h"
#include "PokemonFRLG_AutoStoryParty.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Configuration for an auto-story grinding session.
struct GrindPlan{
    //  The party slot that grinds: the lead (0) only.
    size_t member = 0;
    //  The level to reach.
    unsigned level = 2;
    //  Wild battles, of any species, before the step gives up.
    size_t max_encounters = 60;
    //  The way the player steps to look for a battle, then back.
    Pokemon::Direction pace = Pokemon::Direction::EAST;
    //  Battles between visits to the nurse; 0: only when hurt.
    size_t heal_every = 0;
};

//  The GAME action of a GRIND step: slot INTEGER (required, from 1);
//  level INTEGER (required, 2-100); encounters INTEGER (60); pace
//  DIRECTION (east); heal_every INTEGER (0); nurse GRID_POSE (required),
//  where the step heals.
inline const std::string GRIND_ACTION = "grind";
//  `plan` and `nurse` as its action's args.
Pokemon::StoryActionArgs grind_args(const GrindPlan& plan, Pokemon::GridPose nurse);
//  The plan a GRIND_ACTION step's args give; nothing if one is out of
//  range.
std::optional<GrindPlan> grind_plan_of(const Pokemon::StoryActionArgs& args);

//  The table's GRIND step: `nurse` is where the player faces the nurse.
Pokemon::StoryStep grind_step(std::string name, std::string why, const GrindPlan& plan, Pokemon::GridPose nurse);

//  The GAME action that checks a GRIND step's plan before the walk out to
//  its patch: GRIND_ACTION's args but the nurse.
inline const std::string GRIND_CHECK_ACTION = "grind_check";
//  `plan` as GRIND_CHECK_ACTION's args; grind_plan_of() reads them back.
Pokemon::StoryActionArgs grind_check_args(const GrindPlan& plan);

//  The table's check before the walk out to a GRIND step's patch: when the
//  lead already has what `plan` grinds for, the step sets a run flag named
//  `name`, which the walk out, the GRIND step and the way back skip_if.
Pokemon::StoryStep grind_check_step(std::string name, std::string why, const GrindPlan& plan);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
