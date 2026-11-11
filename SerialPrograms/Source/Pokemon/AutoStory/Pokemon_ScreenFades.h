/*  Screen Fades
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Split out of Pokemon_ScreenEvents.h, unchanged; it includes this header.
 *
 */

#ifndef PokemonAutomation_Pokemon_ScreenFades_H
#define PokemonAutomation_Pokemon_ScreenFades_H

#include <atomic>
#include <cstdint>
#include <deque>
#include <initializer_list>
#include <optional>
#include <string>
#include "Common/Cpp/Concurrency/Mutex.h"
#include "Common/Cpp/Time.h"
#include "CommonFramework/ImageTools/ImageStats.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/InferenceCallbacks/VisualInferenceCallback.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "NintendoSwitch/Inference/NintendoSwitch_HomeMenuDetector.h"
#include "Pokemon_ScreenEventTypes.h"

namespace PokemonAutomation{
namespace Pokemon{


//  The event of a fade: FADE_BLACK, FADE_WHITE, or none for WarpFade::NONE.
ScreenEvents fade_events(WarpFade fade);
//  The fade `events` show: BLACK before WHITE if they have both.
WarpFade fade_in(const ScreenEvents& events);
//  "dialog box, player can move"; "nothing" when empty.
std::string describe(const ScreenEvents& events);


//  A fade is uniform over the middle 80% of the screen, the shared
//  Black/WhiteScreenWatcher's stddev test (upstream #1717): a dim cave floor
//  is textured, so it is no fade (BDSP's Oreburgh Gate, mean 46, lab-B
//  2026-11-08). Check against the Gate's frames on the console before
//  tightening.
constexpr double FADE_MAX_STDDEV_SUM = 10;
//  Dark goes to an rgb sum of 225, as a cave mouth's fade is dim, 45 to 64
//  a channel, not black.
constexpr double FADE_MAX_DARK_SUM = 225;
//  White from an rgb sum of 500, as the shared WhiteScreenWatcher's.
constexpr double FADE_MIN_WHITE_SUM = 500;
//  The fade `middle` (the stats of the screen's middle 80%) shows, if any.
WarpFade fade_of(const ImageStats& middle);
//  fade_of() for a screen.
WarpFade fade_of_screen(const ImageViewRGB32& screen);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
