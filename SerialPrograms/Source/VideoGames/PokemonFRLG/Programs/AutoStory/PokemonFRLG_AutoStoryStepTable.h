/*  AutoStory Step Table
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  FRLG's story steps dispatched by S11 action name (StoryActionId): the
 *  console (StoryExecutor) binds each id to its console action; simulations
 *  and tests bind them to their own handlers.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryStepTable_H
#define PokemonAutomation_PokemonFRLG_AutoStoryStepTable_H

#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "PokemonFRLG_AutoStoryStepTypes.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


inline constexpr std::string_view NUGGET_FARM_ACTION = "nugget_farm";

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
    CLOSE_TRAINER_CARD,
    LEAVE_GIFT_SUMMARY,
    CUT_TREE,
    PUSH_BOULDER,
    TRASH_CANS,
    GAME,
};

//  "route", "game": the action's name in logs and test traces.
std::string_view story_action_name(StoryActionId id);

//  Maps an S11 action name ("route", "scene", "cut_tree", "mart", ...) to its
//  StoryActionId.
StoryActionId story_action_id_of(std::string_view action);

//  How one binding (the console, a sim, a test) does each action id.
using StoryActionBinding = std::function<FrlgStepAction(StoryActionId id)>;

//  Action table keyed by S11 action name.
using FrlgActionTable = std::map<std::string, FrlgStepAction, std::less<>>;

//  Builds an action table covering every registered FRLG S11 action name.
FrlgActionTable make_story_action_table(const StoryActionBinding& bind);


//  Registers step table unit tests.
void add_tests_AutoStoryStepTable(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
