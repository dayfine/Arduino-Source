/*  AutoStory Slope Run
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  On the console: the Cycling Road's slope (PokemonFRLG_AutoStorySlope.h),
 *  and the Items pocket's counts that confirm the items found on it.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStorySlopeRun_H
#define PokemonAutomation_PokemonFRLG_AutoStorySlopeRun_H

#include <map>
#include <optional>
#include <string>
#include "CommonFramework/Language.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "Pokemon/AutoStory/Pokemon_StoryEvents.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryActionSpec.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryStep.h"
#include "PokemonFRLG_AutoStorySupplies.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The GAME step that reads the Items pocket's counts of `items` (bag
//  slugs). Without `found` it keeps them; with it, each must be one more
//  than kept: the item was found since.
inline const std::string COUNT_ITEMS_VARIANT = "count_items";
//  COUNT_ITEMS_VARIANT and its args.
Pokemon::StoryActionSpec count_items_action_spec();

//  What a slope or count step needs from the story.
struct SlopeEnv{
    ConsoleHandle& console;
    ProControllerContext& context;
    Supplies& supplies;
    Pokemon::StoryCheckLevel checks;
    Language language;
    //  The counts COUNT_ITEMS_VARIANT kept.
    std::map<std::string, int>& counted_items;
};

//  On the console, a SLOPE_VARIANT or COUNT_ITEMS_VARIANT step; nothing for
//  another step. A Biker who catches the player on the slope is fought;
//  the step then fails, as where the battle left the player is unknown.
std::optional<Pokemon::Outcome> run_slope_step(SlopeEnv& env, const Pokemon::StoryStep& step);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
