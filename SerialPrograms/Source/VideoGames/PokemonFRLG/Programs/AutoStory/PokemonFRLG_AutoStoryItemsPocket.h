/*  AutoStory Items Pocket
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The Items pocket as the story counts it: rows and counts, read from
 *  the bag, and the items used after a battle.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryItemsPocket_H
#define PokemonAutomation_PokemonFRLG_AutoStoryItemsPocket_H

#include <cstddef>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_ItemListReader.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldWalk.h"
#include "PokemonFRLG_AutoStoryActions.h"
#include "PokemonFRLG_AutoStoryBattleData.h"
#include "PokemonFRLG_AutoStoryParty.h"
#include "PokemonFRLG_AutoStorySupplies.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  How many of `item` (a slug) the bag holds, as far as known.
int item_count(const Supplies& supplies, const std::string& item);

//  The row (from 0) of `item` in the Items pocket, or -1 if none is left.
//  Items the story doesn't count, such as a MOON STONE, take rows too.
int item_row(const Supplies& supplies, const std::string& item);

//  Sets the counts and the pocket's order from the Items pocket as read.
//  A row whose count couldn't be read still holds at least one: it keeps
//  the count known before (the order bought), or 1. Returns those rows'
//  items, for the log.
std::vector<std::string> set_from_bag(Supplies& supplies, const std::vector<BagRow>& rows);

//  After a save is loaded: the Items pocket as read, taken into `supplies`
//  and `saved` (what a reload restores) in place of the row's counts, as
//  set_from_bag() does. With nothing read both keep the row's. Returns the
//  items whose count couldn't be read.
std::vector<std::string> set_from_loaded_bag(Supplies& supplies, Supplies& saved, const std::vector<BagRow>& rows);

//  From the overworld: opens the Items pocket, reads it, and closes the
//  bag again. Empty if nothing could be read.
std::vector<BagRow> read_items_pocket(ConsoleHandle& console, ProControllerContext& context, Language language);
//  From the overworld, after a battle or at a checkpoint's start: the items
//  recovery_items() lists, each if one is left.
//  The bag's inputs are not checked on the console yet.
void recover_after_battle(
    ConsoleHandle& console, ProControllerContext& context,
    Supplies& supplies, Pokemon::StoryCheckLevel checks, Language language
);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
