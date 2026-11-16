/*  PokemonLZA AutoStory - Catches
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include <iterator>
#include <optional>
#include <string>
#include <vector>
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonLZA/Inference/Battles/PokemonLZA_RunFromBattleDetector.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_AlertEyeDetector.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_PartyBarDetector.h"
#include "VideoGames/PokemonLZA/Programs/PokemonLZA_BallThrow.h"
#include "VideoGames/PokemonLZA/Programs/PokemonLZA_TrainerBattle.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryCaughtMarkReader.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryObjectiveMarkerReader.h"
#include "PokemonLZA_AutoStoryLines.h"
#include "PokemonLZA_AutoStoryMarkerWalk.h"
#include "PokemonLZA_AutoStoryCatches.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;


namespace{

//  The throw of plza/autostory's catches, which passed 03.07 on console B
//  (2026-11-13): the banner showed 6.9 s after the throw (03.08).
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
//  partner is out (03.08, console B 2026-11-14).
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

//  Wild Zone 1's two catch spots on its road, as distances from the
//  objective marker, the user's pick (2026-11-13): 100, the plaza by the
//  green advertising column (Scatterbug on the road), and 60, just before
//  the far gate (Mareep in the grass right of the road). The patrol goes
//  100, 60, 100, 60, ...: walking to 100 and back brings the Pokemon at 60
//  back when none showed there.
constexpr int ROAD_SPOTS[] = {100, 60};
constexpr size_t PATROL_VISITS = 10;
constexpr Milliseconds PATROL_LIMIT(600000);

//  How far short of a road stop still counts as there: the walk back to 60
//  from nearer the gate stopped at 58 once (console B, 2026-11-14).
constexpr int ROAD_STOP_SLACK = 3;
constexpr Milliseconds STOP_WALK_LIMIT(30000);
//  The first walk, from where 03.07 left the player, is the longest.
constexpr Milliseconds FIRST_WALK_LIMIT(90000);

//  How long ZL is held for the lock to show: over 1.5 s at times (the
//  user, console B 2026-11-14).
constexpr Milliseconds LOCK_ON_WAIT(3000);
//  A full flick of the right stick with ZL held moves the lock to the next
//  target that way (the user, 2026-11-13).
constexpr double TARGET_FLICK = 0.8;
//  The looks of a stop, the camera turned right between two.
constexpr int STOP_LOOKS = 4;

//  ZL held across reads of the screen: each hold is queued before the last
//  one ends, so ZL is never let go from the lock to the throw (throw_ball()
//  queues its own ZL first).
class HeldZl{
public:
    explicit HeldZl(StoryEnv& env)
        : m_env(env)
        , m_end(current_time())
    {}

    //  Holds ZL until 0.6 s after what was queued and reads the plate locked
    //  on to (lock_target_caught()), with 0.4 s more of ZL queued.
    std::optional<bool> read(){
        hold(Milliseconds(1000), /*right_x=*/0);
        const WallClock at = m_end - Milliseconds(400);
        const WallClock now = current_time();
        if (at > now){
            m_env.context.wait_for(std::chrono::duration_cast<Milliseconds>(at - now));
        }
        const VideoSnapshot frame = m_env.env.console.video().snapshot();
        if (!frame){
            return std::nullopt;
        }
        return lock_target_caught(frame);
    }

    //  read() until a plate shows or `limit` passes.
    std::optional<bool> wait_for_plate(Milliseconds limit){
        const WallClock end = current_time() + limit;
        while (true){
            const std::optional<bool> plate = read();
            if (plate || current_time() >= end){
                return plate;
            }
        }
    }

    //  Flicks the right stick right with ZL held: the lock moves on to the
    //  next target. Returns the plate after it.
    std::optional<bool> flick(){
        hold(Milliseconds(150), TARGET_FLICK);
        return read();
    }

private:
    void hold(Milliseconds time, double right_x){
        pbf_controller_state(
            m_env.context, BUTTON_ZL, DPAD_NONE,
            /*left_joystick=*/{0, 0}, /*right_joystick=*/{right_x, 0}, time
        );
        m_end = std::max(m_end, current_time()) + time;
    }

    StoryEnv& m_env;
    WallClock m_end;
};

//  What ZL locked on to at a stop.
enum class LockOn{
    NO_PLATE,
    ALL_CAUGHT,     //  3 flicks found only plates with the caught mark
    NOT_CAUGHT,     //  locked on, ZL still held, to one not caught
};

