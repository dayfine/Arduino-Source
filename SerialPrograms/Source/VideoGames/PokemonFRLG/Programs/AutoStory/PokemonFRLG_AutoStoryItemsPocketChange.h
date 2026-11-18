/*  AutoStory Items Pocket Change
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What each story step does to the Items pocket, which a battle's items
 *  are used from by their rows: one row per action, so the counts a battle
 *  trusts are read in this run after the last step that changed the
 *  pocket uncounted (Supplies::counts_read).
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryItemsPocketChange_H
#define PokemonAutomation_PokemonFRLG_AutoStoryItemsPocketChange_H

#include <optional>
#include <string_view>
#include "Pokemon/AutoStory/Types/Pokemon_StoryStepKind.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  What a step does to the Items pocket.
enum class ItemsPocketChange{
    //  Nothing in it changes.
    NONE,
    //  Items used, each taken off the story's counts as it goes
    //  (spend_item()), or the pocket read and its counts taken.
    COUNTED,
    //  Items come or go that the story doesn't count (a Mart, a gift, an
    //  item ball, a use_item step): the counts and rows after it are a
    //  guess, and the next battle reads the pocket first.
    STALE,
};

//  The change a step of `kind` done by `action` (StoryStep::action, empty
//  for the kind's own) makes; nothing for an action without a row, which
//  the story takes as STALE. Every action story_action_specs() names has
//  a row.
std::optional<ItemsPocketChange> items_pocket_change(Pokemon::StoryStepKind kind, std::string_view action);


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
