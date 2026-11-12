/*  AutoStory Presses
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The story's basic presses: text advanced to its end, a YES/NO read
 *  and answered, a step onto a trigger and a turn to a wall.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryPresses_H
#define PokemonAutomation_PokemonFRLG_AutoStoryPresses_H

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
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_PlayerFacingReader.h"
#include "PokemonFRLG_AutoStoryStartMenu.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Which row of the standard YES/NO box the arrow is on (true: YES), if
//  one shows.
std::optional<bool> yes_no_cursor(const ImageViewRGB32& screen);
//  Expected screen state when dialogue text finishes.
enum class TextEnd{
    OVERWORLD,
    BATTLE,
};

//  Presses B until the overworld is back or, if `stop_at_battle`, a battle
//  shows. Throws OperationFailedException after `timeout`.
TextEnd advance_text(ConsoleHandle& console, ProControllerContext& context, Milliseconds timeout, bool stop_at_battle);

//  advance_text() to the overworld.
void mash_until_overworld(ConsoleHandle& console, ProControllerContext& context, Milliseconds timeout);

//  From the overworld, facing something that answers A with a message (a
//  sign, Vermilion Gym's trash cans): A, then how many pages the message
//  has, one or two, then on to the overworld. Language-free: one A after
//  the first page has printed either closes the message or shows its
//  second page. Nothing if no message showed.
std::optional<size_t> talk_and_count_pages(ConsoleHandle& console, ProControllerContext& context);

//  Advances text with `advance` until a YES/NO shows, then answers it. B
//  answers NO. Throws OperationFailedException after `timeout`.
void answer_yes_no(ConsoleHandle& console, ProControllerContext& context, bool yes, Button advance, Milliseconds timeout);

//  From the overworld: one step `direction`, onto a trigger or into a door.
//  `facing` is the way the player faces, if known; a player facing another
//  way is turned first (OverworldWalkSteps::turn_to()).
void step_once(
    ConsoleHandle& console, ProControllerContext& context,
    Pokemon::Direction direction, std::optional<Pokemon::Direction> facing, PlayerGender player
);

//  From the overworld, turns to face `direction` without stepping: the
//  player must have a wall that way (OverworldWalkSteps::turn_to()).
void face_wall(ConsoleHandle& console, ProControllerContext& context, Pokemon::Direction direction, PlayerGender player);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