//  ZL held until a plate shows (LOCK_ON_WAIT at most), then flicks on
//  while it shows the caught mark (3 at most).
LockOn lock_on_not_caught(StoryEnv& env, PlzaBackend& backend){
    HeldZl held(env);
    std::optional<bool> caught = held.wait_for_plate(LOCK_ON_WAIT);
    for (int flicks = 0; caught == true && flicks < 3; flicks++){
        caught = held.flick();
    }
    if (!caught){
        backend.log(std::format("[PLZA] catch_in_zone: ZL held {} ms, no name plate", LOCK_ON_WAIT.count()));
        return LockOn::NO_PLATE;
    }
    if (*caught){
        backend.log("[PLZA] catch_in_zone: only species already caught lock on");
        return LockOn::ALL_CAUGHT;
    }
    return LockOn::NOT_CAUGHT;
}

//  A ZL lock-on and throw leave the camera looking steeply down: the
//  right stick down held 1500 ms pins the pitch at its lowest, then up
//  600 ms gives the walking pitch (console B, 2026-11-14).
void level_camera(StoryEnv& env){
    pbf_move_right_joystick(env.context, /*position=*/{0, -1}, /*hold=*/Milliseconds(1500), /*release=*/Milliseconds(100));
    pbf_move_right_joystick(env.context, /*position=*/{0, 1}, /*hold=*/Milliseconds(600), /*release=*/Milliseconds(300));
}

//  Rounds of weaken_target() and a throw, with A through the lines, while
//  a wild battle is on: until it is over (caught, or the target gone), a
//  round lands nothing, or `max_throws`. Returns the throws.
int weaken_and_throw(StoryEnv& env, PlzaBackend& backend, int max_throws, TrainerBattleState& battle_state){
    int throws = 0;
    while (throws < max_throws && wild_battle_on(env)){
        const int landed = weaken_target(env, backend, battle_state);
        if (landed == 0 && throws > 0){
            break;
        }
        press_a_through_lines(backend, LINES_QUIET, LINES_LIMIT);
        throw_ball(env.context, STORY_THROW);
        throws++;
        press_a_through_lines(backend, LINES_QUIET, LINES_LIMIT);
        backend.log(std::format("[PLZA] catch_in_zone: battle round {}: {} attacks landed, then a throw", throws, landed));
    }
    return throws;
}

//  A wild battle at the stop (a Mareep often turns on the player, the
//  user's walk on console B, 2026-11-13; Bunnelby, 2026-11-14): one of a
//  species already caught is fought off with no throws (3 rounds at
//  most); else weaken_and_throw(). Returns the throws.
int fight_back(StoryEnv& env, PlzaBackend& backend, int max_throws, TrainerBattleState& battle_state){
    const std::optional<bool> caught = HeldZl(env).wait_for_plate(LOCK_ON_WAIT);
    if (caught != true){
        backend.log("[PLZA] catch_in_zone: a wild battle at the stop: weakening and throwing");
        return weaken_and_throw(env, backend, max_throws, battle_state);
    }
    backend.log("[PLZA] catch_in_zone: a wild battle with a species already caught: fighting it off");
    for (int round = 0; round < 3 && wild_battle_on(env); round++){
        if (weaken_target(env, backend, battle_state) == 0){
            break;
        }
        press_a_through_lines(backend, LINES_QUIET, LINES_LIMIT);
    }
    return 0;
}

