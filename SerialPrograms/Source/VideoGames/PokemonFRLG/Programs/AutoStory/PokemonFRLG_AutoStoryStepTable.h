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
#include "Pokemon/AutoStory/Pokemon_BuiltinStoryActions.h"
#include "Pokemon/AutoStory/Pokemon_StoryActions.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepExecutor.h"

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
    //  A GAME step: one only FRLG has.
    GAME,
};
//  "route", "game": the action's name in logs and test traces.
std::string_view story_action_name(StoryActionId id);

//  A row of FRLG's table: the action for steps of `kind` whose variant is
//  named `variant`, or for every other step of `kind` if it is empty.
struct StoryActionRow{
    Pokemon::StoryStepKind kind;
    std::string variant;
    StoryActionId id;
};
//  Every row. TRAIN and SAVE have none: FRLG's story doesn't use them.
const std::vector<StoryActionRow>& story_action_rows();

//  The actions FRLG's steps name, with the args each takes: the built-in
//  ones (Pokemon_BuiltinStoryActions.h), then a ROUTE and a LEGS named
//  "run" (RUN_VARIANT), which take the same args and run with B held,
//  then FRLG's GAME actions: closing the Trainer Card or the Gift RNG
//  summary, a PC transfer, a capture, the fossil, a LEVEL check, a Mart,
//  a TEACH and a GRIND, each with the args it declares.
//  A story file is read against these.
std::span<const Pokemon::StoryActionSpec> story_action_specs();

//  How one binding (the console, a test) does each action.
using StoryActionBinding = std::function<Pokemon::StoryAction(StoryActionId id)>;
//  The rows as a table, each with `bind`'s action for its id and its
//  spec's args, and each of FRLG's GAME actions with the GAME row's action
//  and its own args.
Pokemon::StoryActionTable make_story_action_table(const StoryActionBinding& bind);


//  The rows against the dispatch they replaced, and every checkpoint linted
//  against them.
void add_tests_AutoStoryStepTable(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
