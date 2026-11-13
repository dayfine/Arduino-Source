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
//  detectors to be built from. The language list and the name keyboard (the
//  Switch system keyboard) are detected; the look picker is not checked and
//  waits a fixed time.

//  01.00's first step. It starts on the Switch HOME menu with PLZA's tile
//  selected and no save: launches that tile without moving the cursor,
//  closing the game first if it is running, through the user select and
//  the license check to the language list (launch_selected_game_from_home(),
//  up to 90 s; a new game has no title screen). Throws UserSetupError on
//  any other starting screen, OperationFailedException if the list does not
//  show (the selected game may not be PLZA, or it has a save).
void launch_game_from_home(ConsoleHandle& console, ProControllerContext& context);

//  On the language list: moves its highlight to `language`'s entry,
//  reading it after each press, and presses A. Throws UserSetupError for a
//  language the list lacks, OperationFailedException if the list is not on
//  screen or the highlight does not get there.
void new_game_language(ConsoleHandle& console, ProControllerContext& context, Language language);

//  On the look picker, whose highlight starts at random: UP and LEFT x3 to
//  the top-left look (the edges clamp), RIGHT and DOWN to look `look` (1 to 8,
//  a 2 x 4 grid), A. The screen is not checked (is_look_picker() is a TODO
//  stub): the presses are blind.
void pick_player_look(ConsoleHandle& console, ProControllerContext& context, size_t look);

//  On the name screen, the Switch system keyboard: waits up to 20 s for it,
//  deletes any text left in its field, types `name` reading the cursor
//  before each key (type_on_system_keyboard(); letters and digits, a
//  capital only as the first letter; name_keys()), PLUS to finish and waits
//  for the keyboard to close, then on "Confirm Choices" DOWN x2 and A on
//  "Ready!". `first_letter_auto_caps`: the keyboard starts with a capital
//  by itself, so Shift is for a lower case first letter instead (to be
//  confirmed). Throws OperationFailedException if the keyboard does not
//  show or close.
void type_player_name(
    ConsoleHandle& console, ProControllerContext& context,
    const std::string& name, bool first_letter_auto_caps
);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
