/*  AutoStory Hall of Fame
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "Common/Cpp/Color.h"
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/PrettyPrint.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/ImageTools/ImageDiff.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonFRLG/Inference/Map/PokemonFRLG_PokemonLeagueDetectors.h"
#include "PokemonFRLG_AutoStoryHallOfFame.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using namespace std::chrono_literals;


namespace{

//  One look at the screen every so long.
constexpr Milliseconds HALL_OF_FAME_LOOK = 250ms;
//  Still this many looks (1 s): a member's hold (2 s, 120 frames).
constexpr size_t MON_HOLD_LOOKS = 4;
//  Changing this many looks (3 s): the confetti (6.7 s, 400 frames). A
//  member's slide in takes at most 20 frames (300 px at 15 a frame).
constexpr size_t CONFETTI_LOOKS = 12;
//  Still this many looks (3 s) after the confetti: the LEAGUE CHAMP card,
//  which waits for A. The player's picture before it holds 2 s, then slides.
constexpr size_t CHAMPION_HOLD_LOOKS = 12;
//  Two looks that differ by more than this (RMSD over the frame) changed.
//  A confetti piece is 8 px and a member 64 px, of a 240x160 screen; the
//  capture's noise between two frames of a still screen is under it.
constexpr double LOOK_CHANGED_RMSD = 3.0;
//  The talk after the battle to the save message: EliteFourFarmer's bound.
constexpr Milliseconds SAVE_MESSAGE_TIMEOUT = 180000ms;
constexpr Milliseconds SAVE_TIMEOUT = 30000ms;
//  The save's end to the card: six members (2.5 s each), the confetti
//  (6.7 s) and the player's picture (about 4 s) take under 30 s.
constexpr Milliseconds SHOTS_TIMEOUT = 120000ms;

std::string hall_of_fame_folder(){
    return std::format("{}AutoStory/hall_of_fame/", SCREENSHOTS_PATH());
}

//  Saves `frame` as the shot, and logs where. Returns whether it was saved.
bool save_shot(Logger& logger, const ImageViewRGB32& frame, std::string_view stamp, HallOfFameShot shot, size_t mon){
    const std::string folder = hall_of_fame_folder();
    Filesystem::create_directories(Filesystem::Path(folder));
    std::string name = std::format("{}-{}", stamp, hall_of_fame_shot_name(shot));
    if (shot == HallOfFameShot::MON){
        name += std::format("-{}", mon);
    }
    const std::string path = std::format("{}{}.png", folder, name);
    if (!frame.save(path)){
        logger.log(std::format("Hall of Fame: the {} screenshot couldn't be saved at {}.", hall_of_fame_shot_name(shot), path), COLOR_RED);
        return false;
    }
    logger.log(std::format("Hall of Fame: {} screenshot saved at {}.", hall_of_fame_shot_name(shot), path), COLOR_BLUE);
    return true;
}

//  B through the talk until the Hall of Fame saves, and the save done.
bool wait_for_save(ConsoleHandle& console, ProControllerContext& context){
    HallOfFameSavingWatcher saving(COLOR_RED);
    context.wait_for_all_requests();
    const int ret = run_until<ProControllerContext>(
        console, context,
        [](ProControllerContext& context){
            pbf_mash_button(context, BUTTON_B, SAVE_MESSAGE_TIMEOUT);
        },
        {saving}
    );
    if (ret < 0){
        return false;
    }
    console.log("Hall of Fame: saving.");
    HallOfFameSavingOverWatcher saved(COLOR_RED);
    return wait_until(console, context, SAVE_TIMEOUT, {saved}) >= 0;
}

//  Looks at the Hall of Fame until the card, saving the shots.
void take_shots(ConsoleHandle& console, ProControllerContext& context){
    const std::string stamp = now_to_filestring();
    HallOfFameShots shots;
    VideoSnapshot last;
    VideoSnapshot run_start;
    size_t mons = 0;
    const WallClock end = current_time() + SHOTS_TIMEOUT;
    while (!shots.done() && current_time() < end){
        context.wait_for(HALL_OF_FAME_LOOK);
        VideoSnapshot frame = console.video().snapshot();
        if (!frame){
            continue;
        }
        const bool changed = !last
            || last->width() != frame->width() || last->height() != frame->height()
            || ImageMatch::pixel_RMSD(*last.frame, *frame.frame) > LOOK_CHANGED_RMSD;
        const HallOfFameShot shot = shots.see(changed);
        if (shots.run_started()){
            run_start = frame;
        }
        switch (shot){
        case HallOfFameShot::NONE:
            break;
        case HallOfFameShot::MON:
            save_shot(console.logger(), *frame.frame, stamp, shot, ++mons);
            break;
        case HallOfFameShot::TEAM:
            save_shot(console.logger(), *run_start.frame, stamp, shot, mons);
            break;
        case HallOfFameShot::CHAMPION:
            save_shot(console.logger(), *frame.frame, stamp, shot, mons);
            break;
        }
        last = std::move(frame);
    }
    if (!shots.done()){
        console.log(std::format("Hall of Fame: no LEAGUE CHAMP card in {} s; {} members shot.", SHOTS_TIMEOUT.count() / 1000, mons), COLOR_RED);
    }
}

} // namespace


std::string_view hall_of_fame_shot_name(HallOfFameShot shot){
    switch (shot){
    case HallOfFameShot::NONE:      return "none";
    case HallOfFameShot::MON:       return "mon";
    case HallOfFameShot::TEAM:      return "team";
    case HallOfFameShot::CHAMPION:  return "champion";
    }
    return "?";
}

HallOfFameShot HallOfFameShots::see(bool changed){
    m_run_started = false;
    if (m_done){
        return HallOfFameShot::NONE;
    }
    if (changed){
        m_run_started = m_moving == 0;
        m_moving++;
        m_still = 0;
        m_hold_shot = false;
        if (!m_confetti && m_moving == CONFETTI_LOOKS){
            m_confetti = true;
            return HallOfFameShot::TEAM;
        }
        return HallOfFameShot::NONE;
    }
    m_still++;
    m_moving = 0;
    if (m_hold_shot){
        return HallOfFameShot::NONE;
    }
    if (!m_confetti && m_still == MON_HOLD_LOOKS){
        m_hold_shot = true;
        return HallOfFameShot::MON;
    }
    if (m_confetti && m_still == CHAMPION_HOLD_LOOKS){
        m_hold_shot = true;
        m_done = true;
        return HallOfFameShot::CHAMPION;
    }
    return HallOfFameShot::NONE;
}

Pokemon::StoryActionSpec hall_of_fame_action_spec(){
    return {.kind = Pokemon::StoryStepKind::GAME, .name = HALL_OF_FAME_ACTION, .args = {}};
}

std::optional<Pokemon::Outcome> run_hall_of_fame_step(
    ConsoleHandle& console, ProControllerContext& context, const Pokemon::StoryStep& step
){
    if (step.action != HALL_OF_FAME_ACTION){
        return std::nullopt;
    }
    if (!wait_for_save(console, context)){
        return Pokemon::outcome_failed("no_hall_of_fame", "the Hall of Fame's save message didn't show and go");
    }
    console.log(std::format("Hall of Fame: saved; taking the screenshots in {}.", hall_of_fame_folder()));
    take_shots(console, context);
    return Pokemon::outcome_ok();
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
