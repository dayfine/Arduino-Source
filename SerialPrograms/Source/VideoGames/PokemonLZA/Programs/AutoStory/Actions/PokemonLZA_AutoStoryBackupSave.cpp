/*  PokemonLZA AutoStory - Backup save
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <chrono>
#include <format>
#include <optional>
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_Superscalar.h"
#include "NintendoSwitch/Programs/NintendoSwitch_GameEntry.h"
#include "VideoGames/PokemonLZA/PokemonLZA_Settings.h"
#include "VideoGames/PokemonLZA/Programs/PokemonLZA_GameEntry.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryBackupCardReader.h"
#include "PokemonLZA_AutoStoryLaunch.h"
#include "PokemonLZA_AutoStoryBackupSave.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;


namespace{

//  Up with B + X held this long opens the card (the lab's "dpad UP 2000
//  B+X", console B, 2026-11-16).
constexpr Milliseconds BACKUP_COMBO_HOLD = 2000ms;
//  Looks for the card after the combo, CARD_POLL apart: it was up 3 s
//  after the release.
constexpr size_t CARD_LOOKS = 12;
constexpr Milliseconds CARD_POLL = 500ms;
//  After the game is in, before the first look: as reload_latest_save().
constexpr Milliseconds LOAD_SETTLE = 1000ms;

std::optional<BackupCard> open_card(StoryEnv& env){
    ConsoleHandle& console = env.env.console;
    ProControllerContext& context = env.context;
    launch_plza_from_home(console, context);
    if (!openedgame_to_gamemenu(console, context, GameSettings::instance().START_GAME_WAIT)){
        console.log("[Rewind] the title screen did not show after the launch");
        return std::nullopt;
    }
    pbf_wait(context, 1000ms);
    ssf_press_dpad(context, DPAD_UP, /*delay=*/0ms, /*hold=*/BACKUP_COMBO_HOLD);
    ssf_press_button(context, BUTTON_B | BUTTON_X, /*delay=*/BACKUP_COMBO_HOLD, /*hold=*/BACKUP_COMBO_HOLD, /*cool=*/0ms);
    for (size_t look = 0; look < CARD_LOOKS; look++){
        context.wait_for(CARD_POLL);
        const VideoSnapshot frame = console.video().snapshot();
        if (!frame){
            continue;
        }
        if (const std::optional<size_t> registered = read_backup_card_registered(console.logger(), frame)){
            return BackupCard{.registered = *registered};
        }
    }
    console.log(std::format("[Rewind] no backup card read on {} looks", CARD_LOOKS));
    return std::nullopt;
}

Pokemon::Outcome load(StoryEnv& env){
    if (!gamemenu_to_ingame(env.env.console, env.context, GameSettings::instance().ENTER_GAME_WAIT0)){
        return Pokemon::outcome_failed("backup_load_failed", "the game did not reach the overworld from the backup card");
    }
    env.context.wait_for(LOAD_SETTLE);
    return Pokemon::outcome_ok();
}

void cancel(StoryEnv& env){
    pbf_press_button(env.context, BUTTON_B, /*hold=*/160ms, /*release=*/1000ms);
    env.context.wait_for_all_requests();
}

} // namespace


BackupSave console_backup_save(StoryEnv& env){
    return BackupSave{
        .open_card = [&env](PlzaBackend&){
            return open_card(env);
        },
        .load = [&env](PlzaBackend&){
            return load(env);
        },
        .cancel = [&env](PlzaBackend&){
            cancel(env);
        },
    };
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
