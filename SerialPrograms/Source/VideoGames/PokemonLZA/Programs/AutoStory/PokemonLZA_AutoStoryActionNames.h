/*  PokemonLZA AutoStory - Action names
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The "action" names PLZA's plan files (in StoryData) give their
 *  steps. Actions/PokemonLZA_AutoStoryActionTable.cpp binds each to its
 *  code; the plan checks (PokemonLZA_AutoStoryTable.h) and the autosave
 *  policy read them too.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryActionNames_H
#define PokemonAutomation_PokemonLZA_AutoStoryActionNames_H

#include <string_view>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{
namespace ActionName{


//  Waits.
inline constexpr std::string_view WAIT_FOR_PLATFORM = "wait_for_platform";
inline constexpr std::string_view OPENING_TO_PLATFORM = "opening_to_platform";

//  Walks and talks.
inline constexpr std::string_view WALK = "walk";
inline constexpr std::string_view TALK = "talk";
inline constexpr std::string_view TALK_THROUGH_MAP_SCREEN = "talk_through_map_screen";
inline constexpr std::string_view TALK_TO_POKEDEX_SCREEN = "talk_to_pokedex_screen";
inline constexpr std::string_view CLIMB_LADDER = "climb_ladder";
inline constexpr std::string_view CHOOSE_0 = "choose_0";
inline constexpr std::string_view CHOOSE_1 = "choose_1";

//  Scenes, played by the scene machine.
inline constexpr std::string_view SCENE = "a";
inline constexpr std::string_view MASH = "mash";
inline constexpr std::string_view MASH_TO_MOVE_HINTS = "mash_to_move_hints";
inline constexpr std::string_view TEXT_TO_CHOICE = "text_to_choice";
inline constexpr std::string_view STORY_BATTLE_START = "story_battle_start";
inline constexpr std::string_view STORY_BATTLE = "story_battle";

//  Saves from the menu where a checkpoint would otherwise end on an
//  autosave the plan does not place (a catch's).
inline constexpr std::string_view SAVE_FROM_MENU = "save_from_menu";

//  Console only: off a console they fail as "console_only".
inline constexpr std::string_view NEW_GAME_INTRO = "new_game_intro";
inline constexpr std::string_view CONFIRM_NEW_GAME = "confirm_new_game";
inline constexpr std::string_view RUN_TO_GATE = "run_to_gate";
inline constexpr std::string_view AIMED_RUN_TO_GATE = "aimed_run_to_gate";
inline constexpr std::string_view CATCH_TUTORIAL = "catch_tutorial";
inline constexpr std::string_view BATTLE_THEN_CATCH = "battle_then_catch";
inline constexpr std::string_view CATCH_IN_ZONE = "catch_in_zone";
inline constexpr std::string_view WALK_BACK_TO_ZONE_ROAD = "walk_back_to_zone_road";
inline constexpr std::string_view CHECK_OUT_OF_ZONE_1 = "check_out_of_zone_1";
inline constexpr std::string_view WALK_TO_UNREAD_MARKER = "walk_to_unread_marker";
inline constexpr std::string_view CLAIM_MABLE_RESEARCH_REWARD = "claim_mable_research_reward";



} // namespace ActionName
} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
