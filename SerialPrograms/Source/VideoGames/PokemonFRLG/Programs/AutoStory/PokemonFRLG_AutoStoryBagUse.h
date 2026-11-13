/*  AutoStory Bag Use
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Using and registering items from the bag, and renewing a Repel, on
 *  the console or a test's bag.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryBagUse_H
#define PokemonAutomation_PokemonFRLG_AutoStoryBagUse_H

#include <string_view>
#include <vector>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "CommonFramework/Language.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "PokemonFRLG_AutoStoryBag.h"
#include "PokemonFRLG_AutoStoryStartMenu.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Opens the item's pocket, finds it and uses it, then back to the
//  overworld.
ItemUseEnd use_item_from_bag(Logger& logger, ItemUseActions& actions, std::string_view item);

//  `item` used up to `count` times, one bag visit each: as many as the bag
//  has. `end` is USED if any was used and the bag ran out; else why none was.
struct ItemUses{
    ItemUseEnd end;
    size_t used;
};
//  Uses `item` up to `count` times from the bag.
ItemUses use_items_from_bag(Logger& logger, ItemUseActions& actions, std::string_view item, size_t count);

//  The same, REGISTERing it for SELECT (the Bicycle) instead of USE.
ItemUseEnd register_item_in_bag(Logger& logger, ItemUseActions& actions, std::string_view item);

//  After "REPEL's effect wore off..." has closed: FRLG asks nothing, so
//  another of the same Repel is used if the bag has one. NONE_LEFT is no
//  failure: the walk goes on without.
ItemUseEnd renew_repel(Logger& logger, ItemUseActions& actions, std::string_view item);

//  The console's bag, opened from `menu`'s BAG.
class ConsoleItemUseActions : public ItemUseActions{
public:
    ConsoleItemUseActions(ConsoleHandle& console, ProControllerContext& context, Language language, StartMenu menu);

    virtual bool open_pocket(BagPocket pocket) override;
    virtual std::vector<BagRow> read_pocket() override;
    virtual void use_row(size_t row) override;
    virtual bool use_row_on_lead(size_t row) override;
    virtual void register_row(size_t row) override;
    virtual void close_bag() override;

private:
    //  The two halves of use_row_on_lead().
    bool wait_for_level_up_stats();
    bool back_to_bag_declining_moves();

    ConsoleHandle& m_console;
    ProControllerContext& m_context;
    Language m_language;
    StartMenu m_menu;
};


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
