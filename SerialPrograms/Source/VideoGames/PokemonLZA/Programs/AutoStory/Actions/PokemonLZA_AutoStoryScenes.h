/*  PokemonLZA AutoStory - Scenes
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The story actions that press through scenes rather than walk: the scene
 *  mash over PlzaScreenObserver's events, and the new game's opening,
 *  which is timed presses as recorded on console B (2026-11-08). None has run on a console as a story step yet.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryScenes_H
#define PokemonAutomation_PokemonLZA_AutoStoryScenes_H

#include <optional>
#include <string>
#include "Common/Cpp/Time.h"
#include "CommonFramework/Language.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "NintendoSwitch/Controllers/NintendoSwitch_ControllerButtons.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryScreenEvents.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  From a scene, presses through it as scene_mash_action() says, reading
//  PlzaScreenObserver's events, until `goal` shows (OVERWORLD: the party
//  bar held a second). `text_button` advances the text (A or B). With a
//  `quiet` time, an OVERWORLD mash also ends once no box, choice, battle or
//  fade has shown for that long: the new game's tutorial shows no party bar
//  (only the Move / Control Camera hints, which have no detector yet).
//  A cutscene can hold a still frame longer than `quiet`, so the quiet end
//  waits for `min_length`, the scene's rough recorded length, to pass.
//  A tutorial card gets A whatever `text_button` is: only A turns its pages
//  (tutorial_card_shows()).
//  Throws OperationFailedException if the goal does not show within `timeout`.
void mash_scene(
    ConsoleHandle& console, ProControllerContext& context,
    SceneMashGoal goal, Button text_button, Milliseconds timeout,
    std::optional<Milliseconds> quiet = std::nullopt,
    Milliseconds min_length = Milliseconds(0)
);


//  A tutorial card on screen (Map & Missions, Main Missions, Pokedex, ...):
//  a bright panel in the middle over dark, even edges.
bool tutorial_card_shows(const ImageViewRGB32& frame);


//  The new game's opening, as recorded 2026-11-08 on console B, with the
//  player's choices (PokemonLZA_AutoStoryIntro.h has each one's presses).
//  Each logs every press and saves its screen's frame (DebugDumps) for the
//  detectors to be built from. Only the language list's is built: the look
//  picker and the keyboard wait a fixed time for their screens.

//  01.00's first step. It starts on the Switch HOME screen with the game's
//  tile highlighted and no save, the game open or closed: closes the game
//  so the launch is fresh, starts it (start_game_from_home(): the user
//  select and the online license check), waits 5 s, then up to 90 s for
//  the language list (a new game has no title screen). Started on the
//  language list itself, it skips the launch. Then moves the list's
//  highlight to `language`'s entry, reading it after each press, and
//  presses A. Throws UserSetupError for a language the list lacks or any
//  other starting screen, OperationFailedException if the list does not
//  show or the highlight does not get there.
void new_game_language(ConsoleHandle& console, ProControllerContext& context, Language language);

//  On the look picker, whose highlight starts at random: UP and LEFT x3 to
//  the top-left look (the edges clamp), RIGHT and DOWN to look `look` (1 to 8,
//  a 2 x 4 grid, to be confirmed), A.
void pick_player_look(ConsoleHandle& console, ProControllerContext& context, size_t look);

//  On the name screen: types `name` on the Switch keyboard (letters and
//  digits, a capital only as the first letter; name_keys()), + to finish,
//  then DOWN x2 and A on "Ready!". `first_letter_auto_caps`: the keyboard
//  starts with a capital by itself, so Shift is for a lower case first
//  letter instead (to be confirmed).
void type_player_name(
    ConsoleHandle& console, ProControllerContext& context,
    const std::string& name, bool first_letter_auto_caps
);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
