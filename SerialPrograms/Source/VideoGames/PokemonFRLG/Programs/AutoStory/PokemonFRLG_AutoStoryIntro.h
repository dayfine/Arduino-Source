/*  AutoStory Intro
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The new game's intro: what it asks about the names, and the intro
 *  played from the title screen to the bedroom.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryIntro_H
#define PokemonAutomation_PokemonFRLG_AutoStoryIntro_H

#include <cstddef>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/Time.h"
#include "CommonFramework/Language.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepExecutor.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckLevel.h"
#include "Pokemon/Navigation/Pokemon_Grid.h"
#include "PokemonFRLG_AutoStoryPresses.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The arrow's row on Oak's list of rival names (NEW NAME, then four preset
//  names), if the list shows. In pokefirered's oak_speech.c the list is a
//  12x10-tile window at tile (2, 2), rows 16 px apart, the cursor starting
//  on NEW NAME. The intro's YES/NO boxes open in the same corner but are
//  6x4 tiles: the list is told from them by its white past their frame.
std::optional<int> rival_name_list_cursor(const ImageViewRGB32& screen);

//  What shows in the new game's intro, as far as the names go.
enum class IntroNameScreen{
    OTHER,
    KEYBOARD,
    RIVAL_LIST,
};
//  What the new game's intro does about the names. The player's name is
//  asked on a keyboard, always first; the rival's on the name list, where
//  the first preset name is picked, as SID Helper does. A keyboard after
//  the player's is the rival's, opened because the list was missed.
enum class IntroNameStep{
    NONE,
    PLAYER_KEYBOARD,
    RIVAL_PRESET,
    RIVAL_KEYBOARD,
};
//  A preset picked returns to the list only after NO on "Er, was it ...?",
//  so after this many picks A opens the keyboard instead.
constexpr size_t RIVAL_PRESET_TRIES = 3;
//  Determines the next input step in character/rival naming sequence.
IntroNameStep intro_name_step(IntroNameScreen screen, size_t keyboards_seen, size_t presets_picked);
//  From the title screen of a game with no save, to the bedroom: the
//  controls pages, Oak's speech, BOY, `player_name` typed on the player's
//  keyboard, and the rival's first preset name. An empty `player_name`
//  keeps the player's default name (an empty name confirmed). Returns on
//  the overworld. Throws UserSetupError if the game can't take the name
//  (naming_text_error()).
void play_new_game_intro(ConsoleHandle& console, ProControllerContext& context, const std::string& player_name);

//  play_new_game_intro() up to the player's keyboard: `player_name` typed,
//  OK not pressed, which is where SID Helper starts. Empty types nothing.
void play_new_game_to_player_name(ConsoleHandle& console, ProControllerContext& context, const std::string& player_name);

//  From the Trainer Card SID Helper leaves open, or from HOME (its "go
//  home when done"): back to the game, the card and the START menu closed.
//  Returns on the overworld.
void close_trainer_card(ConsoleHandle& console, ProControllerContext& context);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
