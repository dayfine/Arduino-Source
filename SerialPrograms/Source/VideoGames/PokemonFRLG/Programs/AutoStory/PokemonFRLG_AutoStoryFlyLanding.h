/*  AutoStory Fly Landing
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  When Fly has arrived. The flight's black band across the screen and the
 *  departure town's overworld before the fade are both quiet, with nothing
 *  showing: a quiet overworld alone passed for the landing (console B,
 *  2026-11-12 23:59, 12.03: "landed in PalletTown" 3 s before the Pallet
 *  banner, with the player still in Fuchsia). So the landing is a fade
 *  (the warp's black or white) that rose after the A on the fly map, then
 *  a quiet overworld, with the player on screen: their facing can be read.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryFlyLanding_H
#define PokemonAutomation_PokemonFRLG_AutoStoryFlyLanding_H

#include <functional>
#include <optional>
#include <string>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Common/Cpp/Time.h"
#include "Pokemon/AutoStory/Pokemon_InterruptHandlers.h"
#include "Pokemon/AutoStory/Pokemon_StoryInterrupts.h"
#include "Pokemon/AutoStory/Types/Pokemon_Direction.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Whether `look` saw a full-screen fade, black or white, begin.
bool fly_faded(const Pokemon::ScreenLook& look);

//  What a look found of the landing.
struct FlyLanding{
    bool faded = false;
    Pokemon::ScreenSettle settle = {};
    //  The facing read, once a fade has passed and the overworld is quiet.
    std::optional<Pokemon::Direction> facing = std::nullopt;

    bool landed() const;
};

//  Judges `look` (which goes on from the Fly's A): landed once a fade has
//  begun, the overworld has been quiet for `quiet`, and `read_facing`
//  returns a direction. `read_facing` is only called when the first two
//  hold.
FlyLanding judge_fly_landing(
    const Pokemon::ScreenLook& look, Milliseconds quiet,
    const std::function<std::optional<Pokemon::Direction>()>& read_facing
);

//  "faded, settled overworld (quiet 1.1 s), the player faces south"; "no fade yet, ...".
std::string describe(const FlyLanding& landing);


//  Registers the landing judge's tests.
void add_tests_AutoStoryFlyLanding(UnitTestDatabase& database);


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
