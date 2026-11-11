/*  BDSP AutoStory - Story step IO
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include <vector>
#include "Common/Cpp/PrettyPrint.h"
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/ImageTools/ImageStats.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
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
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_LocationBannerWatcher.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_RouteWalk.h"
#include "Actions/PokemonBDSP_AutoStory_TalkOver.h"
#include "Actions/PokemonBDSP_AutoStory_Talks.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_WalkDpad.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_WalkResult.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_WalkScreen.h"
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

DpadPosition dpad_toward(Direction direction){
    return
        direction == Direction::NORTH ? DPAD_UP :
        direction == Direction::SOUTH ? DPAD_DOWN :
        direction == Direction::WEST ? DPAD_LEFT : DPAD_RIGHT;
}

//  A door run's fade watch (DOOR_FADE_RULE) that fires only from `from` on.
//  It keeps a small copy of each frame for the scene comparison, and the
//  fade it saw.
class DoorFadeFrom : public VisualInferenceCallback{
public:
    explicit DoorFadeFrom(WallClock from)
        : VisualInferenceCallback("DoorFadeFrom")
        , m_watch(DOOR_FADE_RULE)
        , m_from(from)
    {}
    virtual void make_overlays(VideoOverlaySet& items) const override{
        items.add(COLOR_RED, {0.1, 0.1, 0.8, 0.8});
    }
    virtual bool process_frame(const ImageViewRGB32& frame, WallClock timestamp) override{
        const Pokemon::TransitionSample sample = Pokemon::transition_sample(VideoSnapshot(frame.scale_to(192, 108), timestamp));
        m_fade = m_watch.push(sample);
        return m_fade != WarpFade::NONE && timestamp >= m_from;
    }
    WarpFade fade() const{
        return m_fade;
    }
    const WarpFadeWatch& watch() const{
        return m_watch;
    }

private:
    WarpFadeWatch m_watch;
    WallClock m_from;
    WarpFade m_fade = WarpFade::NONE;
};

//  How one hold toward a door ended: run_until()'s index (0 the fade, 1 a
//  line, 2 a wild battle, 3 the far map's banner; -1 time up), the fade
//  and how it showed, when the screen last went dark, and the banner read.
struct DoorHoldEnd{
    int ret;
    WarpFade fade;
    std::string how;
    WallClock went_dark;
    std::string banner;
};

DoorHoldEnd hold_into_door(VideoStream& stream, ProControllerContext& context, DpadPosition dpad, bool run, Milliseconds hold, Milliseconds dark_from, const std::string& banner){
    DoorFadeFrom fade(current_time() + dark_from);
    ShortDialogWatcher dialog(COLOR_YELLOW);
    BattleMenuWatcher battle(BattleType::STANDARD, COLOR_YELLOW);
    LocationBannerWatcher far_banner(banner);
    std::vector<PeriodicInferenceCallback> watchers{{fade}, {dialog}, {battle}};
    if (!banner.empty()){
        watchers.emplace_back(far_banner);
    }
    const int ret = run_until<ProControllerContext>(stream, context, [&](ProControllerContext& context){
        if (run){
            ssf_press_button(context, BUTTON_B, 0ms, hold, 0ms);
        }
        pbf_press_dpad(context, dpad, hold, 0ms);
    }, watchers);
    return {
        .ret = ret,
        .fade = ret == 0 ? fade.fade() : WarpFade::NONE,
        .how = fade.watch().how(),
        .went_dark = fade.watch().went_dark().value_or(WallClock::max()),
        .banner = ret == 3 ? far_banner.seen() : "",
    };
}

//  Wild battles a run into a door flees and runs on after.
constexpr size_t DOOR_RUN_BATTLES = 3;

//  Holds the D-pad toward a door (with B after the shoes) for `timeout`
//  until its fade (DOOR_FADE_RULE) or a line shows. Returns what it saw.
//  A fade whose hold fell between two frames shows at the scene after it,
//  so a missed 50 ms fade no longer runs the whole timeout.
//  A line stops it: holding on under Mom's "Oh, Kosmos!" wasted 12 s (r45).
//  Dark in the first `dark_from` of running is not the door (lab-B
//  2026-11-08: dark at 1.8 s, halfway down the mine's entrance hall). A wild
//  battle on the way is fled, and the run goes on for the time it had left:
//  only the running counts, up to the battle's fade.
//  The far map's banner `banner` shows after the fade, so it ends the run
//  only when the fade went unseen (#268 slice 1).
WarpSeen run_into_door(VideoStream& stream, ProControllerContext& context, Direction direction, bool run, Milliseconds timeout, Milliseconds dark_from, const std::string& banner){
    Milliseconds left = timeout;
    for (size_t battles = 0;; battles++){
        const WallClock start = current_time();
        const DoorHoldEnd end = hold_into_door(stream, context, dpad_toward(direction), run, left, dark_from, banner);
        if (end.ret == 2){
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
            return {};
        }
        if (end.ret == 0){
            stream.log(std::format("[Story] run_into_door: {} detected ({}).", warp_fade_name(end.fade), end.how));
        }else if (end.ret == 1){
            stream.log("[Story] run_into_door: dialog detected.");
        }else if (end.ret == 3){
            stream.log("[Story] run_into_door: no fade seen, but the banner \"" + end.banner + "\" read.");
        }else if (end.went_dark != WallClock::max()){
            //  Dark only before dark_from: early, or the door's fade itself.
            stream.log(std::format("[Story] run_into_door: timed out; dark {} ms in, before dark_from {} ms.",
                std::chrono::duration_cast<Milliseconds>(end.went_dark - start).count(), dark_from.count()));
        }else{
            stream.log("[Story] run_into_door: timed out without detecting fade.");
        }
        return {.fade = end.fade, .banner = end.banner};
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
//  `fade` for the fade, each frame to `fade`'s watch (TAP_FADE_RULE), which
//  follows the taps from before the first. Logs each distinct banner read
//  and how the wait ended: the Oreburgh Gate's entry missed its banner twice
//  (lab-B, 2026-11-08 15:31 and 15:39) though a hand probe saw it 1.5 s
//  after the fade, and nothing said why.
TapLook look_after_tap(VideoStream& stream, ProControllerContext& context, const std::string& banner, Milliseconds wait, WarpFadeWatch* fade){
    const WallClock start = current_time();
    const WallClock deadline = start + wait;
    std::string last_read;
    LocationBannerReads reads;
    double darkest = 255;
    //  Every 500 ms of a long wait (the one after a fade) a frame goes to
    //  Screenshots/AutoStory/banner/, to see what the game showed.
    const std::string folder = SCREENSHOTS_PATH() + "AutoStory/banner/";
    WallClock next_frame = start;
    while (true){
        context.wait_for_all_requests();
        const VideoSnapshot screen = stream.video().snapshot();
        if (!fade && screen && current_time() >= next_frame){
            keep_banner_frame(*screen.frame, folder, banner);
            next_frame += 500ms;
        }
        const std::string read = screen ? reads.read(*screen.frame, current_time()) : "";
        log_new_banner_read(stream, read, last_read, banner, start);
        if (screen && location_banner_matches(read, banner)){
            return TapLook::BANNER;
        }
        darkest = std::min(darkest, brightness_of(screen));
        if (fade != nullptr && screen && fade->push(Pokemon::transition_sample(screen)) != WarpFade::NONE){
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

WarpSeen ConsoleStoryStepIO::run_into_door(Direction direction, bool run, Milliseconds timeout, Milliseconds dark_from, const std::string& banner){
    return PokemonBDSP::run_into_door(m_stream, m_context, direction, run, timeout, dark_from, banner);
}

//  A held walk can carry on past a fade that goes unseen: out of the Gym it
//  walked on south (r219g), and through the Oreburgh Gate's west mouth on
//  west into a pocket by Route 203's pond (r75). Taps during the fade are
//  eaten, so a fade ends the taps; the banner after it is evidence, not
//  required (#268 slice 1). The watch starts before the first tap: a walk
//  before that slid onto the warp unseen may have left its banner up
//  (lab-B 2026-11-08 15:31).
std::optional<size_t> ConsoleStoryStepIO::tap_to_banner(Direction direction, const std::string& map, size_t max_taps){
    const std::string& banner = story_banner_name(map);
    m_context.wait_for_all_requests();
    WarpFadeWatch watch(TAP_FADE_RULE);
    if (look_after_tap(m_stream, m_context, banner, 0ms, &watch) == TapLook::BANNER){
        m_stream.log("[Banner] \"" + banner + "\" shows before the first tap: through already.");
        return 0;
    }
    for (size_t tap = 1; tap <= max_taps; tap++){
        pbf_press_dpad(m_context, dpad_toward(direction), 160ms, 0ms);
        const TapLook look = look_after_tap(m_stream, m_context, banner, 1500ms, &watch);
        if (look == TapLook::DARK){
            m_stream.log("[Banner] the fade: " + watch.how() + ".");
        }
        if (look == TapLook::DARK && look_after_tap(m_stream, m_context, banner, 8000ms, nullptr) != TapLook::BANNER){
            m_stream.log("[Banner] a fade, no \"" + banner + "\" banner after it: counting the fade.", COLOR_ORANGE);
        }
        if (look != TapLook::NOTHING){
            //  No fixed wait: the step settles after it (talk_over()).
            return tap;
        }
    }
    return std::nullopt;
}

//  One rule for scenes and map changes (talk_over()): press through the
//  text, and keep pressing B while the screen is quiet, until the overworld
//  is quiet. A step or scene that ends on another map (AFTER_WARP) must fade on

//  the way, and only the quiet after the fade counts: r28 "went out" a tile
//  beside the door and walked a whole route in the house. The far map's
//  banner `banner`, if it has one, counts as the fade: a fade the run
//  missed is over, but its banner comes after it (#268 slice 1).
void ConsoleStoryStepIO::settle(Button button, const std::string& label, SettleKind kind, const std::string& banner){
    const TalkOverRule rule{.warp = kind == SettleKind::AFTER_WARP, .door = kind == SettleKind::DOOR};
    talk_over(m_stream, m_context, label, {.next = button, .rule = rule, .banner = banner});
}





} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
