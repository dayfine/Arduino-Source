/*  AutoStory Boulder Presses
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTools/ImageDiff.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_BoulderPushReader.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Navigation.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Settings.h"
#include "PokemonFRLG_AutoStoryBoulderPresses.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_MovementStepPresses.h"
#include "PokemonFRLG_AutoStoryPresses.h"
#include "PokemonFRLG_AutoStoryQuietOverworld.h"
#include "PokemonFRLG_AutoStoryStepInterrupts.h"
#include "PokemonFRLG_AutoStoryTrainerMessage.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

//  How long the screen is watched, a shot every BOULDER_STILL_POLL, for
//  two shots alike after a tap toward a boulder: the push's slow slide
//  ends by the tap's release (MovementMechanics::STEP_TAP_RELEASE), so
//  this only waits out a late frame.
constexpr Milliseconds BOULDER_STILL_POLL(100);
constexpr int BOULDER_STILL_POLLS = 5;
//  How much two shots of a still screen may differ.
constexpr double BOULDER_STILL_RMSD = 6.0;
//  How long a box that stopped a push may take to close.
constexpr Milliseconds BOX_TIMEOUT(20000);
//  After A, the overworld this long quiet with nothing showing means A
//  found nothing to use: Strength's question shows well within it. Looked
//  for at most NOTHING_LOOK_TIMEOUT.
constexpr Milliseconds NOTHING_QUIET(1000);
constexpr Milliseconds NOTHING_LOOK_TIMEOUT(3000);
//  How long Strength's YES/NO, and then its message, may take.
constexpr Milliseconds YES_NO_TIMEOUT(60000);
constexpr Milliseconds MESSAGE_TIMEOUT(300000);

std::string_view describe(const BoulderPressRead& read){
    if (read.scrolled){
        return "the player walked a tile";
    }
    return read.boulder_moved ? "the boulder moved, the player in place" : "nothing moved";
}

}  // namespace


std::optional<std::string> use_strength(ConsoleHandle& console, ProControllerContext& context, const MapPosition& here, Direction direction){
    pbf_press_button(context, BUTTON_A, /*hold=*/160ms, /*release=*/800ms);
    context.wait_for_all_requests();
    ConsoleScreenLook eyes(console, context);
    const Pokemon::ScreenLookFunction look = [&eyes](bool fresh){ return eyes.look(fresh); };
    const Pokemon::ScreenSettle settle = wait_for_quiet_overworld(look, NOTHING_QUIET, NOTHING_LOOK_TIMEOUT);
    console.log(std::format("Boulder: after A for Strength, {}.", Pokemon::describe(settle)));
    if (quiet_overworld(settle, NOTHING_QUIET)){
        return std::format("no boulder {} of {}: A for Strength showed nothing.", direction_name(direction), to_string(here));
    }
    answer_yes_no(console, context, /*yes=*/true, BUTTON_B, YES_NO_TIMEOUT);
    mash_until_overworld(console, context, MESSAGE_TIMEOUT);
    return std::nullopt;
}

ConsoleBoulderPresses::ConsoleBoulderPresses(ConsoleHandle& console, ProControllerContext& context, PlayerGender player)
    : m_console(console)
    , m_context(context)
    , m_player(player)
{}

BoulderPressRead ConsoleBoulderPresses::press(Direction direction){
    m_context.wait_for_all_requests();
    const VideoSnapshot before = m_console.video().snapshot();
    step_once(m_console, m_context, direction, direction, m_player);
    const VideoSnapshot after = still_shot();
    if (!before || !after){
        m_console.log("Boulder: no shot of the screen around the tap: read as nothing moved.", COLOR_ORANGE);
        return {};
    }
    const BoulderPressRead read = read_boulder_press(*before.frame, *after.frame, GameSettings::instance().GAME_BOX, direction);
    m_console.log(std::format("Boulder: tap {}: {}.", direction_name(direction), describe(read)));
    return read;
}

bool ConsoleBoulderPresses::clear_interrupt(){
    m_context.wait_for_all_requests();
    ConsoleScreenLook eyes(m_console, m_context);
    const Pokemon::ScreenLookFunction look = [&eyes](bool fresh){ return eyes.look(fresh); };
    Pokemon::ScreenSettle settle = wait_for_quiet_overworld(look, MovementMechanics::AFTER_BATTLE_QUIET, BATTLE_EXIT_TIMEOUT);
    if (settle.showing.has(ScreenEvent::BATTLE_STARTED)){
        m_console.log("Boulder: a wild battle stopped the push; running from it.", COLOR_ORANGE);
        flee_battle(m_console, m_context);
        wait_out_battle_exit(m_console, m_context);
        settle = wait_for_quiet_overworld(look, MovementMechanics::AFTER_BATTLE_QUIET, BATTLE_EXIT_TIMEOUT);
    }else if (settle.showing.has(ScreenEvent::DIALOG_BOX) || settle.showing.has(ScreenEvent::CHOICE_BOX)){
        m_console.log("Boulder: a box stopped the push; closing it.", COLOR_ORANGE);
        mash_until_overworld(m_console, m_context, BOX_TIMEOUT);
        settle = wait_for_quiet_overworld(look, MovementMechanics::AFTER_BATTLE_QUIET, BATTLE_EXIT_TIMEOUT);
    }
    const bool quiet = quiet_overworld(settle, MovementMechanics::AFTER_BATTLE_QUIET);
    m_console.log(std::format("Boulder: after the tap, {}{}.", Pokemon::describe(settle), quiet ? "; tapping again" : ""), quiet ? COLOR_BLACK : COLOR_RED);
    return quiet;
}

VideoSnapshot ConsoleBoulderPresses::still_shot(){
    const ImageFloatBox& game = GameSettings::instance().GAME_BOX;
    VideoSnapshot last = m_console.video().snapshot();
    for (int poll = 0; last && poll < BOULDER_STILL_POLLS; poll++){
        m_context.wait_for(BOULDER_STILL_POLL);
        VideoSnapshot now = m_console.video().snapshot();
        if (!now){
            break;
        }
        const bool still = ImageMatch::pixel_RMSD(
            extract_box_reference(*last.frame, game), extract_box_reference(*now.frame, game)
        ) <= BOULDER_STILL_RMSD;
        last = std::move(now);
        if (still){
            break;
        }
    }
    return last;
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
