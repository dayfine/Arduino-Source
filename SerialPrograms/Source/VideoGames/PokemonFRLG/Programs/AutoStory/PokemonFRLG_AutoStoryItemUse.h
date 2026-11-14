/*  AutoStory Item Use
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Items the story uses from the bag on the overworld, as PokeGuy's run
 *  does: a Repel or Super Repel before long walks through grass and caves,
 *  renewed when it wears off; an Escape Rope out of the Rocket Hideout;
 *  and the Bicycle. The bag is read with read_bag_pocket(), so the step
 *  finds its item wherever the pocket keeps it.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryItemUse_H
#define PokemonAutomation_PokemonFRLG_AutoStoryItemUse_H

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "CommonFramework/Language.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryActionArg.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryStep.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_ItemListReader.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The GAME step that uses an item from the bag on the overworld, its
//  args the item's slug as read_item_list_name() names it and, for an item
//  used up more than once (the Rare Candy), its count.
inline const std::string USE_ITEM_VARIANT = "use_item";
//  The GAME step that gets on the registered Bicycle with SELECT
//  (OverworldWalkSteps::get_on_bike()). The ROUTE steps after it ride
//  (BIKE_VARIANT).
inline const std::string RIDE_BIKE_VARIANT = "ride_bike";
//  The variant of a ROUTE or LEGS step ridden on the Bicycle, mounted by a
//  RIDE_BIKE_VARIANT step before it.
//  Indoors, where the game doesn't let the player ride, it walks.
inline const std::string BIKE_VARIANT = "bike";
//  The GAME step that registers the Bicycle for SELECT: the Key Items
//  pocket's BICYCLE, REGISTER, its menu's second option. Once a save:
//  the registration is saved with the game, and a save made before it
//  doesn't have it.
inline const std::string REGISTER_BIKE_VARIANT = "register_bike";


//  The item a use_item step uses (with "safari", from the Safari Zone's
//  START menu, step_start_menu()), or nothing if it isn't one, the item
//  isn't one the story uses from the bag, or it names a slot or a count
//  that is out of range. An item for a Pokemon (is_party_item()) goes on
//  the lead.
std::optional<std::string> parse_use_item_step(const Pokemon::StoryStep& step);
//  How often the step uses its item ("count", 1 to 99; 1 if left out).
size_t use_item_count(const Pokemon::StoryStep& step);
//  Whether `step` is one of this file's steps.
bool is_item_use_step(const Pokemon::StoryStep& step);


//  Registers the item use tests.
void add_tests_AutoStoryItemUse(UnitTestDatabase& database);


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
