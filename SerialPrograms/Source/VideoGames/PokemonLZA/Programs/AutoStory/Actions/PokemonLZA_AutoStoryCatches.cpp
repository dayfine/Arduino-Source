/*  PokemonLZA AutoStory - Catches
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cmath>
#include <format>
#include <string_view>
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Images/BinaryImage_FilterRgb32.h"
#include "CommonTools/OCR/OCR_NumberReader.h"
#include "Kernels/Waterfill/Kernels_Waterfill_Session.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonLZA/Inference/Battles/PokemonLZA_MoveEffectivenessSymbol.h"
#include "VideoGames/PokemonLZA/Inference/Battles/PokemonLZA_RunFromBattleDetector.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_DialogDetector.h"
#include "VideoGames/PokemonLZA/Programs/PokemonLZA_TrainerBattle.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryCaughtMarkReader.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryDexRegisteredDetector.h"
#include "PokemonLZA_AutoStoryCatchPlan.h"
#include "PokemonLZA_AutoStoryCatches.h"
#include "PokemonLZA_AutoStoryScenes.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;
using namespace Kernels::Waterfill;


namespace{

//  What advance_text_and_cards() pressed A through.
struct ScreensPassed{
    bool card = false;
    bool text = false;
};

//  A through text and tutorial cards until neither has shown for 3 s
//  (30 s at most).
ScreensPassed advance_text_and_cards(ConsoleHandle& console, ProControllerContext& context){
    FlatWhiteDialogDetector white_dialog;
    BlueDialogDetector blue_dialog;
    TealDialogDetector teal_dialog;
    ScreensPassed passed;
    WallClock last_busy = current_time();
    const WallClock deadline = last_busy + 30s;
    while (current_time() < deadline && current_time() - last_busy < 3s){
        context.wait_for_all_requests();
        const VideoSnapshot screen = console.video().snapshot();
        const bool card_now = screen && tutorial_card_shows(screen);
        if (card_now || white_dialog.detect(screen) || blue_dialog.detect(screen) || teal_dialog.detect(screen)){
            passed.card = passed.card || card_now;
            passed.text = passed.text || !card_now;
            last_busy = current_time();
            pbf_press_button(context, BUTTON_A, 160ms, 840ms);
        }else{
            context.wait_for(250ms);
        }
    }
    return passed;
}

//  ZL held 0.5 s, then ZL+ZR held 1.5 s throws a ball at the locked target
//  (0.5 s of ZR only aims), then the ball's flight, shakes and any loading
//  screen. ZL is never let go between the lock and the throw: a release
//  may drop the lock. Returns whether the Pokedex's "Registered!" banner
//  showed meanwhile: a new species caught. A repeat catch or a break-out
//  shows none.
bool throw_ball(ConsoleHandle& console, ProControllerContext& context){
    DetectorToFinder<DexRegisteredDetector> registered("DexRegisteredWatcher", 100ms);
    const int ret = run_until<ProControllerContext>(
        console, context,
        [](ProControllerContext& context){
            pbf_controller_state(context, BUTTON_ZL, DPAD_NONE, {0, 0}, {0, 0}, 500ms);
            pbf_controller_state(context, BUTTON_ZL | BUTTON_ZR, DPAD_NONE, {0, 0}, {0, 0}, 1500ms);
            pbf_wait(context, 6000ms);
        },
        {registered}
    );
    if (ret == 0){
        context.wait_for(3000ms);
    }
    context.wait_for_all_requests();
    return ret == 0;
}

//  ZL held a moment at what is in front: whether it locks on.
bool lock_on_ahead(ConsoleHandle& console, ProControllerContext& context){
    MoveEffectivenessSymbolWatcher lock_on(COLOR_RED, &console.overlay(), 100ms);
    const int ret = run_until<ProControllerContext>(
        console, context,
        [](ProControllerContext& context){
            pbf_mash_button(context, BUTTON_ZL, 2000ms);
        },
        {lock_on}
    );
    return ret == 0;
}

//  ZL at what is in front, then again after each of 3 small camera turns
//  right (the grass side): whether something locked on. No full turn, which
//  would lock on to whatever is behind, across the road.
bool lock_on_near(ConsoleHandle& console, ProControllerContext& context){
    for (int turn = 0; turn < 4; turn++){
        if (turn > 0){
            pbf_move_right_joystick(context, {0.5, 0}, 250ms, 200ms);
        }
        if (lock_on_ahead(console, context)){
            return true;
        }
    }
    return false;
}

//  Whether a wild battle is on: its Run prompt ("-" at the left), which
//  story battles do not show. A target a ball broke out of can turn on the
//  player and start one (Mareep in 03.08, console B 2026-11-13).
bool wild_battle_on(ConsoleHandle& console, ProControllerContext& context){
    context.wait_for_all_requests();
    const VideoSnapshot screen = console.video().snapshot();
    RunFromBattleDetector run_prompt(COLOR_RED, nullptr);
    return screen && run_prompt.detect(screen);
}

//  What tells weaken_then_throw() a throw caught its target.
enum class CatchSign{
    NO_TARGET,      //  only a later round with nothing to lock on (Bunnelby)
    REGISTERED,     //  the Pokedex's "Registered!" banner: a new species
    CARD,           //  a tutorial card after the throw (03.07's tutorial)
};

//  How weaken_then_throw() ended.
struct WeakenThrows{
    bool caught = false;
    int throws = 0;
};

//  TrainerBattleState attacks until 3 land (12 tries at most). Two lock-ons
//  in a row with no move symbols press D-pad up (twice at most): the
//  partner may not be out. Returns the attacks that landed.
int weaken_target(ProgramEnvironment& env, ConsoleHandle& console, ProControllerContext& context, TrainerBattleState& battle_state){
    int landed = 0;
    int misses = 0;
    int sends = 0;
    for (int tries = 0; tries < 12 && landed < 3; tries++){
        if (battle_state.attempt_one_attack(env, console, context)){
            landed++;
            misses = 0;
        }else if (++misses == 2 && sends < 2){
            console.log("weaken_target(): no lock-on twice: D-pad up to send the partner out.");
            pbf_press_dpad(context, DPAD_UP, 100ms, 2000ms);
            misses = 0;
            sends++;
        }
    }
    return landed;
}

//  Rounds of weaken_target() and a throw, with A through any lines, until
//  `sign` shows, a round after the first finds nothing to lock on (caught
//  for NO_TARGET, gone otherwise), or `max_throws` throws.
WeakenThrows weaken_then_throw(
    ProgramEnvironment& env, ConsoleHandle& console, ProControllerContext& context,
    BattleAIOption& battle_ai, CatchSign sign, int max_throws, std::string_view caller
){
    TrainerBattleState battle_state(battle_ai);
    WeakenThrows out;
    while (out.throws < max_throws){
        const int landed = weaken_target(env, console, context, battle_state);
        if (landed == 0 && out.throws > 0){
            console.log(std::format("{}: nothing to lock on in round {}: {}.", caller, out.throws + 1, sign == CatchSign::NO_TARGET ? "caught" : "gone"));
            out.caught = sign == CatchSign::NO_TARGET;
            return out;
        }
        advance_text_and_cards(console, context);
        const bool registered = throw_ball(console, context);
        out.throws++;
        const bool card = advance_text_and_cards(console, context).card;
        console.log(std::format("{}: round {}: {} attacks landed, then a throw.", caller, out.throws, landed));
        if ((sign == CatchSign::REGISTERED && registered) || (sign == CatchSign::CARD && card)){
            out.caught = true;
            return out;
        }
    }
    return out;
}

//  ZL held across reads of the screen: each hold is queued before the last
//  one ends, so ZL is never let go from the lock to the throw (throw_ball()
//  queues its own ZL first).
class HeldZl{
public:
    HeldZl(ConsoleHandle& console, ProControllerContext& context)
        : m_console(console)
        , m_context(context)
        , m_end(current_time())
    {}

    //  Queues `time` of ZL with the sticks at `left` and `right`.
    void hold(Milliseconds time, const JoystickPosition& left, const JoystickPosition& right){
        pbf_controller_state(m_context, BUTTON_ZL, DPAD_NONE, left, right, time);
        m_end = std::max(m_end, current_time()) + time;
    }

    //  Holds ZL still until 0.6 s after what was queued and reads the plate
    //  locked on to, with 0.4 s more of ZL queued.
    std::optional<LockTarget> read(){
        hold(1000ms, {0, 0}, {0, 0});
        const WallClock at = m_end - 400ms;
        const WallClock now = current_time();
        if (at > now){
            m_context.wait_for(std::chrono::duration_cast<Milliseconds>(at - now));
        }
        const VideoSnapshot frame = m_console.video().snapshot();
        if (!frame){
            return std::nullopt;
        }
        const std::optional<ImageFloatBox> bar = lock_target_plate_bar(frame);
        if (!bar){
            return std::nullopt;
        }
        LockTarget target;
        target.x = bar->x + bar->width / 2;
        target.y = bar->y + bar->height / 2;
        target.width = bar->width;
        target.caught = lock_target_caught(frame);
        return target;
    }

    //  The right stick flicked `x` (right when positive) with ZL held moves
    //  the lock to the next target that way (the user, 2026-11-13). Returns
    //  the plate after it.
    std::optional<LockTarget> flick(double x){
        hold(150ms, {0, 0}, {x, 0});
        return read();
    }

private:
    ConsoleHandle& m_console;
    ProControllerContext& m_context;
    WallClock m_end;
};

//  A full flick of the right stick: what moves the lock to the next target.
constexpr double TARGET_FLICK = 0.8;

std::string_view caught_text(const std::optional<bool>& caught){
    if (!caught){
        return "caught mark unread";
    }
    return *caught ? "caught" : "not caught";
}

void log_target(ConsoleHandle& console, std::string_view caller, std::string_view what, const LockTarget& target){
    console.log(std::format(
        "{}: {}: plate at ({:.2f}, {:.2f}), {:.3f} wide, {}.",
        caller, what, target.x, target.y, target.width, caught_text(target.caught)
    ));
}

//  What choose_target() found.
struct TargetChoice{
    //  The target locked on to now; nothing when no plate read.
    std::optional<LockTarget> target;
    //  Plates read, all of species already caught.
    bool all_caught = false;
};

//  With ZL held, flicks the right stick right up to 3 times, reading each
//  target's plate (LockTargetCycle), then flicks back to the nearest one
//  not caught: the closer one is much more likely to be caught (the user,
//  2026-11-13). The plate's height on the screen stands for the distance;
//  the cyan catch-chance icon over the target is not read.
TargetChoice choose_target(ConsoleHandle& console, HeldZl& held, std::string_view caller){
    LockTargetCycle cycle(4);
    std::optional<LockTarget> view = held.read();
    for (int flicks = 0; view && flicks < 4; flicks++){
        log_target(console, caller, std::format("target {}", cycle.targets().size()), *view);
        if (!cycle.add(*view)){
            break;
        }
        view = held.flick(TARGET_FLICK);
    }
    TargetChoice choice;
    const std::vector<LockTarget>& targets = cycle.targets();
    const std::optional<size_t> best = nearest_uncaught(targets);
    if (!best){
        choice.all_caught = !targets.empty();
        console.log(std::format("{}: {} plates read, none of a species not caught.", caller, targets.size()));
        return choice;
    }
    const int flicks = cycle.flicks_to(*best);
    console.log(std::format("{}: choosing target {} of {}: {} flicks {}.", caller, *best, targets.size(), std::abs(flicks), flicks < 0 ? "left" : "right"));
    std::optional<LockTarget> now = targets[*best];
    for (int i = 0; i < std::abs(flicks); i++){
        now = held.flick(flicks < 0 ? -TARGET_FLICK : TARGET_FLICK);
    }
    if (now && !same_lock_target(*now, targets[*best])){
        log_target(console, caller, "the lock moved on to another plate", *now);
    }
    choice.target = now ? now : targets[*best];
    return choice;
}

//  Walks toward a far target with ZL held (the lock keeps the camera on
//  it): 0.5 s steps, re-reading the plate, 4 at most (2 s).
void approach_target(ConsoleHandle& console, HeldZl& held, LockTarget target, std::string_view caller){
    int steps = 0;
    for (; steps < 4 && lock_target_far(target); steps++){
        held.hold(500ms, {0, 1}, {0, 0});
        const std::optional<LockTarget> view = held.read();
        if (!view){
            console.log(std::format("{}: the plate went after {} steps toward the target.", caller, steps + 1));
            return;
        }
        target = *view;
    }
    log_target(console, caller, std::format("after {} steps toward the target", steps), target);
}

//  ZL held: chooses the nearest target not caught (choose_target()) and
//  walks toward it when it is far, then leaves ZL held for throw_ball().
TargetChoice aim_choose_approach(ConsoleHandle& console, ProControllerContext& context, std::string_view caller){
    HeldZl held(console, context);
    const TargetChoice choice = choose_target(console, held, caller);
    if (choice.target){
        approach_target(console, held, *choice.target, caller);
    }
    return choice;
}

//  Turns the camera, without walking, until the objective marker is in the
//  middle (15 turns at most).
void face_objective(ConsoleHandle& console, ProControllerContext& context){
    for (int i = 0; i < 15; i++){
        context.wait_for_all_requests();
        const std::optional<ObjectiveMarker> marker = read_objective_marker(console, console.video().snapshot());
        const double off = marker ? marker->x - 0.5 : 0.5;
        if (off > -0.06 && off < 0.06){
            return;
        }
        pbf_move_right_joystick(context, {off > 0 ? 0.4 : -0.4, 0}, Milliseconds((int)(std::min(1.0, std::abs(off) * 4) * 400)) + 80ms, 150ms);
    }
}

//  What catch_at_stop() did: new species registered, throws, and locks on
//  that found only species already caught.
struct GrassCatches{
    int found = 0;
    int throws = 0;
    int caught_seen = 0;

    GrassCatches& operator+=(const GrassCatches& other){
        found += other.found;
        throws += other.throws;
        caught_seen += other.caught_seen;
        return *this;
    }
};

//  A throw at the target locked on to, then weaken_then_throw() when it
//  broke out into a wild battle, `max_throws` in all at most.
GrassCatches throw_at_target(
    ProgramEnvironment& env, ConsoleHandle& console, ProControllerContext& context,
    BattleAIOption& battle_ai, int max_throws
){
    GrassCatches out;
    out.throws = 1;
    out.found = throw_ball(console, context) ? 1 : 0;
    console.log(std::format("catch_in_zone(): throw: {}.", out.found > 0 ? "a new species registered" : "no new species"));
    advance_text_and_cards(console, context);
    if (out.found == 0 && max_throws > 1 && wild_battle_on(console, context)){
        console.log("catch_in_zone(): the throw broke out: battle, weakening.");
        const WeakenThrows weakened = weaken_then_throw(
            env, console, context, battle_ai, CatchSign::REGISTERED, max_throws - 1, "catch_in_zone()"
        );
        out.throws += weakened.throws;
        out.found += weakened.caught ? 1 : 0;
    }
    return out;
}

//  At one stop: locks on (lock_on_near()), chooses the nearest target not
//  caught and walks toward it (aim_choose_approach()), and throws, 4 locks
//  at most, until `new_species_wanted` new species or `max_throws`. When
//  nothing locks on from where it stands, steps right into the grass 1 s,
//  once, and back at the end. All species caught: turns the camera right
//  a little and locks on again.
GrassCatches catch_at_stop(
    ProgramEnvironment& env, ConsoleHandle& console, ProControllerContext& context,
    BattleAIOption& battle_ai, int new_species_wanted, int max_throws
){
    GrassCatches out;
    bool stepped = false;
    for (int locks = 0; locks < 4 && out.found < new_species_wanted && out.throws < max_throws; locks++){
        if (!lock_on_near(console, context)){
            if (stepped){
                console.log("catch_in_zone(): nothing locks on in the grass either.");
                break;
            }
            console.log("catch_in_zone(): nothing locks on here: a step right into the grass.");
            pbf_move_left_joystick(context, {1, 0}, 1000ms, 300ms);
            stepped = true;
            continue;
        }
        const TargetChoice choice = aim_choose_approach(console, context, "catch_in_zone()");
        if (choice.all_caught){
            console.log("catch_in_zone(): only species already caught lock on: looking further right.");
            out.caught_seen++;
            pbf_move_right_joystick(context, {0.5, 0}, 400ms, 300ms);
            continue;
        }
        out += throw_at_target(env, console, context, battle_ai, max_throws - out.throws);
    }
    if (stepped){
        pbf_move_left_joystick(context, {-1, 0}, 1000ms, 300ms);
    }
    return out;
}

//  Off screen, the game pins the marker to the edge with no digits
//  (anything read there is noise): turns the camera to it. Returns whether
//  the marker at `x` (0 to 1) was pinned.
bool turn_to_pinned_marker(ProControllerContext& context, double x){
    if (x >= 0.10 && x <= 0.90){
        return false;
    }
    pbf_move_right_joystick(context, {x < 0.5 ? -0.5 : 0.5, 0}, 400ms, 200ms);
    return true;
}

//  Turns the camera toward the marker at `x` (0 to 1) and walks 0.6 s on.
void steer_and_step(ProControllerContext& context, double x){
    const double off = x - 0.5;
    if (off > 0.06 || off < -0.06){
        pbf_move_right_joystick(context, {off > 0 ? 0.4 : -0.4, 0}, Milliseconds((int)(std::min(1.0, std::abs(off) * 4) * 400)) + 80ms, 150ms);
    }
    pbf_move_left_joystick(context, {0, 1}, 600ms, 0ms);
}

//  Whether a walk toward the marker is held up: the distance has not
//  dropped below its best for 4 reads (a fence or a wall in the way, the
//  Research Lab's front fence on console B, 2026-11-10).
class StuckWatch{
public:
    bool stuck(int distance){
        if (distance < m_best){
            m_best = distance;
            m_still = 0;
            return false;
        }
        if (++m_still < 4){
            return false;
        }
        m_still = 0;
        return true;
    }
    //  Steps sideways around what is in the way: right, then left twice as
    //  far, and so on (4 at most).
    void sidestep(ConsoleHandle& console, ProControllerContext& context){
        if (m_sidesteps >= 4){
            return;
        }
        const bool right = m_sidesteps % 2 == 0;
        const Milliseconds time = Milliseconds(1500 * (m_sidesteps + 1));
        console.log(std::format("walk_to_objective(): held up at {}: {} for {} ms.", m_best, right ? "right" : "left", time.count()));
        pbf_move_left_joystick(context, {right ? 1.0 : -1.0, 0}, time, 200ms);
        m_sidesteps++;
    }

private:
    int m_best = 1 << 30;
    int m_still = 0;
    int m_sidesteps = 0;
};

//  Walks back away from the objective marker (the stick down: toward the
//  camera, which stays on the marker) until its distance reads `stop_at`
//  or more, or `limit`. A read more than 12 from the last is a misread, as
//  in walk_to_objective(). Returns the last distance read.
std::optional<int> walk_from_objective(ConsoleHandle& console, ProControllerContext& context, int stop_at, Milliseconds limit){
    const WallClock deadline = current_time() + limit;
    std::optional<int> last;
    while (current_time() < deadline){
        context.wait_for_all_requests();
        const std::optional<ObjectiveMarker> marker = read_objective_marker(console, console.video().snapshot());
        if (marker && turn_to_pinned_marker(context, marker->x)){
            continue;
        }
        if (marker && marker->distance > 0 && (!last || std::abs(marker->distance - *last) <= 12)){
            last = marker->distance;
            if (*last >= stop_at){
                console.log(std::format("walk_from_objective(): {} from the objective.", *last));
                return last;
            }
        }
        pbf_move_left_joystick(context, {0, -1}, 600ms, 0ms);
    }
    console.log(std::format("walk_from_objective(): not back to {} in time (last read {}).", stop_at, last.value_or(-1)));
    return last;
}

} // namespace


std::optional<ObjectiveMarker> read_objective_marker(Logger& logger, const ImageViewRGB32& screen){
    //  The hexagon's orange, about (245, 175, 40) on console B: the largest
    //  hexagon-sized blob outside the minimap (its marker is there too).
    const double scale = screen.height() / 1080.0;
    PackedBinaryMatrix matrix = compress_rgb32_to_binary_range(screen, 220, 255, 140, 215, 0, 90);
    std::unique_ptr<WaterfillSession> session = make_WaterfillSession(matrix);
    std::unique_ptr<WaterfillIterator> iter = session->make_iterator((size_t)(500 * scale * scale));
    std::optional<WaterfillObject> best;
    WaterfillObject object;
    while (iter->find_next(object, false)){
        //  The minimap: a circle of radius 125 around (180, 180) on
        //  1920x1080. A box around it also covered the real marker when it
        //  sat at the left edge, high up (console B, 2026-11-10).
        const double dx = object.center_of_gravity_x() / scale - 180;
        const double dy = object.center_of_gravity_y() / scale - 180;
        const bool in_minimap = dx * dx + dy * dy < 140 * 140;
        const bool in_party_bar = object.center_of_gravity_y() / screen.height() > 0.70;
        const double aspect = object.aspect_ratio();
        if (in_minimap || in_party_bar || object.area > 4000 * scale * scale || aspect < 0.6 || aspect > 1.4){
            continue;
        }
        if (!best || object.area > best->area){
            best = object;
        }
    }
    if (!best){
        return std::nullopt;
    }
    //  The distance: white digits just above the hexagon, up to its width
    //  out each side, about half its height tall.
    const double w = (double)best->width();
    const double h = (double)best->height();
    const ImagePixelBox digits(
        (size_t)std::max(0.0, best->min_x - w), (size_t)std::max(0.0, best->min_y - 0.95 * h),
        (size_t)std::min((double)screen.width(), best->max_x + w), (size_t)std::max(1.0, best->min_y - 0.10 * h)
    );
    ObjectiveMarker marker;
    marker.x = (best->min_x + best->max_x) / 2.0 / screen.width();
    marker.distance = OCR::read_number_waterfill(logger, extract_box_reference(screen, digits), 0xffd0d0d0, 0xffffffff);
    return marker;
}

std::optional<int> walk_to_objective(ConsoleHandle& console, ProControllerContext& context, int stop_at, Milliseconds limit){
    PlzaScreenObserver observer;
    const WallClock deadline = current_time() + limit;
    int unseen = 0;
    double last_x = 0.5;
    std::optional<int> last;
    //  Reads far from `last` in a row that agree with each other.
    std::optional<int> rejected_read;
    int rejected = 0;
    StuckWatch stuck;
    while (current_time() < deadline){
        context.wait_for_all_requests();
        const VideoSnapshot frame = console.video().snapshot();
        const ScreenEvents events = observer.observe(frame, current_time());
        if (stop_at <= 0 && events.any_of({ScreenEvent::INTERACT_PROMPT, ScreenEvent::DIALOG_BOX})){
            return last;
        }
        const std::optional<ObjectiveMarker> marker = read_objective_marker(console, frame);
        if (!marker){
            //  Behind us: turn the camera, toward where it was last, until it
            //  shows.
            if (++unseen > 12){
                return std::nullopt;
            }
            pbf_move_right_joystick(context, {last_x < 0.5 ? -0.5 : 0.5, 0}, 500ms, 200ms);
            continue;
        }
        unseen = 0;
        last_x = marker->x;
        if (turn_to_pinned_marker(context, marker->x)){
            continue;
        }
        //  A read far from the last one is a misread: a 0.6 s step covers
        //  about 3. Below: 36 as "3" or 29 on console B; above: 110 as
        //  "1110" (2026-11-13), which then rejected every true read after
        //  it. So three agreeing reads in a row far from the last mean the
        //  last read was the wrong one, and they are taken.
        const auto near = [](int a, int b){ return a >= b - 12 && a <= b + 12; };
        if (marker->distance > 0 && last && !near(marker->distance, *last)){
            rejected = rejected_read && near(marker->distance, *rejected_read) ? rejected + 1 : 1;
            rejected_read = marker->distance;
            if (rejected < 3){
                steer_and_step(context, marker->x);
                continue;
            }
        }
        if (marker->distance > 0){
            rejected_read.reset();
            rejected = 0;
            if (last != marker->distance){
                console.log(std::format("walk_to_objective(): {} to go, marker at x = {:.2f}.", marker->distance, marker->x));
            }
            last = marker->distance;
            if (marker->distance <= stop_at){
                console.log(std::format("walk_to_objective(): {} to the objective.", marker->distance));
                return last;
            }
            if (stuck.stuck(marker->distance)){
                stuck.sidestep(console, context);
            }
        }
        steer_and_step(context, marker->x);
    }
    console.log(std::format("walk_to_objective(): not within {} in time (last read {}).", stop_at, last.value_or(-1)));
    return last;
}

namespace{

//  Other species than Mareep stand at other distances from the far gate
//  (the user, 2026-11-13): catch_at_stop() at stops back to 100 and
//  forward to 60 again, 2 laps, 8 minutes at most.
GrassCatches patrol_and_catch(
    ProgramEnvironment& env, ConsoleHandle& console, ProControllerContext& context,
    BattleAIOption& battle_ai, int new_species_wanted, int max_throws
){
    const std::vector<int> stops = patrol_stops(60, 100, 20, 2);
    const WallClock deadline = current_time() + 8min;
    GrassCatches result;
    for (size_t i = 0; i < stops.size() && result.found < new_species_wanted && result.throws < max_throws; i++){
        if (current_time() > deadline){
            console.log("catch_in_zone(): out of time for the patrol.");
            break;
        }
        std::optional<int> at = stops[i];
        if (i > 0){
            at = stops[i] > stops[i - 1]
                ? walk_from_objective(console, context, stops[i], 15s)
                : walk_to_objective(console, context, stops[i], 15s);
        }
        face_objective(console, context);
        const GrassCatches stop = catch_at_stop(
            env, console, context, battle_ai, new_species_wanted - result.found, max_throws - result.throws
        );
        console.log(std::format(
            "catch_in_zone(): stop {} of {} at {} (for {}): {} throws, {} new species, {} locks on caught species only.",
            i + 1, stops.size(), at.value_or(-1), stops[i], stop.throws, stop.found, stop.caught_seen
        ));
        result += stop;
    }
    return result;
}

} // namespace


void catch_in_zone(
    ProgramEnvironment& env, ConsoleHandle& console, ProControllerContext& context,
    BattleAIOption& battle_ai, int new_species_wanted, int max_throws
){
    //  Taunie's two rocks stand between the start and the marker: a step
    //  right first, around them, from the start (a catch's autosave can
    //  leave the save further on).
    context.wait_for_all_requests();
    const std::optional<ObjectiveMarker> start = read_objective_marker(console, console.video().snapshot());
    if (start && start->distance >= 115){
        pbf_move_left_joystick(context, {1, 0}, 1000ms, 300ms);
    }
    //  Mareep, the species still missing after the tutorial, graze in the
    //  grass right of the road at 55-60 from the far gate (console B,
    //  2026-11-10).
    walk_to_objective(console, context, 60, 90s);
    const GrassCatches result = patrol_and_catch(env, console, context, battle_ai, new_species_wanted, max_throws);
    console.log(std::format("catch_in_zone(): {} new species in {} throws.", result.found, result.throws));
    //  A rerun from a save after the catch: only species already caught
    //  lock on along the road, so the quest is done.
    if (result.found == 0 && result.throws == 0 && result.caught_seen > 0){
        console.log("catch_in_zone(): only species already caught here: taking the quest as done.");
        return;
    }
    if (result.found < new_species_wanted){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            std::format("catch_in_zone(): {} new species of {} in {} throws.", result.found, new_species_wanted, result.throws),
            console
        );
    }
}

void walk_to_marker(ConsoleHandle& console, ProControllerContext& context){
    walk_to_objective(console, context, 0, 90s);
    PlzaScreenObserver observer;
    context.wait_for_all_requests();
    const ScreenEvents events = observer.observe(console.video().snapshot(), current_time());
    if (!events.any_of({ScreenEvent::INTERACT_PROMPT, ScreenEvent::DIALOG_BOX})){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "walk_to_marker(): no A prompt or dialog at the objective.",
            console
        );
    }
}

void catch_tutorial(
    ProgramEnvironment& env, ConsoleHandle& console, ProControllerContext& context,
    BattleAIOption& battle_ai, int max_throws
){
    int throws = 0;
    while (throws < max_throws){
        //  Fletchling, the tutorial's target, is never caught yet: the
        //  throw goes at whatever the choice locked on to.
        aim_choose_approach(console, context, "catch_tutorial()");
        throw_ball(console, context);
        throws++;
        if (advance_text_and_cards(console, context).card){
            console.log(std::format("catch_tutorial(): caught on throw {}.", throws));
            return;
        }
        if (throws < max_throws && wild_battle_on(console, context)){
            console.log(std::format("catch_tutorial(): throw {} broke out: battle, weakening.", throws));
            const WeakenThrows weakened = weaken_then_throw(
                env, console, context, battle_ai, CatchSign::CARD, max_throws - throws, "catch_tutorial()"
            );
            throws += weakened.throws;
            if (weakened.caught){
                console.log(std::format("catch_tutorial(): caught on throw {}.", throws));
                return;
            }
        }
    }
    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::SEND_ERROR_REPORT,
        std::format("catch_tutorial(): no catch in {} throws.", max_throws),
        console
    );
}

void battle_then_catch(
    ProgramEnvironment& env, ConsoleHandle& console, ProControllerContext& context,
    BattleAIOption& battle_ai, int max_rounds
){
    //  D-pad up is ignored while Bunnelby's "weaken it" banner shows.
    pbf_press_button(context, BUTTON_A, 160ms, 840ms);
    advance_text_and_cards(console, context);
    pbf_press_dpad(context, DPAD_UP, 100ms, 2000ms);
    if (weaken_then_throw(env, console, context, battle_ai, CatchSign::NO_TARGET, max_rounds, "battle_then_catch()").caught){
        return;
    }
    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::SEND_ERROR_REPORT,
        std::format("battle_then_catch(): still a target after {} rounds.", max_rounds),
        console
    );
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
