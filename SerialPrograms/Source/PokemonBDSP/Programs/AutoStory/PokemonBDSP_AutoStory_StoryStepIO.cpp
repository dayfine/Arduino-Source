/*  BDSP AutoStory - Story step IO
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/ImageTools/ImageStats.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/VisualDetectors/BlackScreenDetector.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_Superscalar.h"
#include "PokemonBDSP/Inference/Battles/PokemonBDSP_BattleMenuDetector.h"
#include "PokemonBDSP/Inference/PokemonBDSP_DialogDetector.h"
#include "PokemonBDSP/Programs/AutoStory/Detect/PokemonBDSP_AutoStory_OverworldDetector.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Location.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_RouteWalk.h"
#include "PokemonBDSP_AutoStory_Talks.h"
#include "PokemonBDSP_AutoStory_Walks.h"
#include "PokemonBDSP_AutoStory_WildBattle.h"
#include "PokemonBDSP_AutoStory_StoryPlaces.h"
#include "PokemonBDSP_AutoStory_StoryStepIO.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace std::chrono_literals;
using Pokemon::Direction;


namespace{

//  One rule for scenes and map changes: v2's long B mashes, with an end
//  check. Press through the text, and keep pressing B while the screen is
//  quiet: a scene can pause while someone walks off (Dawn and Barry after
//  Rowan leaves, r23), fade dimly, not to black, to another map (r25), or
//  wait on a box the detector misses (r26). B never starts a talk on the
//  overworld. Done once the overworld has shown, with no text and a still
//  screen, for 5 s of presses: after r31's fade home a scripted walk-in ran
//  5 s with no text before Mom spoke.
//
//  A step or scene that ends on another map (`fade`) must fade on the way,
//  and only the quiet after the fade counts: r30's scene went quiet for 5 s
//  before the fade home, and r28 "went out" a tile beside the door and
//  walked a whole route in the house. No fade is a map mismatch.
constexpr double FADE_DARKNESS = 75;    //  brightness 0 to 255; cave fades reach ~45..64, r24's dim fade goes darker
void settle(VideoStream& stream, ProControllerContext& context, Button button, const std::string& label, bool fade){
    const WallClock none = WallClock::max();
    const WallClock deadline = current_time() + 120s;
    ShortDialogDetector dialog;
    OverworldDetector overworld;
    BattleMenuDetector battle_menu(BattleType::STANDARD);
    WallClock quiet_since = none, calm_since = none;
    double darkest = 255;
    VideoSnapshot previous;
    while (current_time() < deadline){
        context.wait_for_all_requests();
        const auto frame = stream.video().snapshot();
        const WallClock now = current_time();
        if (frame && battle_menu.detect(frame)){
            stream.log("[Story] " + label + ": in wild battle during settle; fleeing.");
            run_from_wild_battle(stream, context);
            continue;
        }
        if (frame){
            darkest = std::min(darkest, image_average(*frame.frame).sum() / 3);
        }
        const bool talking = frame && dialog.detect(frame);
        const bool still = frame && previous && walk_screen_change(*previous.frame, *frame.frame) < WALK_TAP_BLOCKED_CHANGE;
        previous = frame;
        const bool calm = !talking && overworld.detect(frame) && (!fade || darkest < FADE_DARKNESS);
        calm_since = calm ? (calm_since == none ? now : calm_since) : none;
        if (calm && now - calm_since >= 15000ms){
            stream.log(std::format("[Story] {}: settled on calm overworld; darkest frame {:.0f}.", label, darkest));
            return;
        }
        if (!talking && still && overworld.detect(frame) && (!fade || darkest < FADE_DARKNESS)){
            quiet_since = quiet_since == none ? now : quiet_since;
            if (now - quiet_since >= 5000ms){
                stream.log(std::format("[Story] {}: settled; darkest frame {:.0f}.", label, darkest));
                return;
            }
        }else{
            quiet_since = none;
        }
        pbf_press_button(context, talking ? button : BUTTON_B, 80ms, 1200ms);
    }
    OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
        fade && darkest >= FADE_DARKNESS
            ? label + ": no fade within 120 s, so we are not where the step goes (map mismatch)."
            : label + ": not back on a quiet overworld within 120 s.",
        stream);
}

//  Holds the D-pad toward a door (with B after the shoes) until the screen goes
//  dark or a line shows, up to 12 s. Returns whether it went dark. A line
//  stops it: holding on under Mom's "Oh, Kosmos!" wasted 12 s (r45). A wild
//  battle in a cave corridor runs away before returning.
DpadPosition dpad_toward(Direction direction){
    return
        direction == Direction::NORTH ? DPAD_UP :
        direction == Direction::SOUTH ? DPAD_DOWN :
        direction == Direction::WEST ? DPAD_LEFT : DPAD_RIGHT;
}

bool run_into_door(VideoStream& stream, ProControllerContext& context, Direction direction, bool run){
    const DpadPosition dpad = dpad_toward(direction);
    BlackScreenWatcher dark(COLOR_RED, {0.1, 0.1, 0.8, 0.8}, 3 * FADE_DARKNESS, 60);
    ShortDialogWatcher dialog(COLOR_YELLOW);
    BattleMenuWatcher battle(BattleType::STANDARD, COLOR_YELLOW);
    const int ret = run_until<ProControllerContext>(stream, context, [&](ProControllerContext& context){
        if (run){
            ssf_press_button(context, BUTTON_B, 0ms, 12000ms, 0ms);
        }
        pbf_press_dpad(context, dpad, 12000ms, 0ms);
    }, {{dark}, {dialog}, {battle}});
    if (ret == 2){
        stream.log("[Story] run_into_door: wild battle; fleeing.");
        run_from_wild_battle(stream, context);
        return false;
    }
    return ret == 0;
}

//  What the screen showed after a tap toward a door or a cave mouth.
enum class TapLook{
    NOTHING,
    DARK,
    BANNER,
};

//  Looks for the location banner `banner` (and, if `dark_too`, the fade)
//  for up to `wait`.
TapLook look_after_tap(VideoStream& stream, ProControllerContext& context, const std::string& banner, Milliseconds wait, bool dark_too){
    const WallClock deadline = current_time() + wait;
    while (true){
        context.wait_for_all_requests();
        const VideoSnapshot screen = stream.video().snapshot();
        if (screen && location_banner_matches(read_location_banner(*screen.frame), banner)){
            return TapLook::BANNER;
        }
        if (dark_too && screen && image_average(*screen.frame).sum() / 3 < FADE_DARKNESS){
            return TapLook::DARK;
        }
        if (current_time() >= deadline){
            return TapLook::NOTHING;
        }
        context.wait_for(100ms);
    }
}

} // namespace


ConsoleStoryStepIO::ConsoleStoryStepIO(VideoStream& stream, ProControllerContext& context)
    : m_stream(stream)
    , m_context(context)
    , m_escape(run_from_wild_battle)
    , m_route(stream, context, m_escape)
{}

void ConsoleStoryStepIO::fight(const std::string& trainer){
    if (trainer.find("Twins") != std::string::npos){
        talk_or_fight(m_stream, m_context, trainer, TalkExpect::BATTLE, BattleFormat::DOUBLES);
        return;
    }
    fight_route_trainer(m_stream, m_context, trainer);
}

void ConsoleStoryStepIO::walk_until_stopped(Direction direction, size_t tiles, const std::string& label){
    PokemonBDSP::walk_until_stopped(m_stream, m_context, direction, tiles, label);
}

bool ConsoleStoryStepIO::run_until_map(Direction direction, Milliseconds limit, const std::string& map, const std::string& label){
    const auto [x, y] = dpad_of(direction);
    const WalkResult walk = walk_until_dpad(m_stream, m_context, run_from_wild_battle, {x, y}, limit, label, {
        .arrived = [&](const ImageViewRGB32& screen){ return location_banner_matches(read_location_banner(screen), map); },
        .pace = DpadPace::RUN,
    });
    m_stream.log(std::format("[Story] {}: ran {} to {}'s banner: {}.", label, direction_name(direction), map, walk_stop_name(walk.stop)));
    return walk.stop == WalkStop::ARRIVED;
}

bool ConsoleStoryStepIO::run_into_door(Direction direction, bool run){
    return PokemonBDSP::run_into_door(m_stream, m_context, direction, run);
}

//  A held walk can carry on past a fade that goes unseen: out of the Gym it
//  walked on south (r219g), and through the Oreburgh Gate's west mouth on
//  west into a pocket by Route 203's pond (r75). Taps during the fade are
//  eaten.
std::optional<size_t> ConsoleStoryStepIO::tap_to_banner(Direction direction, const std::string& map, size_t max_taps){
    const std::string& banner = story_banner_name(map);
    for (size_t tap = 1; tap <= max_taps; tap++){
        pbf_press_dpad(m_context, dpad_toward(direction), 160ms, 0ms);
        TapLook look = look_after_tap(m_stream, m_context, banner, 1500ms, true);
        const bool faded = look == TapLook::DARK;
        if (faded){
            look = look_after_tap(m_stream, m_context, banner, 8000ms, false);
        }
        if (look == TapLook::BANNER){
            m_context.wait_for(1000ms);
            return tap;
        }
        if (faded){
            break;
        }
    }
    return std::nullopt;
}

void ConsoleStoryStepIO::settle(Button button, const std::string& label, bool fade){
    PokemonBDSP::settle(m_stream, m_context, button, label, fade);
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
