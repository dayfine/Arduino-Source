/*  AutoStory Boulder Presses
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTools/ImageDiff.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_BattleScreenDetector.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_BoulderPushReader.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Navigation.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Settings.h"
#include "PokemonFRLG_AutoStoryBoulderPresses.h"
#include "PokemonFRLG_AutoStoryPresses.h"
#include "PokemonFRLG_AutoStoryTrainerLine.h"
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

std::string_view describe(const BoulderPressRead& read){
    if (read.scrolled){
        return "the player walked a tile";
    }
    return read.boulder_moved ? "the boulder moved, the player in place" : "nothing moved";
}

}  // namespace


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
    BattleScreenWatcher battle(COLOR_RED);
    if (wait_until(m_console, m_context, WILD_BATTLE_IN_LINE_WATCH, {battle}) != 0){
        return false;
    }
    m_console.log("Boulder: a wild battle stopped the push; running from it, then tapping again.", COLOR_ORANGE);
    flee_battle(m_console, m_context);
    wait_out_battle_exit(m_console, m_context);
    return true;
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
