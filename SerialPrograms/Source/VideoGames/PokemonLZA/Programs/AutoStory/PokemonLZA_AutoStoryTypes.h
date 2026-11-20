/*  PokemonLZA AutoStory - Types
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Shared pure types, action names, map coordinates, and backend reading keys
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

//  1: a main mission's banner ("MAIN MISSION 01" over a yellow bar).
constexpr std::string_view MISSION_BANNER_READING = "scene.mission_banner";

//  1: the "COMPLETE" card a main mission ends with; excludes MISSION_BANNER_READING.
constexpr std::string_view MISSION_COMPLETE_READING = "scene.mission_complete";

//  A choice box's row count; unset when the rows do not read.
constexpr std::string_view CHOICE_ROWS_READING = "choice.rows";

//  With CHOICE_ROWS_READING, the highlighted row, 0 the top.
constexpr std::string_view CHOICE_SELECTED_READING = "choice.selected";

//  1 while a tutorial card shows; only A turns its pages.
constexpr std::string_view TUTORIAL_CARD_READING = "scene.tutorial_card";

//  1 while the main menu (X) shows.
constexpr std::string_view MAIN_MENU_READING = "menu.main";

//  1 while the arrow is on Save, after R on the main menu.
constexpr std::string_view MENU_SAVE_BUTTON_READING = "menu.save_button";

//  1: the full map, which Plus opens on free control.
constexpr std::string_view MAP_READING = "menu.map";

//  Full-map coordinates scaled to 800 x 450.
struct MapPoint{
    double x = 0;
    double y = 0;
};

enum class PlzaAction{
    WAIT_FOR_PLATFORM,
    OPENING_TO_PLATFORM,
    WALK,
    TALK_THROUGH_MAP_SCREEN,
    TALK_TO_POKEDEX_SCREEN,
    CLIMB_LADDER,
    CHOOSE_0,
    CHOOSE_1,
    SCENE,
    PLUS_SKIP,
    STORY_BATTLE,
    SAVE_FROM_MENU,
    SET_TEXT_SPEED_FAST,
    NEW_GAME_INTRO,
    CONFIRM_NEW_GAME,
    RUN_TO_GATE,
    RUN_OUT_OF_ZONE_1,
    CATCH_TUTORIAL,
    BATTLE_THEN_CATCH,
    CATCH_IN_ZONE,
    CHECK_OUT_OF_ZONE_1,
    WALK_TO_UNREAD_MARKER,
    CLAIM_MABLE_RESEARCH_REWARD,
};

//  The action's name in the plan files ("walk").
std::string_view action_name(PlzaAction action);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
