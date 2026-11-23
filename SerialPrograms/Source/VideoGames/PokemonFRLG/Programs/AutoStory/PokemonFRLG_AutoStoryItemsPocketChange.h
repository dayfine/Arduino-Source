/*  AutoStory Items Pocket Change
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What each story action does to the Items pocket, which a battle's items
 *  are used from by their rows: one row per S11 action name, so the counts a
 *  battle trusts are read in this run after the last step that changed the
 *  pocket uncounted (Supplies::counts_read).
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryItemsPocketChange_H
#define PokemonAutomation_PokemonFRLG_AutoStoryItemsPocketChange_H

#include <optional>
#include <string_view>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  What a step does to the Items pocket.
enum class ItemsPocketChange{
    NONE,
    COUNTED,
    STALE,
};

//  The change a step with S11 action name `action` makes; nothing for an
//  action without a row, which the story takes as STALE.
std::optional<ItemsPocketChange> items_pocket_change(std::string_view action);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
