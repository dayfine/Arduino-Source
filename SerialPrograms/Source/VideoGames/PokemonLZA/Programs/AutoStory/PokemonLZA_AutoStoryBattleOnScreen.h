/*  PokemonLZA AutoStory - Battle on screen
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A trainer's battle can start before the plan's walk and talk to them
 *  (03.02b: Taunie's, in the plaza scene). A scene or walk step with a story
 *  battle later in its checkpoint then ends, and the run goes on at that
 *  battle step. The battle HUD flickers, so it counts by FlickerFilter.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryBattleOnScreen_H
#define PokemonAutomation_PokemonLZA_AutoStoryBattleOnScreen_H

#include "Pokemon/AutoStory/Engine/Pokemon_ScreenTimers.h"
#include "Pokemon/AutoStory/Pokemon_StoryFailure.h"
#include "PokemonLZA_AutoStoryBackend.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


using Pokemon::FlickerFilter;

//  The battle HUD: on 2 looks within 2 s, off after 3 s unseen.
FlickerFilter battle_filter();

//  "battle_on_screen": the run goes on at the checkpoint's next story battle.
Pokemon::StoryFailure battle_on_screen_failure();

//  The battle HUD by battle_filter(), looking on only while the first look
//  shows it, at most 8 looks (2 s).
bool battle_showing(PlzaBackend& backend);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
