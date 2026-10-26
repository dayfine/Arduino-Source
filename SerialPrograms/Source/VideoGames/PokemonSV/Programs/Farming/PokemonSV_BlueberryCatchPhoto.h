/*  Blueberry Quests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonSV_BlueberryCatchPhoto_H
#define PokemonAutomation_PokemonSV_BlueberryCatchPhoto_H

#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_Settings.h"
#include "VideoGames/PokemonSV/Programs/Farming/PokemonSV_BlueberryQuests.h"
#include "VideoGames/PokemonSV/Options/PokemonSV_BBQOption.h"

using namespace std;

namespace PokemonAutomation{
    struct ProgramInfo;
namespace NintendoSwitch{
namespace PokemonSV{

//Navigate to a photo target
CameraAngle quest_photo_navi(
    const ProgramInfo& info,
    ConsoleHandle& console, ProControllerContext& context,
    const BBQOption& BBQ_OPTIONS,
    BBQuests current_quest
);

//Take picture of a pokemon/location
void quest_photo(
    const ProgramInfo& info,
    ConsoleHandle& console, ProControllerContext& context,
    const BBQOption& BBQ_OPTIONS,
    BBQuests current_quest
);


//Navigate to a catch target
void quest_catch_navi(
    const ProgramInfo& info,
    ConsoleHandle& console, ProControllerContext& context,
    const BBQOption& BBQ_OPTIONS,
    BBQuests current_quest
);

//Select and throw ball
void quest_catch_throw_ball(
    const ProgramInfo& info,
    ConsoleHandle& console, ProControllerContext& context,
    Language language,
    const std::string& selected_ball
);

//Handle catching the target
void quest_catch_handle_battle(
    const ProgramInfo& info,
    ConsoleHandle& console, ProControllerContext& context,
    const BBQOption& BBQ_OPTIONS,
    BBQuests current_quest
);

//Whether quest_catch() catches a wild battle that starts on the way back to Central Plaza
//instead of fleeing it. Any wild pokemon completes catch_any, so until this attempt has
//battled, the battle is a catch. Typed catch quests flee: the type is not read in battle.
bool follow_tera_glow(ConsoleHandle& console, ProControllerContext& context);

bool catch_quest_catches_on_return(BBQuests current_quest, bool battled_this_attempt);

//How many looks quest_catch() takes. The Coastal hill can look again from the closest fly
//point (Flying with the Coastal option, Poison and Dark always); the others get one look.
size_t catch_quest_looks(BBQuests current_quest, bool coastal_flying_catch);

//Fly to the beach north of Coastal Plaza and land on the sand, facing a little right of
//the Slowpoke there (tuned 2026-10-25: in frame 3 of 3 after a 250 ms camera turn left).
void central_to_slowpoke_beach(
    const ProgramInfo& info,
    ConsoleHandle& console, ProControllerContext& context,
    const BBQOption& BBQ_OPTIONS
);

//Fly from the Coastal Plaza fly point to the middle of the hill west of the plaza, where
//Flying types spawn.
void coastal_plaza_to_grassland(ConsoleHandle& console, ProControllerContext& context, bool inverted_flight);

//From near Coastal Plaza, fly back onto it: the closest fly point, or, when the player
//marker hides its icon, the direct flight that finds it from the map corner.
void fly_back_to_coastal_plaza(const ProgramInfo& info, ConsoleHandle& console, ProControllerContext& context, Language language);

//Catch a pokemon
void quest_catch(
    const ProgramInfo& info,
    ConsoleHandle& console, ProControllerContext& context,
    const BBQOption& BBQ_OPTIONS,
    BBQuests current_quest
);


//Handle battles for tera-self/defeat-wild-tera. Use first attack until victory.
void wild_battle_tera(
    const ProgramInfo& info,
    VideoStream& console, ProControllerContext& context,
    bool& tera_self
);

}
}
}
#endif
