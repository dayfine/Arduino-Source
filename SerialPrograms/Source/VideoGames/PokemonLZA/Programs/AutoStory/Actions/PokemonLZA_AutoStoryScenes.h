/*  PokemonLZA AutoStory - Scenes
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The story actions that press through scenes rather than walk: the scene
 *  mash over PlzaScreenObserver's events, and the new game's opening and
 *  the ladder, which are timed presses as recorded on console B
 *  (2026-11-08). None has run on a console as a story step yet.
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
//  None of it reads the screen yet: each waits a fixed time for its screen,
//  logs every press, and saves the screen's frame (DebugDumps) for the
//  detectors to be built from.

//  At the title screen with no save: A into the language list, DOWN to
//  `language`'s entry (the list's order is Scarlet/Violet's: to be
//  confirmed), A. Throws UserSetupError for a language the list lacks.
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

//  Taunie's talk after her battle (03.02): A through it and the "Map &
//  Missions" card (A turns its pages, B turns back) until the map shows,
//  B to close the map (A would place a pin), then B through the rest of the
//  talk to the overworld (console B, 2026-11-10). Throws
//  OperationFailedException if the map does not show within 60 s.
void talk_through_map_screen(ConsoleHandle& console, ProControllerContext& context);

//  Facing the side street's wall (03.03): sidestep right to the ladder
//  (0.6 s), face it (150 ms), A at "A Climb Up", hold up 5 s to the top.
void climb_ladder(ConsoleHandle& console, ProControllerContext& context);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