//  One stop: a wild battle on is fought (fight_back()); else, look by
//  look (STOP_LOOKS, the camera turned right between two), a throw at the
//  first target not caught (lock_on_not_caught()), then weaken_and_throw()
//  when it broke out into a wild battle. Returns the throws.
int catch_at_stop(StoryEnv& env, PlzaBackend& backend, int max_throws, TrainerBattleState& battle_state){
    int throws = 0;
    for (int look = 0; look < STOP_LOOKS && throws == 0 && max_throws > 0; look++){
        if (wild_battle_on(env)){
            throws += fight_back(env, backend, max_throws, battle_state);
            break;
        }
        if (lock_on_not_caught(env, backend) != LockOn::NOT_CAUGHT){
            pbf_move_right_joystick(env.context, /*position=*/{0.5, 0}, /*hold=*/Milliseconds(400), /*release=*/Milliseconds(300));
            continue;
        }
        throw_ball(env.context, STORY_THROW);
        throws++;
        press_a_through_lines(backend, LINES_QUIET, LINES_LIMIT);
        throws += weaken_and_throw(env, backend, max_throws - throws, battle_state);
    }
    level_camera(env);
    return throws;
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

bool party_done(const std::optional<int>& slots, const ZoneCatchGoal& goal){
    return slots && *slots >= goal.party_species;
}

//  Walks to stop `stop` from `last` (none: from the start, further out
//  than every stop): back from the objective when the stop is further.
std::optional<int> walk_to_stop(StoryEnv& env, PlzaBackend& backend, std::optional<int> last, int stop){
    if (last && stop > *last){
        return walk_back_from_marker(env, backend, stop, STOP_WALK_LIMIT);
    }
    return walk_toward_marker(env, backend, stop, last ? STOP_WALK_LIMIT : FIRST_WALK_LIMIT);
}

//  The objective's distance on the screen now, if it reads.
std::optional<int> objective_distance_now(StoryEnv& env){
    env.context.wait_for_all_requests();
    const VideoSnapshot frame = env.env.console.video().snapshot();
    if (!frame){
        return std::nullopt;
    }
    const std::optional<ObjectiveMarker> marker = read_objective_marker(env.env.console, frame);
    if (!marker || marker->distance <= 0){
        return std::nullopt;
    }
    return marker->distance;
}

//  What the patrol came to: the party bar's last read, the throws.
struct Patrol{
    std::optional<int> slots;
    int throws = 0;
};

//  catch_at_stop() at ROAD_SPOTS in turn, until the party bar, read after
//  each stop that threw, shows `goal.party_species`, or PATROL_VISITS,
//  `goal.max_throws` or PATROL_LIMIT run out. `slots` is the bar's read
//  before the first stop.
Patrol patrol_road(StoryEnv& env, PlzaBackend& backend, const ZoneCatchGoal& goal, std::optional<int> slots){
    Patrol patrol{.slots = slots};
    TrainerBattleState battle_state(env.battle_ai);
    const WallClock deadline = current_time() + PATROL_LIMIT;
    std::optional<int> last;
    for (size_t visit = 0; visit < PATROL_VISITS && patrol.throws < goal.max_throws && current_time() < deadline; visit++){
        const int stop = ROAD_SPOTS[visit % std::size(ROAD_SPOTS)];
        const std::optional<int> at = walk_to_stop(env, backend, last, stop);
        last = at ? at : stop;
        const int thrown = catch_at_stop(env, backend, goal.max_throws - patrol.throws, battle_state);
        patrol.throws += thrown;
        backend.log(std::format("[PLZA] catch_in_zone: stop {} at {} (for {}): {} throws", visit + 1, at.value_or(-1), stop, thrown));
        if (thrown > 0){
            patrol.slots = read_party_slots(env, backend);
        }
        if (party_done(patrol.slots, goal)){
            break;
        }
    }
    return patrol;
}

//  The road stops to walk through from `at` from the objective, in order:
//  the spots of ROAD_SPOTS nearer the gate, furthest first; at or past the
//  one nearest the gate, that one: back onto the road there first, as a
//  walk at the gate from off the road ran into a building west of the zone
//  (console B, 2026-11-13, last read 49). With no distance read, every spot.
std::vector<int> road_stops_to_gate(std::optional<int> at){
    std::vector<int> stops;
    for (int spot : ROAD_SPOTS){
        if (!at || spot < *at){
            stops.push_back(spot);
        }
    }
    if (stops.empty()){
        stops.push_back(ROAD_SPOTS[std::size(ROAD_SPOTS) - 1]);
    }
    return stops;
}

} // namespace


Pokemon::Outcome catch_in_zone(StoryEnv& env, PlzaBackend& backend, const ZoneCatchGoal& goal){
    //  Taunie's two rocks stand between 03.07's end and the marker: a step
    //  right first, around them (a catch's autosave can leave the save
    //  further on).
    const std::optional<int> start = objective_distance_now(env);
    if (start && *start >= 115){
        pbf_move_left_joystick(env.context, /*position=*/{1, 0}, /*hold=*/Milliseconds(1000), /*release=*/Milliseconds(300));
    }
    const std::optional<int> slots = read_party_slots(env, backend);
    if (party_done(slots, goal)){
        backend.log("[PLZA] catch_in_zone: the party already holds the species: nothing to catch");
        return Pokemon::outcome_ok();
    }
    const Patrol patrol = patrol_road(env, backend, goal, slots);
    if (party_done(patrol.slots, goal)){
        return Pokemon::outcome_ok();
    }
    return Pokemon::outcome_failed("short_of_species", std::format(
        "the party bar shows {} of {} species after {} throws",
        patrol.slots ? std::to_string(*patrol.slots) : std::string("no read"), goal.party_species, patrol.throws
    ));
}

Pokemon::Outcome walk_back_to_zone_road(StoryEnv& env, PlzaBackend& backend){
    std::optional<int> at = objective_distance_now(env);
    for (int stop : road_stops_to_gate(at)){
        backend.log(std::format("[PLZA] walk_back_to_zone_road: to {} on the road, from {}", stop, at.value_or(-1)));
        const bool toward = !at || stop <= *at;
        const std::optional<int> reached = walk_to_stop(env, backend, at, stop);
        const bool arrived = reached && (toward ? *reached <= stop + ROAD_STOP_SLACK : *reached >= stop - ROAD_STOP_SLACK);
        if (!arrived){
            return Pokemon::outcome_failed("road_stop_not_reached", std::format(
                "not back on the road at {} (last read {})", stop, reached.value_or(-1)
            ));
        }
        at = reached;
    }
    return Pokemon::outcome_ok();
}


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
