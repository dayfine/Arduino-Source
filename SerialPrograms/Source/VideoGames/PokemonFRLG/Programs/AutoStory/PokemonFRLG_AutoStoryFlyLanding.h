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
 *  The fly map's own close is such a fade, and the departure town after it
 *  is quiet with the player on screen for 2.6 s before the flight (console
 *  B, 2026-11-13 02:42 and 02:46, 12.03: "landed in PalletTown" on Fuchsia
 *  both times; the first leg was then held during Pallet's fade-in, walked
 *  nothing, and every later tile was 2 rows off). So the screen must also
 *  have left the departure town: it differs from the overworld shot before
 *  the Fly menu opened.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryFlyLanding_H
#define PokemonAutomation_PokemonFRLG_AutoStoryFlyLanding_H

#include <functional>
#include <optional>
#include <string>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Common/Cpp/Time.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "Pokemon/AutoStory/Pokemon_InterruptHandlers.h"
#include "Pokemon/AutoStory/Pokemon_StoryInterrupts.h"
#include "Pokemon/Navigation/Pokemon_Direction.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Whether `look` saw a full-screen fade, black or white, begin.
bool fly_faded(const Pokemon::ScreenLook& look);

//  How far the game screen (`game`) differs, as an RMSD, between the
//  overworld before the Fly (`departure`) and `now`.
double departure_change(const ImageViewRGB32& departure, const ImageViewRGB32& now, const ImageFloatBox& game);

//  Whether `now` shows another town than `departure`: more change than the
//  departure town with its Fly animation over it.
bool left_departure(const ImageViewRGB32& departure, const ImageViewRGB32& now, const ImageFloatBox& game);

//  What a look found of the landing.
struct FlyLanding{
    bool faded = false;
    Pokemon::ScreenSettle settle = {};
    //  Whether the screen has left the departure town, once a fade has
    //  passed and the overworld is quiet.
    bool left = false;
    //  The facing read, once a fade has passed and the overworld is quiet.
    std::optional<Pokemon::Direction> facing = std::nullopt;

    bool landed() const;
};

//  Judges `look` (which goes on from the Fly's A): landed once a fade has
//  begun, the overworld has been quiet for `quiet`, `has_left` says the
//  screen left the departure town, and `read_facing` returns a direction.
//  `has_left` is only called when the first two hold, `read_facing` when
//  the first three do.
FlyLanding judge_fly_landing(
    const Pokemon::ScreenLook& look, Milliseconds quiet,
    const std::function<bool()>& has_left,
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
