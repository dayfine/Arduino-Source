/*  PokemonLZA AutoStory - Action names
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Common/Cpp/EnumStringMap.h"
#include "PokemonLZA_AutoStoryActionNames.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


std::string_view action_name(PlzaAction action){
    static const EnumStringMap<PlzaAction> names{
        {PlzaAction::WAIT_FOR_PLATFORM, "wait_for_platform"},
        {PlzaAction::OPENING_TO_PLATFORM, "opening_to_platform"},
        {PlzaAction::WALK, "walk"},
        {PlzaAction::TALK, "talk"},
        {PlzaAction::TALK_THROUGH_MAP_SCREEN, "talk_through_map_screen"},
        {PlzaAction::TALK_TO_POKEDEX_SCREEN, "talk_to_pokedex_screen"},
        {PlzaAction::CLIMB_LADDER, "climb_ladder"},
        {PlzaAction::CHOOSE_0, "choose_0"},
        {PlzaAction::CHOOSE_1, "choose_1"},
        {PlzaAction::SCENE, "a"},
        {PlzaAction::MASH, "mash"},
        {PlzaAction::MASH_TO_MOVE_HINTS, "mash_to_move_hints"},
        {PlzaAction::PLUS_SKIP, "plus_skip"},
        {PlzaAction::TEXT_TO_CHOICE, "text_to_choice"},
        {PlzaAction::STORY_BATTLE_START, "story_battle_start"},
        {PlzaAction::STORY_BATTLE, "story_battle"},
        {PlzaAction::SAVE_FROM_MENU, "save_from_menu"},
        {PlzaAction::SET_TEXT_SPEED_FAST, "set_text_speed_fast"},
        {PlzaAction::NEW_GAME_INTRO, "new_game_intro"},
        {PlzaAction::CONFIRM_NEW_GAME, "confirm_new_game"},
        {PlzaAction::RUN_TO_GATE, "run_to_gate"},
        {PlzaAction::RUN_OUT_OF_ZONE_1, "run_out_of_zone_1"},
        {PlzaAction::CATCH_TUTORIAL, "catch_tutorial"},
        {PlzaAction::BATTLE_THEN_CATCH, "battle_then_catch"},
        {PlzaAction::CATCH_IN_ZONE, "catch_in_zone"},
        {PlzaAction::CHECK_OUT_OF_ZONE_1, "check_out_of_zone_1"},
        {PlzaAction::WALK_TO_UNREAD_MARKER, "walk_to_unread_marker"},
        {PlzaAction::CLAIM_MABLE_RESEARCH_REWARD, "claim_mable_research_reward"},
    };
    return names.get_string(action);
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
