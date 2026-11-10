/*  AutoStory Abilities
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The GAME steps beyond walking and talking that later checkpoints name
 *  in the table: MART (a budgeted buy list), TEACH (a TM or HM), LEVEL (a
 *  member's level checked), GRIND (wild battles to a level) and
 *  CHOICE(fossil). Each is an action of a GAME step; this runs the one a
 *  step names with the game steps it is given.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryAbilities_H
#define PokemonAutomation_PokemonFRLG_AutoStoryAbilities_H

#include <optional>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "CommonFramework/Tools/VideoStream.h"
#include "Pokemon/AutoStory/Pokemon_StoryEvents.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepExecutor.h"
#include "PokemonFRLG_AutoStoryChoice.h"
#include "PokemonFRLG_AutoStoryGrind.h"
#include "PokemonFRLG_AutoStoryMart.h"
#include "PokemonFRLG_AutoStoryMember.h"
#include "PokemonFRLG_AutoStoryTeach.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The game steps the abilities take.
struct StoryAbilities{
    MemberReader members;
    MartActions& mart;
    TeachActions& teach;
    GrindActions& grind;
    ChoiceActions& choice;
};

//  A MART, TEACH, LEVEL, GRIND or fossil step with `abilities`: nothing for
//  another action. A GRIND that meets a shiny throws
//  ProgramFinishedException, with a screenshot from `stream` if given, so
//  the segment stops without reloading. Any other end but success is a
//  failed Outcome; a MART that buys nothing because the money doesn't
//  cover it, and a TEACH whose TM isn't in the TM CASE, succeed, with the
//  reason logged.
std::optional<Pokemon::Outcome> run_ability_game_step(
    Logger& logger, VideoStream* stream, const Pokemon::StoryStep& step, StoryAbilities& abilities
);


//  These steps' tests, with the Mart's, TEACH's, GRIND's and the fossil's.
void add_tests_AutoStoryAbilities(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
