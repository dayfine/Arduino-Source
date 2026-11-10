/*  AutoStory Step Table
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  FRLG's story steps as rows of the shared action table
 *  (Pokemon/AutoStory/Pokemon_StoryActions.h): each row names what does
 *  a kind of step, or one variant of it. The console (StoryExecutor) binds
 *  each name to its console action; the tests bind them to a recorder, so
 *  the rows are checked without a console.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryStepTable_H
#define PokemonAutomation_PokemonFRLG_AutoStoryStepTable_H

#include <functional>
#include <span>
#include <string>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Pokemon/AutoStory/Pokemon_StoryActions.h"
#include "Pokemon/AutoStory/Pokemon_StorySteps.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  What does a step: one of FRLG's story actions.
enum class StoryActionId{
    NEW_GAME,
    OPTIONS,
    ROUTE,
    LEGS,
    STEP,
    TALK,
    ANSWER,
    MASH,
    BATTLE,
    HEAL,
    SHOP,
    READY,
    //  CLOSE_TRAINER_CARD: back from SID Helper (close_trainer_card()).
    CLOSE_TRAINER_CARD,
    //  LEAVE_GIFT_SUMMARY: back from Gift RNG (leave_gift_summary()).
    LEAVE_GIFT_SUMMARY,
    //  CUT_TREE_VARIANT: the tree ahead cut, and a step onto its tile.
    CUT_TREE,
    //  TRASH_CANS_VARIANT: the search for Vermilion Gym's switches.
    TRASH_CANS,
    //  Any other GAME step: a capture or a PC deposit or withdrawal
    //  (capture_step(), pc_step() in PokemonFRLG_AutoStoryCapture.h); a
    //  shiny met in a capture stops the program without reloading. A MART,
    //  TEACH, LEVEL, GRIND or fossil CHOICE
    //  (PokemonFRLG_AutoStoryAbilities.h); a GRIND heals by routing to its
    //  target, the nurse, and back.
    GAME,
};
//  "route", "game": the action's name in logs and test traces.
const char* story_action_name(StoryActionId id);

//  A row of FRLG's table: the action for steps of `kind` whose variant is
//  named `variant`, or for every other step of `kind` if it is empty, and
//  the args its steps carry.
struct StoryActionRow{
    Pokemon::StoryStepKind kind;
    std::string variant;
    StoryActionId id;
    std::span<const Pokemon::StoryArgSpec> args = {};
};
//  Every row. TRAIN and SAVE have none: FRLG's story doesn't use them.
const std::vector<StoryActionRow>& story_action_rows();

//  How one binding (the console, a test) does each action.
using StoryActionBinding = std::function<Pokemon::StoryAction(StoryActionId id)>;
//  The rows as a table, each with `bind`'s action for its id.
Pokemon::StoryActionTable make_story_action_table(const StoryActionBinding& bind);


//  The rows against the dispatch they replaced, and every checkpoint linted
//  against them.
void add_tests_AutoStoryStepTable(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
