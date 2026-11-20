/*  PokemonLZA AutoStory - Action names
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The actions PLZA's plan files (in StoryData) give their steps.
 *  Actions/PokemonLZA_AutoStoryActionTable.cpp binds each to its code; the
 *  plan checks (PokemonLZA_AutoStoryTable.h) and the autosave policy read
 *  them too.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryActionNames_H
#define PokemonAutomation_PokemonLZA_AutoStoryActionNames_H

#include <string_view>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


enum class PlzaAction{
    //  Waits.
    WAIT_FOR_PLATFORM,
    OPENING_TO_PLATFORM,
    //  Walks and talks.
    WALK,
    TALK,
    TALK_THROUGH_MAP_SCREEN,
    TALK_TO_POKEDEX_SCREEN,
    CLIMB_LADDER,
    CHOOSE_0,
    CHOOSE_1,
    //  Scenes, played by the scene machine.
    SCENE,
    MASH,
    MASH_TO_MOVE_HINTS,
    //  Plus on a cutscene once its opening black fade ends.
    PLUS_SKIP,
    TEXT_TO_CHOICE,
    STORY_BATTLE_START,
    STORY_BATTLE,
    //  Saves from the menu where a checkpoint would otherwise end on an
    //  autosave the plan does not place (a catch's).
    SAVE_FROM_MENU,
    //  Text speed to fast through the options, on 01.01's platform.
    SET_TEXT_SPEED_FAST,
    //  Console only: off a console they fail as "console_only".
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
