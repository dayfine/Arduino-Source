/*  BDSP AutoStory - Story step IO
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include "Common/Cpp/PrettyPrint.h"
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/ImageTools/ImageStats.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/VisualDetectors/BlackScreenDetector.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_Superscalar.h"
#include "PokemonBDSP/Inference/PokemonBDSP_TransitionWatchers.h"
#include "PokemonBDSP/Inference/Battles/PokemonBDSP_BattleMenuDetector.h"
#include "PokemonBDSP/Inference/PokemonBDSP_DialogDetector.h"
#include "PokemonBDSP/Programs/AutoStory/Detect/PokemonBDSP_AutoStory_OverworldDetector.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Location.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_RouteWalk.h"
#include "Actions/PokemonBDSP_AutoStory_Talks.h"
#include "PokemonBDSP_AutoStory_Walks.h"
#include "Actions/PokemonBDSP_AutoStory_WildBattle.h"
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

DpadPosition dpad_toward(Direction direction){
    return
        direction == Direction::NORTH ? DPAD_UP :
        direction == Direction::SOUTH ? DPAD_DOWN :
        direction == Direction::WEST ? DPAD_LEFT : DPAD_RIGHT;
}

//  A dark-screen watcher that fires only from `from` on, and keeps when the
//  screen last went dark: where a wild battle's fade began.
class DarkFrom : public VisualInferenceCallback{
public:
    DarkFrom(BlackScreenWatcher& dark, WallClock from)
        : VisualInferenceCallback("DarkFrom")
        , m_dark(dark)
        , m_from(from)
    {}
    virtual void make_overlays(VideoOverlaySet& items) const override{
        m_dark.make_overlays(items);
    }
    virtual bool process_frame(const ImageViewRGB32& frame, WallClock timestamp) override{
        const bool dark = m_dark.process_frame(frame, timestamp);
        if (dark && !m_was_dark){
            m_went_dark = timestamp;
        }
        m_was_dark = dark;
        return dark && timestamp >= m_from;
    }
    //  When the screen last went dark; WallClock::max() if it has not.
    WallClock went_dark() const{
        return m_went_dark;
    }

private:
    BlackScreenWatcher& m_dark;
    WallClock m_from;
    bool m_was_dark = false;
    WallClock m_went_dark = WallClock::max();
};

//  How one hold toward a door ended: run_until()'s index (0 dark, 1 white,
//  2 a line, 3 a wild battle; -1 time up), and when the screen last went dark.
struct DoorHoldEnd{
    int ret;
    WallClock went_dark;
};

DoorHoldEnd hold_into_door(VideoStream& stream, ProControllerContext& context, DpadPosition dpad, bool run, Milliseconds hold, Milliseconds dark_from){
    BlackScreenWatcher dark(COLOR_RED, {0.1, 0.1, 0.8, 0.8}, 3 * FADE_DARKNESS, 60, BlackScreenWatcher::FinderType::PRESENT, 50ms);
    DarkFrom late_dark(dark, current_time() + dark_from);
    WhiteScreenWatcher white(COLOR_BLUE, {0.1, 0.1, 0.8, 0.8}, WhiteScreenWatcher::FinderType::PRESENT, 60ms);
    ShortDialogWatcher dialog(COLOR_YELLOW);
    BattleMenuWatcher battle(BattleType::STANDARD, COLOR_YELLOW);
    const int ret = run_until<ProControllerContext>(stream, context, [&](ProControllerContext& context){
        if (run){
            ssf_press_button(context, BUTTON_B, 0ms, hold, 0ms);
        }
        pbf_press_dpad(context, dpad, hold, 0ms);
    }, {{late_dark}, {white}, {dialog}, {battle}});
    return {.ret = ret, .went_dark = late_dark.went_dark()};
}

//  Wild battles a run into a door flees and runs on after.
constexpr size_t DOOR_RUN_BATTLES = 3;

//  Holds the D-pad toward a door (with B after the shoes) for `timeout`
//  until the screen goes dark or a line shows. Returns whether it went dark.
//  A line stops it: holding on under Mom's "Oh, Kosmos!" wasted 12 s (r45).
//  Dark in the first `dark_from` of running is not the door (lab-B
//  2026-11-08: dark at 1.8 s, halfway down the mine's entrance hall). A wild
//  battle on the way is fled, and the run goes on for the time it had left:
//  only the running counts, up to the battle's fade.
bool run_into_door(VideoStream& stream, ProControllerContext& context, Direction direction, bool run, Milliseconds timeout, Milliseconds dark_from){
    Milliseconds left = timeout;
    for (size_t battles = 0;; battles++){
        const WallClock start = current_time();
        const DoorHoldEnd end = hold_into_door(stream, context, dpad_toward(direction), run, left, dark_from);
        if (end.ret == 3){
            const WallClock stopped = std::min(end.went_dark, current_time());
            const Milliseconds ran = std::chrono::duration_cast<Milliseconds>(stopped - start) - ROUTE_START_DELAY;
            const Milliseconds counted = std::clamp(ran, 0ms, left);
            left -= counted;
            dark_from -= std::min(dark_from, counted);
            stream.log(std::format("[Story] run_into_door: wild battle after {} ms of running; fleeing.", counted.count()));
            run_from_wild_battle(stream, context);
            if (battles < DOOR_RUN_BATTLES && left > 0ms){
                continue;
            }
            return false;
        }
        if (end.ret == 0){
            stream.log("[Story] run_into_door: black screen fade detected.");
        }else if (end.ret == 1){
            stream.log("[Story] run_into_door: white screen fade detected.");
        }else if (end.ret == 2){
            stream.log("[Story] run_into_door: dialog detected.");
        }else if (end.went_dark != WallClock::max()){
            //  Dark only before dark_from: early, or the door's fade itself.
            stream.log(std::format("[Story] run_into_door: timed out; dark {} ms in, before dark_from {} ms.",
                std::chrono::duration_cast<Milliseconds>(end.went_dark - start).count(), dark_from.count()));
        }else{
            stream.log("[Story] run_into_door: timed out without detecting fade.");
        }
        return end.ret == 0 || end.ret == 1;
    }
}

//  What the screen showed after a tap toward a door or a cave mouth.
enum class TapLook{
    NOTHING,
    DARK,
    BANNER,
};

//  The mean of r, g and b over the screen, 0 to 255; 255 with no frame.
double brightness_of(const VideoSnapshot& screen){
    return screen ? image_average(*screen.frame).sum() / 3 : 255;
}

void keep_banner_frame(const ImageViewRGB32& frame, const std::string& folder, const std::string& banner){
    Filesystem::create_directories(Filesystem::Path(folder));
    frame.save(folder + now_to_filestring() + "-" + normalize_location_name(banner) + ".png");
}
void log_new_banner_read(VideoStream& stream, const std::string& read, std::string& last_read, const std::string& banner, WallClock start){
    if (read.empty() || read == last_read){
        return;
    }
    stream.log(std::format("[Banner] read \"{}\" after {} ms, looking for \"{}\".",
        read, std::chrono::duration_cast<Milliseconds>(current_time() - start).count(), banner));
    last_read = read;
}

//  Looks for the location banner `banner` for up to `wait`, and with
//  `before` (the brightness before the tap) for the fade (tap_faded()).
//  Logs each distinct banner read and how the wait ended: the Oreburgh Gate's
//  entry missed its banner twice (lab-B, 2026-11-08 15:31 and 15:39) though
//  a hand probe saw it 1.5 s after the fade, and nothing said why.
TapLook look_after_tap(VideoStream& stream, ProControllerContext& context, const std::string& banner, Milliseconds wait, std::optional<double> before){
    const WallClock start = current_time();
    const WallClock deadline = start + wait;
    std::string last_read;
    double darkest = 255;
    //  Every 500 ms of a long wait (the one after a fade) a frame goes to
    //  Screenshots/AutoStory/banner/, to see what the game showed.
    const std::string folder = SCREENSHOTS_PATH() + "AutoStory/banner/";
    WallClock next_frame = start;
    while (true){
        context.wait_for_all_requests();
        const VideoSnapshot screen = stream.video().snapshot();
        if (!before && screen && current_time() >= next_frame){
            keep_banner_frame(*screen.frame, folder, banner);
            next_frame += 500ms;
        }
        const std::string read = screen ? read_location_banner(*screen.frame) : "";
        log_new_banner_read(stream, read, last_read, banner, start);
        if (screen && location_banner_matches(read, banner)){
            return TapLook::BANNER;
        }
        const double brightness = brightness_of(screen);
        darkest = std::min(darkest, brightness);
        if (before && tap_faded(*before, brightness)){
            return TapLook::DARK;
        }
        if (current_time() >= deadline){
            stream.log(std::format("[Banner] no \"{}\" in {} ms; darkest frame {:.0f}, last read \"{}\".",
                banner, wait.count(), darkest, last_read));
            return TapLook::NOTHING;
        }
        context.wait_for(100ms);
    }
}

} // namespace


//  Black as the Gym's door fades (< 25), or a drop to dark: a look under
//  FADE_DARKNESS is not a fade by itself, since a cave's floor is that dim
//  (the Oreburgh Gate's about 46, lab-B 2026-11-08), but a fade into a cave
//  from a lit street goes from about 120 to 45..64.
//  TODO(#268 slice 3): stopgap. A uniform-screen fade watcher (the shared
//  Black/WhiteScreen stddev test, as in upstream #1717) should replace this
//  brightness drop; remove it once that watcher passes the Gate frames.
constexpr double TAP_FADE_BLACK = 25;
constexpr double TAP_FADE_DROP = 0.7;
bool tap_faded(double before, double after){
    return after < TAP_FADE_BLACK || (after < FADE_DARKNESS && after < before * TAP_FADE_DROP);
}

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

bool ConsoleStoryStepIO::run_into_door(Direction direction, bool run, Milliseconds timeout, Milliseconds dark_from){
    return PokemonBDSP::run_into_door(m_stream, m_context, direction, run, timeout, dark_from);
}

//  A held walk can carry on past a fade that goes unseen: out of the Gym it
//  walked on south (r219g), and through the Oreburgh Gate's west mouth on
//  west into a pocket by Route 203's pond (r75). Taps during the fade are
//  eaten. The banner before the first tap means we are through already:
//  the walk before slid onto the warp (lab-B 2026-11-08 15:31).
//  TODO(#268 slice 2): stopgap. The "through already" check sees only a
//  banner still on screen; once walkers report the fade or banner they saw
//  (WalkResult::warp), read that instead and remove this check.
std::optional<size_t> ConsoleStoryStepIO::tap_to_banner(Direction direction, const std::string& map, size_t max_taps){
    const std::string& banner = story_banner_name(map);
    m_context.wait_for_all_requests();
    VideoSnapshot screen = m_stream.video().snapshot();
    if (screen && location_banner_matches(read_location_banner(*screen.frame), banner)){
        m_stream.log("[Banner] \"" + banner + "\" shows before the first tap: through already.");
        return 0;
    }
    for (size_t tap = 1; tap <= max_taps; tap++){
        const double before = brightness_of(screen);
        pbf_press_dpad(m_context, dpad_toward(direction), 160ms, 0ms);
        TapLook look = look_after_tap(m_stream, m_context, banner, 1500ms, before);
        const bool faded = look == TapLook::DARK;
        if (faded){
            look = look_after_tap(m_stream, m_context, banner, 8000ms, std::nullopt);
        }
        if (look == TapLook::BANNER){
            m_context.wait_for(1000ms);
            return tap;
        }
        if (faded){
            break;
        }
        screen = m_stream.video().snapshot();
    }
    return std::nullopt;
}

void ConsoleStoryStepIO::settle(Button button, const std::string& label, bool fade){
    PokemonBDSP::settle(m_stream, m_context, button, label, fade);
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
