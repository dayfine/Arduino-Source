/*  PokemonLZA AutoStory - Catches
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cmath>
#include <format>
#include <chrono>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/VisualDetector.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonLZA/Inference/Battles/PokemonLZA_RunFromBattleDetector.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_AlertEyeDetector.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_PartyBarDetector.h"
#include "VideoGames/PokemonLZA/Programs/PokemonLZA_BallThrow.h"
#include "VideoGames/PokemonLZA/Programs/PokemonLZA_LockOnSweep.h"
#include "VideoGames/PokemonLZA/Programs/PokemonLZA_TrainerBattle.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryCaughtMarkReader.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryDexRegisteredDetector.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryObjectiveMarkerReader.h"
#include "PokemonLZA_AutoStoryCatchPlan.h"
#include "PokemonLZA_AutoStoryCatchStop.h"
#include "PokemonLZA_AutoStoryLines.h"
#include "PokemonLZA_AutoStoryMapWalk.h"
#include "PokemonLZA_AutoStoryMarkerWalk.h"
#include "PokemonLZA_AutoStorySpeciesCheck.h"
#include "PokemonLZA_AutoStoryZone1Map.h"
#include "PokemonLZA_AutoStoryZone1Road.h"
#include "PokemonLZA_AutoStoryZoneStart.h"
#include "PokemonLZA_AutoStoryCatches.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;


namespace{

//  The throw that passed 03.07: the banner shows ~7 s after it (03.08).
const BallThrowOptions STORY_THROW{
    .lock_on = Milliseconds(500),
    .aim = Milliseconds(1500),
    .after_throw = Milliseconds(8000),
};

//  The lines after a throw end once none has shown for LINES_QUIET.
constexpr Milliseconds LINES_QUIET(3000);
constexpr Milliseconds LINES_LIMIT(30000);

//  Whether a wild battle is on: its Run prompt, or the red alert bar with
//  the eye at the top while a wild Pokemon attacks the player before the
//  partner is out (03.08).
bool wild_battle_on(StoryEnv& env){
    env.context.wait_for_all_requests();
    const VideoSnapshot screen = env.env.console.video().snapshot();
    if (!screen){
        return false;
    }
    RunFromBattleDetector run_prompt(COLOR_RED, nullptr);
    AlertEyeDetector alert(COLOR_RED, nullptr);
    return run_prompt.detect(screen) || alert.detect(screen);
}

//  Attacks until 3 land (12 tries at most). Two lock-ons in a row with no
//  move symbols press D-pad up (twice at most): the partner may not be
//  out. Returns the attacks that landed.
int weaken_target(StoryEnv& env, PlzaBackend& backend, TrainerBattleState& battle_state){
    int landed = 0;
    int misses = 0;
    int sends = 0;
    for (int tries = 0; tries < 12 && landed < 3; tries++){
        if (battle_state.attempt_one_attack(env.env, env.env.console, env.context)){
            landed++;
            misses = 0;
        }else if (++misses == 2 && sends < 2){
            backend.log("[PLZA] weaken_target: no lock-on twice: D-pad up to send the partner out");
            backend.press(BUTTON_UP, /*hold=*/100ms, /*release=*/2000ms);
            misses = 0;
            sends++;
        }
    }
    return landed;
}

} // namespace


Pokemon::Outcome catch_tutorial(StoryEnv& env, PlzaBackend& backend, int max_throws){
    TrainerBattleState battle_state(env.battle_ai);
    for (int throws = 1; throws <= max_throws; throws++){
        throw_ball(env.context, STORY_THROW);
        if (press_a_through_lines(backend, LINES_QUIET, LINES_LIMIT).card){
            backend.log(std::format("[PLZA] catch_tutorial: caught on throw {}", throws));
            return Pokemon::outcome_ok();
        }
        if (throws < max_throws && wild_battle_on(env)){
            const int landed = weaken_target(env, backend, battle_state);
            backend.log(std::format("[PLZA] catch_tutorial: throw {} broke out into a battle: {} attacks landed", throws, landed));
            press_a_through_lines(backend, LINES_QUIET, LINES_LIMIT);
        }
    }
    return Pokemon::outcome_failed("no_catch", std::format("no tutorial card after {} throws", max_throws));
}

