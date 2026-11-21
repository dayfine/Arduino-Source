/*  PokemonLZA AutoStory - Types
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Shared pure types, action names, and backend reading keys
 *  for PLZA AutoStory.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryTypes_H
#define PokemonAutomation_PokemonLZA_AutoStoryTypes_H

#include <string_view>
#include "Pokemon/AutoStory/Engine/Pokemon_StoryBackend.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


using PlzaBackend = Pokemon::StoryBackend;

//  1: the autosave badge ("Saving...", top right).
constexpr std::string_view AUTOSAVE_READING = "scene.autosave";

//  1: the tutorial's controls hint ("(L) Move"): the player has control.
constexpr std::string_view CONTROLS_HINT_READING = "scene.controls_hint";

//  1: the advance chevron, at a dialogue box or boxless subtitles.
constexpr std::string_view ADVANCE_ARROW_READING = "scene.advance_arrow";

//  A choice box's row count; unset when the rows do not read.
constexpr std::string_view CHOICE_ROWS_READING = "choice.rows";

//  With CHOICE_ROWS_READING, the highlighted row, 0 the top.
constexpr std::string_view CHOICE_SELECTED_READING = "choice.selected";

//  1 while a tutorial card shows; only A turns its pages.
constexpr std::string_view TUTORIAL_CARD_READING = "scene.tutorial_card";

enum class PlzaAction{
    WAIT_FOR_PLATFORM,
    WALK,
    CHOOSE_1,
    SCENE,
    PLUS_SKIP,
    SET_TEXT_SPEED_FAST,
};

//  The action's name in the plan files ("walk").
std::string_view action_name(PlzaAction action);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
