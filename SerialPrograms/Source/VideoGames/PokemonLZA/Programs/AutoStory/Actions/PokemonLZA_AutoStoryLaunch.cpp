/*  PokemonLZA AutoStory - Launch
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "Common/Cpp/Exceptions.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "NintendoSwitch/NintendoSwitch_Settings.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Programs/NintendoSwitch_GameEntry.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryIntroScreens.h"
#include "PokemonLZA_AutoStoryLaunch.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

constexpr Milliseconds HOME_TILE_RELEASE = 500ms;

//  The highlighted HOME tile is PLZA's; false without a frame.
bool highlighted_tile_is_plza(ConsoleHandle& console, ProControllerContext& context){
    context.wait_for_all_requests();
    const VideoSnapshot frame = console.video().snapshot();
    return frame && is_home_tile_plza(frame);
}

} // namespace


std::optional<size_t> find_plza_tile(
    const std::function<bool()>& highlighted_is_plza,
    const std::function<void()>& move_right
){
    for (size_t tile = 0; tile < HOME_TILES_SEARCHED; tile++){
        if (tile > 0){
            move_right();
        }
        if (highlighted_is_plza()){
            return tile;
        }
    }
    return std::nullopt;
}

void launch_plza_from_home(ConsoleHandle& console, ProControllerContext& context){
    //  HOME from the game or from HOME: the cursor on the first tile, the
    //  running game's when one runs.
    go_home(console, context);
    const bool first_is_plza = highlighted_tile_is_plza(console, context);
    console.log(std::format(
        "AutoStory launch: HOME's first tile (the running game, if any) is {}; closing it.",
        first_is_plza ? "PLZA" : "another game"
    ));
    close_game_from_home(console, context);

    pbf_press_button(context, BUTTON_HOME, /*hold=*/160ms, ConsoleSettings::instance().SETTINGS_TO_HOME_DELAY0);
    const std::optional<size_t> tile = find_plza_tile(
        [&]{ return highlighted_tile_is_plza(console, context); },
        [&]{ pbf_press_dpad(context, DPAD_RIGHT, /*hold=*/80ms, HOME_TILE_RELEASE); }
    );
    if (!tile){
        throw UserSetupError(
            console,
            std::format(
                "AutoStory: Pokemon Legends: Z-A is not on HOME's first {} tiles. Play it once so its tile moves to the front.",
                HOME_TILES_SEARCHED
            )
        );
    }
    console.log(std::format("AutoStory launch: launching PLZA, HOME's tile {}.", *tile));
    //  Slot 0: the cursor stays on PLZA's tile.
    start_game_from_home(console, context, /*tolerate_update_menu=*/true, /*game_slot=*/0, /*user_slot=*/0);
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