Pokemon::Outcome battle_then_catch(StoryEnv& env, PlzaBackend& backend, int max_rounds){
    backend.press(BUTTON_A, /*hold=*/160ms, /*release=*/840ms);
    press_a_through_lines(backend, LINES_QUIET, LINES_LIMIT);
    backend.press(BUTTON_UP, /*hold=*/100ms, /*release=*/2000ms);
    TrainerBattleState battle_state(env.battle_ai);
    for (int round = 0; round < max_rounds; round++){
        const int landed = weaken_target(env, backend, battle_state);
        if (landed == 0 && round > 0){
            backend.log(std::format("[PLZA] battle_then_catch: nothing to lock on in round {}: caught", round + 1));
            return Pokemon::outcome_ok();
        }
        press_a_through_lines(backend, LINES_QUIET, LINES_LIMIT);
        throw_ball(env.context, STORY_THROW);
        press_a_through_lines(backend, LINES_QUIET, LINES_LIMIT);
        backend.log(std::format("[PLZA] battle_then_catch: round {}: {} attacks landed, then a throw", round + 1, landed));
    }
    return Pokemon::outcome_failed("still_a_target", std::format("a target still locks on after {} rounds", max_rounds));
}


namespace{

//  Walks toward the objective never go below this, read or not: 60 is
//  just before the far gate, and past it is the city.
constexpr int ROAD_FLOOR = CATCH_SPOTS[std::size(CATCH_SPOTS) - 1] - ROAD_STOP_SLACK;

//  The patrol's time in the zone and its visits to CATCH_SPOTS (5 rounds).
constexpr Milliseconds PATROL_LIMIT(600000);
constexpr size_t PATROL_VISITS = 10;

//  How long ZL is held for the lock to show before a stop gives up on the
//  target: over 1.5 s at times.
constexpr Milliseconds LOCK_ON_WAIT(3000);
//  A full flick of the right stick with ZL held moves the lock to the next
//  target that way.
constexpr double TARGET_FLICK = 0.8;

//  A catch stop's lock-on sweep (sweep_for_lock_on()), upstream's
//  InPlaceCatcher's: rounds of the right stick at SWEEP_STICK_X for 1 s,
//  D-pad up (the partner out), ZL mashed 1 s and held 0.5 s.
//  Right, toward the grass right of the road, where the user's catches
//  were (Mareep) besides the road itself (Scatterbug).
constexpr double SWEEP_STICK_X = 0.5;
constexpr int SWEEP_ROUNDS = 8;


//  A ZL lock-on and throw leave the camera looking steeply down: the
//  right stick down held 1500 ms pins the pitch at its lowest, then up
//  600 ms gives the walking pitch.
void level_camera(StoryEnv& env){
    pbf_move_right_joystick(env.context, /*position=*/{0, -1}, /*hold=*/Milliseconds(1500), /*release=*/Milliseconds(100));
    pbf_move_right_joystick(env.context, /*position=*/{0, 1}, /*hold=*/Milliseconds(600), /*release=*/Milliseconds(300));
}

//  The story's throw (STORY_THROW) with the Pokedex's "Registered!" banner
//  (a new species) watched for over it and REGISTERED_BANNER_TAIL after.
//  Returns whether it showed; then waits 3 s for it to go.
bool throw_ball_watched(StoryEnv& env, PlzaBackend& backend){
    ConsoleHandle& console = env.env.console;
    DetectorToFinder<DexRegisteredDetector> registered("DexRegisteredWatcher", REGISTERED_BANNER_HOLD);
    int fired = run_until<ProControllerContext>(
        console, env.context,
        [](ProControllerContext& context){
            throw_ball(context, STORY_THROW);
        },
        {registered}
    );
    if (fired < 0){
        fired = wait_until(console, env.context, REGISTERED_BANNER_TAIL, {registered});
    }
    const bool seen = fired >= 0;
    backend.log(std::format("[PLZA] catch_in_zone: the Registered! banner: {}", seen ? "seen" : "not seen"));
    if (seen){
        env.context.wait_for(Milliseconds(3000));
    }
    env.context.wait_for_all_requests();
    return seen;
}

//  How weaken_then_throw() ended.
struct WeakenThrows{
    bool caught = false;
    int throws = 0;
};

//  Rounds of weaken_target() and a throw, with A through the lines, until
//  the "Registered!" banner shows, a round after the first finds nothing
//  to lock on (the target gone), or `max_throws` throws.
WeakenThrows weaken_then_throw(StoryEnv& env, PlzaBackend& backend, int max_throws){
    TrainerBattleState battle_state(env.battle_ai);
    WeakenThrows out;
    while (out.throws < max_throws){
        const int landed = weaken_target(env, backend, battle_state);
        if (landed == 0 && out.throws > 0){
            backend.log(std::format("[PLZA] catch_in_zone: nothing to lock on in round {}: gone", out.throws + 1));
            return out;
        }
        press_a_through_lines(backend, LINES_QUIET, LINES_LIMIT);
        const bool registered = throw_ball_watched(env, backend);
        out.throws++;
        press_a_through_lines(backend, LINES_QUIET, LINES_LIMIT);
        backend.log(std::format("[PLZA] catch_in_zone: round {}: {} attacks landed, then a throw", out.throws, landed));
        if (registered){
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
    explicit HeldZl(StoryEnv& env)
        : m_env(env)
        , m_end(current_time())
    {}

    //  Queues `time` of ZL with the right stick at `right_x`, the left at
    //  `left_y` (forward when positive).
    void hold(Milliseconds time, double right_x, double left_y){
        pbf_controller_state(
            m_env.context, BUTTON_ZL, DPAD_NONE,
            /*left_joystick=*/{0, left_y}, /*right_joystick=*/{right_x, 0}, time
        );
        m_end = std::max(m_end, current_time()) + time;
    }

    //  Holds ZL still until 0.6 s after what was queued and reads the plate
    //  locked on to, with 0.4 s more of ZL queued.
    std::optional<LockTarget> read(){
        hold(Milliseconds(1000), /*right_x=*/0, /*left_y=*/0);
        const WallClock at = m_end - Milliseconds(400);
        const WallClock now = current_time();
        if (at > now){
            m_env.context.wait_for(std::chrono::duration_cast<Milliseconds>(at - now));
        }
        const VideoSnapshot frame = m_env.env.console.video().snapshot();
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

    //  read() with ZL held on until a plate shows or `limit` passes.
    std::optional<LockTarget> wait_for_plate(Milliseconds limit){
        const WallClock end = current_time() + limit;
        while (true){
            const std::optional<LockTarget> target = read();
            if (target || current_time() >= end){
                return target;
            }
        }
    }

    //  The right stick flicked `x` (right when positive) with ZL held moves
    //  the lock to the next target that way. Returns the plate after it.
    std::optional<LockTarget> flick(double x){
        hold(Milliseconds(150), x, /*left_y=*/0);
        return read();
    }

private:
    StoryEnv& m_env;
    WallClock m_end;
};

void log_target(PlzaBackend& backend, std::string_view what, const LockTarget& target){
    backend.log(std::format(
        "[PLZA] catch_in_zone: {}: plate at ({:.2f}, {:.2f}), {:.3f} wide, {}",
        what, target.x, target.y, target.width, target.caught_text()
    ));
}

//  What choose_target() found.
struct TargetChoice{
    //  The target locked on to now; nothing when no plate read.
    std::optional<LockTarget> target;
    //  The plates read, the nearest first.
    std::vector<LockTarget> plates;
};

//  With ZL held until the lock shows (LOCK_ON_WAIT at most), flicks the
//  right stick right up to 3 times, reading each target's plate
//  (LockTargetCycle), then flicks back to the nearest one not caught: the
//  closer one is much more likely to be caught.
TargetChoice choose_target(PlzaBackend& backend, HeldZl& held){
    LockTargetCycle cycle(4);
    std::optional<LockTarget> view = held.wait_for_plate(LOCK_ON_WAIT);
    if (!view){
        backend.log(std::format("[PLZA] catch_in_zone: ZL held {} ms, but no name plate: no lock on", LOCK_ON_WAIT.count()));
    }
    for (int flicks = 0; view && flicks < 4; flicks++){
        log_target(backend, std::format("target {}", cycle.targets().size()), *view);
        if (!cycle.add(*view)){
            break;
        }
        view = held.flick(TARGET_FLICK);
    }
    TargetChoice choice;
    choice.plates = cycle.targets();
    const std::vector<LockTarget>& targets = choice.plates;
    const std::optional<size_t> best = nearest_uncaught(targets);
    if (!best){
        backend.log(std::format("[PLZA] catch_in_zone: {} plates read, none of a species not caught", targets.size()));
        return choice;
    }
    const int flicks = cycle.flicks_to(*best);
    backend.log(std::format("[PLZA] catch_in_zone: choosing target {} of {}: {} flicks {}", *best, targets.size(), std::abs(flicks), flicks < 0 ? "left" : "right"));
    std::optional<LockTarget> now = targets[*best];
    for (int i = 0; i < std::abs(flicks); i++){
        now = held.flick(flicks < 0 ? -TARGET_FLICK : TARGET_FLICK);
    }
    if (now && !same_lock_target(*now, targets[*best])){
        log_target(backend, "the lock moved on to another plate", *now);
    }
    choice.target = now ? now : targets[*best];
    return choice;
}

//  Steps toward `target` with ZL held (the lock keeps the camera on it),
//  0.5 s at a time and re-reading the plate, while step_toward_target()
//  says so after `misses` throws missed at this lock. Returns the plate
//  last read: the one to throw at.
LockTarget approach_target(LockTarget target, int misses, PlzaBackend& backend, HeldZl& held){
    int steps = 0;
    for (; step_toward_target(target, misses, steps); steps++){
        held.hold(Milliseconds(500), /*right_x=*/0, /*left_y=*/1);
        const std::optional<LockTarget> view = held.read();
        if (!view){
            backend.log(std::format("[PLZA] catch_in_zone: the plate went after {} steps toward the target", steps + 1));
            return target;
        }
        target = *view;
    }
    log_target(backend, std::format("after {} steps toward the target", steps), target);
    return target;
}

//  ZL held: chooses the nearest target not caught (choose_target()) and
//  steps toward it when it is far, then leaves ZL held for the throw.
TargetChoice aim_choose_approach(StoryEnv& env, PlzaBackend& backend){
    HeldZl held(env);
    TargetChoice choice = choose_target(backend, held);
    if (choice.target){
        choice.target = approach_target(*choice.target, /*misses=*/0, backend, held);
    }
    return choice;
}

//  What throw_at_target() did.
struct TargetThrows{
    GrassCatches catches;
    //  THROWS_AT_ONE_TARGET missed at one lock: the stop ends.
    bool move_on = false;
};

//  Throws at `target` (`max_throws` at most), then as after_throw() says:
//  after a miss, at a plate that locks on again with ZL held, steps toward
//  it while it reads far (approach_target()) and throws again; after
//  THROWS_AT_ONE_TARGET misses, moves on; weaken_then_throw() when it broke
//  out into a wild battle. Each throw is logged with its plate's height,
//  the distance signal, to calibrate step_toward_target().
TargetThrows throw_at_target(StoryEnv& env, PlzaBackend& backend, const LockTarget& target, int max_throws){
    TargetThrows out;
    GrassCatches& catches = out.catches;
    LockTarget aimed = target;
    while (catches.throws < max_throws){
        const bool registered = throw_ball_watched(env, backend);
        catches.throws++;
        press_a_through_lines(backend, LINES_QUIET, LINES_LIMIT);
        const bool battle = !registered && wild_battle_on(env);
        std::optional<HeldZl> held;
        std::optional<LockTarget> relocked;
        if (!registered && !battle){
            held.emplace(env);
            relocked = held->wait_for_plate(LOCK_ON_WAIT);
        }
        const ThrowSeen seen{registered, aimed.caught, !registered && !battle && !relocked};
        const std::optional<NewSpeciesSign> sign = seen.new_species();
        catches.found = sign ? 1 : 0;
        catches.found_by_plate = sign == NewSpeciesSign::PLATE_NOT_CAUGHT ? 1 : 0;
        backend.log(std::format(
            "[PLZA] catch_in_zone: throw {} at plate y {:.2f}: {} ({})",
            catches.throws, aimed.y, sign ? "hit" : "miss", seen.new_species_text()
        ));
        const AfterThrow next = after_throw(seen, battle, relocked, catches.throws);
        if (next == AfterThrow::THROW_AGAIN){
            log_target(backend, "missed, locked on again", *relocked);
            aimed = approach_target(*relocked, catches.throws, backend, *held);
            continue;
        }
        if (next == AfterThrow::MOVE_ON){
            log_target(backend, std::format("{} misses at one lock: on to the next stop", catches.throws), *relocked);
            out.move_on = true;
            return out;
        }
        if (next == AfterThrow::WEAKEN && catches.throws < max_throws){
            backend.log("[PLZA] catch_in_zone: the throw broke out: battle, weakening");
            const WeakenThrows weakened = weaken_then_throw(env, backend, max_throws - catches.throws);
            catches.throws += weakened.throws;
            catches.found += weakened.caught ? 1 : 0;
        }
        return out;
    }
    return out;
}

//  Upstream's lock-on sweep (lock_on_by_sweep(), InPlaceCatcher's):
//  SWEEP_ROUNDS rounds turning the camera right, until the battle menu's
//  move symbols show: ZL has locked on to a wild Pokemon. Returns whether
//  they showed; when not, the sweep's L put the camera back behind the
//  player.
bool sweep_for_lock_on(StoryEnv& env, PlzaBackend& backend, int at){
    env.context.wait_for_all_requests();
    const WallClock start = current_time();
    const bool locked = lock_on_by_sweep(env.env.console, env.context, SWEEP_STICK_X, SWEEP_ROUNDS);
    const long long elapsed = std::chrono::duration_cast<Milliseconds>(current_time() - start).count();
    if (locked){
        backend.log(std::format("[PLZA] catch_in_zone: at {}: the move symbols showed {} ms into the lock-on sweep: locked on", at, elapsed));
        return true;
    }
    backend.log(std::format("[PLZA] catch_in_zone: at {}: no move symbols in {} sweep rounds ({} ms): L", at, SWEEP_ROUNDS, elapsed));
    return false;
}

//  Attacks until the wild battle is over or a round lands nothing, 3
//  rounds at most: a wild Pokemon of a species already caught, fought off
//  with no throws.
void fight_off(StoryEnv& env, PlzaBackend& backend){
    TrainerBattleState battle_state(env.battle_ai);
    for (int round = 1; round <= 3; round++){
        const int landed = weaken_target(env, backend, battle_state);
        press_a_through_lines(backend, LINES_QUIET, LINES_LIMIT);
        const bool still_on = wild_battle_on(env);
        backend.log(std::format("[PLZA] catch_in_zone: fighting off, round {}: {} attacks landed, battle {}", round, landed, still_on ? "on" : "over"));
        if (landed == 0 || !still_on){
            return;
        }
    }
}

//  A wild Pokemon fighting the player (Mareep, Bunnelby): ZL locks on to
//  it and reads its plate. Of a species already caught, it is fought off;
//  otherwise weaken_then_throw() until it is caught or gone.
GrassCatches fight_back(StoryEnv& env, PlzaBackend& backend, int max_throws){
    GrassCatches out;
    const std::optional<LockTarget> attacker = HeldZl(env).wait_for_plate(LOCK_ON_WAIT);
    if (attacker){
        log_target(backend, "a wild battle at the stop, locked on to", *attacker);
    }
    if (attacker && attacker->caught == true){
        backend.log("[PLZA] catch_in_zone: a species already caught: fighting it off, no throws");
        fight_off(env, backend);
        return out;
    }
    backend.log("[PLZA] catch_in_zone: a wild battle at the stop: weakening and throwing");
    const WeakenThrows weakened = weaken_then_throw(env, backend, max_throws);
    out.throws = weakened.throws;
    out.found = weakened.caught ? 1 : 0;
    return out;
}

//  At one stop (`at` from the objective), 4 tries at most, until a new
//  species is caught or `max_throws`: a wild battle on is fought
//  (fight_back()); otherwise it sweeps for a lock-on (sweep_for_lock_on()),
//  chooses the nearest target not caught (aim_choose_approach()) and
//  throws (throw_at_target()), as stop_aim() says. No lock-on in the
//  sweep, or THROWS_AT_ONE_TARGET misses at one lock: on to the next stop. All species caught: turns the camera right a little
//  and watches again. The pitch is reset only after ZL (CameraPitch).
//  After a catch, out of battle, the map pin is read and logged
//  (read_pin_on_map()): where the catch spots really are, for their
//  waypoints (ZONE1_SPOT_100, ZONE1_SPOT_60).
GrassCatches catch_at_stop(StoryEnv& env, PlzaBackend& backend, int at, int max_throws){
    GrassCatches out;
    CameraPitch pitch;
    for (int tries = 0; tries < 4 && out.found == 0 && out.throws < max_throws; tries++){
        backend.scope().throw_if_cancelled();
        if (pitch.level_now()){
            level_camera(env);
        }
        if (wild_battle_on(env)){
            out += fight_back(env, backend, max_throws - out.throws);
            pitch.zl_held();
            continue;
        }
        const bool locked = sweep_for_lock_on(env, backend, at);
        pitch.zl_held();
        const TargetChoice choice = locked ? aim_choose_approach(env, backend) : TargetChoice{};
        const StopAim aim = stop_aim(locked, choice.plates);
        if (aim == StopAim::MOVE_ON){
            backend.log(std::format("[PLZA] catch_in_zone: no lock-on at {} in the sweep", at));
            break;
        }
        if (aim == StopAim::LOOK_AGAIN){
            continue;
        }
        if (aim == StopAim::LOOK_FURTHER){
            backend.log("[PLZA] catch_in_zone: only species already caught lock on: looking further right");
            out.caught_seen++;
            pbf_move_right_joystick(env.context, /*position=*/{0.5, 0}, /*hold=*/Milliseconds(400), /*release=*/Milliseconds(300));
            continue;
        }
        const TargetThrows thrown = throw_at_target(env, backend, *choice.target, max_throws - out.throws);
        out += thrown.catches;
        if (thrown.move_on){
            break;
        }
    }
    if (out.found > 0){
        backend.log(std::format("[PLZA] catch_in_zone: caught at {}: the stop ends, on to the next", at));
    }
    if (pitch.level_now()){
        level_camera(env);
    }
    if (out.found > 0 && !wild_battle_on(env)){
        backend.log(std::format("[PLZA] catch_in_zone: the map pin after the catch at {}", at));
        read_pin_on_map(env, backend);
    }
    return out;
}

//  The party bar's filled slots, when it shows within 2 s and reads a
//  slot; none otherwise (a text box or a fade still on).
std::optional<int> read_party_slots(StoryEnv& env, PlzaBackend& backend){
    PartyBarDetector bar;
    for (int looks = 0; looks < 8; looks++){
        env.context.wait_for_all_requests();
        const VideoSnapshot frame = env.env.console.video().snapshot();
        if (frame && bar.detect(frame) && bar.filled_slots() > 0){
            backend.log(std::format("[PLZA] catch_in_zone: the party bar shows {} filled slots", bar.filled_slots()));
            return bar.filled_slots();
        }
        env.context.wait_for(Milliseconds(250));
    }
    backend.log("[PLZA] catch_in_zone: the party bar did not read within 2 s");
    return std::nullopt;
}

//  Turns the camera until the objective marker is in the middle of the
//  screen, 12 turns at most. Returns whether it is.
bool face_marker(StoryEnv& env, PlzaBackend& backend){
    for (int turns = 0; turns < 12; turns++){
        backend.scope().throw_if_cancelled();
        const Pokemon::Observation<Pokemon::ZonePose> look = backend.observe();
        std::optional<ObjectiveMarker> marker;
        if (look.frame && *look.frame){
            marker = read_objective_marker(env.env.console, *look.frame->frame);
        }
        if (!marker){
            pbf_move_right_joystick(env.context, /*position=*/{0.5, 0}, /*hold=*/Milliseconds(500), /*release=*/Milliseconds(200));
            continue;
        }
        const double off = marker->x - 0.5;
        if (off > -0.06 && off < 0.06){
            return true;
        }
        const Milliseconds turn((int)(std::min(1.0, std::abs(off) * 4) * 400) + 80);
        pbf_move_right_joystick(env.context, /*position=*/{off > 0 ? 0.4 : -0.4, 0}, turn, /*release=*/Milliseconds(150));
    }
    return false;
}

//  The camera level and onto the objective marker, then an agreed
//  distance read (read_marker_distance()). None when the marker was not
//  found or no read agreed.
std::optional<int> read_at_marker(StoryEnv& env, PlzaBackend& backend){
    level_camera(env);
    if (!face_marker(env, backend)){
        backend.log("[PLZA] catch_in_zone: no objective marker found to read");
        return std::nullopt;
    }
    //  Standing still: 24 looks, the digits flicker over leaves and sky.
    return read_marker_distance(env, backend, ROAD_FLOOR, /*looks=*/24);
}

std::string_view start_move_name(ZoneStartMove move){
    switch (move){
    case ZoneStartMove::WALK_TO_SPOT:
        return "off the rocks: the marker walk to the spot";
    case ZoneStartMove::SEARCH:
        return "at the spot: the patrol";
    case ZoneStartMove::ROCKS_NOT_LEFT:
        return "not at the rocks by the pin (or, unread, by the read off them): stop";
    case ZoneStartMove::SPOT_NOT_REACHED:
        return "the marker walk ended off the spot: stop";
    }
    return "?";
}

//  The objective marker's distance where a map leg ended, logged only: a
//  cross-check of the map's waypoints (MapWaypoint), and the stop's name
//  in the catch logs. The camera ends on the marker.
std::optional<int> cross_check_marker(StoryEnv& env, PlzaBackend& backend, const MapWaypoint& waypoint){
    const std::optional<int> distance = read_at_marker(env, backend);
    backend.log(std::format(
        "[PLZA] map leg cross-check: at {} the marker reads {}",
        waypoint.name, distance ? std::to_string(*distance) : std::string("nothing")
    ));
    return distance;
}

//  From the start save at Taunie's rocks to the patrol's first spot: the
//  pin read at the rocks (route.start), L puts the camera behind the
//  player, who faces the rocks and the marker; the leg off the rocks by
//  `route`, then the map legs to the 100 spot (ROCKS_TO_SPOT_100). The
//  pin decides whether the player starts at the rocks (ZoneStart::leave());
//  the marker read after the leg is a cross-check, and decides only when
//  the pin did not read. Fails as "rocks_not_left", or as
//  walk_map_legs() does, before any catch.
Pokemon::Outcome go_to_zone_spot(StoryEnv& env, PlzaBackend& backend, const ZoneStartRoute& route){
    backend.log(std::format(
        "[PLZA] catch_in_zone: at the start, {} is put at ({:.0f}, {:.0f}) on the map{}",
        route.start.name, route.start.pin.x, route.start.pin.y, route.start.provisional ? " [provisional]" : ""
    ));
    const std::optional<MapPoint> start_pin = read_pin_on_map(env, backend);
    backend.press(BUTTON_L, /*hold=*/Milliseconds(100), /*release=*/Milliseconds(150));
    backend.observe();
    backend.log(std::format("[PLZA] catch_in_zone: at Taunie's rocks, the arrow reads {:.0f} deg", minimap_arrow(env)()));
    pbf_move_left_joystick(env.context, /*position=*/{route.leg_x, route.leg_y}, route.leg_hold, /*release=*/Milliseconds(300));
    ZoneStart start(route);
    const std::optional<int> distance = read_at_marker(env, backend);
    const ZoneStartMove move = start.leave(start_pin, distance);
    const std::string pin_text = start_pin ? std::format("({:.0f}, {:.0f})", start_pin->x, start_pin->y) : std::string("not read");
    const std::string distance_text = distance ? std::to_string(*distance) : std::string("not read");
    backend.log(std::format(
        "[PLZA] catch_in_zone: pin at the start {}, distance off the rocks {} (expected {} to {}): {}",
        pin_text, distance_text, route.leg_read_min, route.leg_read_max, start_move_name(move)
    ));
    if (move != ZoneStartMove::WALK_TO_SPOT){
        return Pokemon::outcome_failed("rocks_not_left", std::format(
            "the pin at the start was {}, not within {:.0f} of ({:.0f}, {:.0f}); the read off Taunie's rocks was {}, not {} to {}",
            pin_text, route.start_radius, route.start.pin.x, route.start.pin.y, distance_text, route.leg_read_min, route.leg_read_max
        ));
    }
    return walk_map_legs(env, backend, ROCKS_TO_SPOT_100);
}

//  What the patrol came to.
struct Patrol{
    GrassCatches catches;
    //  The party bar's filled slots at the last read; none: not read.
    std::optional<int> party_slots;
    //  A walk between stops that failed (walk_map_legs()): the patrol
    //  stopped there.
    std::optional<Pokemon::Outcome> walk_failed;
};

//  catch_at_stop() at the user's two catch spots (CATCH_SPOTS, 100 and
//  60), back and forth (PATROL_VISITS visits), PATROL_LIMIT at most, by
//  map legs to each spot's waypoint (catch_spot_waypoint()); a leg that
//  fails stops the patrol (Patrol::walk_failed). It turns back at once
//  where a new species was caught (spot_visit()). A
//  wild battle that starts on the way is fought at the next stop. The
//  party bar's filled slots, read before the first stop (`slots`) and
//  after each stop that threw, end it at `goal.party_species`.
Patrol patrol_and_catch(StoryEnv& env, PlzaBackend& backend, const ZoneCatchGoal& goal, std::optional<int> slots){
    const std::vector<int> stops = alternating_stops(CATCH_SPOTS, PATROL_VISITS);
    const WallClock deadline = current_time() + PATROL_LIMIT;
    Patrol patrol;
    patrol.party_slots = slots;
    GrassCatches& result = patrol.catches;
    std::vector<int> caught_at;
    for (size_t i = 0; i < stops.size() && result.found < goal.new_species && result.throws < goal.max_throws; i++){
        backend.scope().throw_if_cancelled();
        if (current_time() > deadline){
            backend.log("[PLZA] catch_in_zone: out of time for the patrol");
            break;
        }
        const std::optional<MapWaypoint> waypoint = catch_spot_waypoint(stops[i]);
        if (!waypoint){
            backend.log(std::format("[PLZA] catch_in_zone: no map waypoint for the stop {}: the patrol ends", stops[i]));
            break;
        }
        //  The first stop is where go_to_zone_spot() ended.
        if (i > 0){
            const Pokemon::Outcome walked = walk_map_legs(env, backend, std::span<const MapWaypoint>(&*waypoint, 1));
            if (!walked.succeeded()){
                patrol.walk_failed = walked;
                break;
            }
        }
        const std::optional<int> at = cross_check_marker(env, backend, *waypoint);
        if (spot_visit(stops[i], caught_at) == SpotVisit::TURN_BACK && !wild_battle_on(env)){
            backend.log(std::format("[PLZA] catch_in_zone: stop {} of {} at {} (for {}): a new species was caught here: turning back", i + 1, stops.size(), at.value_or(-1), stops[i]));
            continue;
        }
        const GrassCatches stop = catch_at_stop(env, backend, at.value_or(stops[i]), goal.max_throws - result.throws);
        backend.log(std::format(
            "[PLZA] catch_in_zone: stop {} of {} at {} (for {}): {} throws, {} new species, {} locks on caught species only",
            i + 1, stops.size(), at.value_or(-1), stops[i], stop.throws, stop.found, stop.caught_seen
        ));
        result += stop;
        if (stop.found > 0){
            caught_at.push_back(stops[i]);
        }
        if (stop.throws > 0){
            patrol.party_slots = read_party_slots(env, backend);
            if (party_has_species(patrol.party_slots, goal.party_species)){
                backend.log(std::format("[PLZA] catch_in_zone: the party holds {} species: the patrol is done", goal.party_species));
                break;
            }
        }
    }
    return patrol;
}

} // namespace


Pokemon::Outcome catch_in_zone(StoryEnv& env, PlzaBackend& backend, const ZoneCatchGoal& goal){
    const std::optional<int> slots = read_party_slots(env, backend);
    if (party_has_species(slots, goal.party_species)){
        backend.log("[PLZA] catch_in_zone: the party already holds the species: nothing to catch");
        return Pokemon::outcome_ok();
    }
    const Pokemon::Outcome at_spot = go_to_zone_spot(env, backend, TAUNIE_ROCKS_ROUTE);
    if (!at_spot.succeeded()){
        return at_spot;
    }
    const Patrol patrol = patrol_and_catch(env, backend, goal, slots);
    if (patrol.walk_failed){
        return *patrol.walk_failed;
    }
    const GrassCatches& result = patrol.catches;
    const SpeciesTally tally = species_tally(result, goal.new_species);
    backend.log(std::format(
        "[PLZA] catch_in_zone: {}",
        catch_summary_text(result, patrol.party_slots, goal.new_species, goal.party_species)
    ));
    if (party_has_species(patrol.party_slots, goal.party_species)){
        return Pokemon::outcome_ok();
    }
    //  The party bar's count, read, stands in for the Pokedex's; unread,
    //  the banners' and plates' tally decides.
    if (!enough_species(tally, patrol.party_slots, goal.party_species)){
        return Pokemon::outcome_failed("short_of_species", patrol.party_slots
            ? std::format("the party holds {} species of {} ({} new by the banners in {} throws)", *patrol.party_slots, goal.party_species, result.found, result.throws)
            : std::format("{} new species of {} in {} throws", result.found, goal.new_species, result.throws)
        );
    }
    return Pokemon::outcome_ok();
}

Pokemon::Outcome walk_back_to_zone_road(StoryEnv& env, PlzaBackend& backend){
    const Pokemon::Outcome walked = walk_map_legs(env, backend, std::span<const MapWaypoint>(&ZONE1_SPOT_60, 1));
    if (!walked.succeeded()){
        return walked;
    }
    cross_check_marker(env, backend, ZONE1_SPOT_60);
    face_marker(env, backend);
    return Pokemon::outcome_ok();
}


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
