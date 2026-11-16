/*  PokemonLZA AutoStory - Catches
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cmath>
#include <format>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
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
#include "PokemonLZA_AutoStoryZoneSearch.h"
#include "PokemonLZA_AutoStoryZoneStart.h"
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

//  Wild Zone 1's two spots on its road, as distances from the objective
//  marker, the user's pick (2026-11-13): 100, the plaza by the green
//  advertising column (the search spot), and 60, just before the far
//  gate. walk_back_to_zone_road() walks back onto the road at them.
constexpr int ROAD_SPOTS[] = {ZONE_SPOT_DISTANCE, 60};

//  How far short of a road stop still counts as there: the walk back to 60
//  from nearer the gate stopped at 58 once (console B, 2026-11-14).
constexpr int ROAD_STOP_SLACK = 3;
//  Walks toward the objective never go below this, read or not: 60 is
//  just before the far gate, and past it is the city (console B,
//  2026-11-16, a walk to 100 came out at 37).
constexpr int ROAD_FLOOR = ROAD_SPOTS[std::size(ROAD_SPOTS) - 1] - ROAD_STOP_SLACK;
constexpr Milliseconds STOP_WALK_LIMIT(30000);
//  A walk with no distance read to start from is the longest.
constexpr Milliseconds FIRST_WALK_LIMIT(90000);

//  catch_in_zone()'s time in the zone: spawns come back, so a search that
//  finds nothing new keeps going until this runs out.
constexpr Milliseconds SEARCH_LIMIT(600000);
//  A scan's camera steps, the right stick full right for SCAN_STEP_TURN
//  each: meant as a full turn in all; not measured on console in degrees.
constexpr size_t SCAN_STEPS = 8;
constexpr Milliseconds SCAN_STEP_TURN(300);
//  One leg of the search (ZoneLeg), the left stick full one way, with the
//  camera on the objective marker: a few steps into the grass, short of
//  the 30 s walks that left the zone (console B, 2026-11-16).
constexpr Milliseconds LEG_HOLD(2000);
//  A ZoneMove::PAUSE: standing for spawns to come back.
constexpr Milliseconds SEARCH_PAUSE(15000);

//  How long ZL is held for the lock to show: over 1.5 s at times (the
//  user, console B 2026-11-14).
constexpr Milliseconds LOCK_ON_WAIT(3000);
//  A full flick of the right stick with ZL held moves the lock to the next
//  target that way (the user, 2026-11-13).
constexpr double TARGET_FLICK = 0.8;
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

//  ZL held until a plate shows (LOCK_ON_WAIT at most), then flicks on
//  while it shows the caught mark (3 at most): NO_PLATE, ALL_CAUGHT, or
//  NOT_CAUGHT with ZL still held on the target.
ZoneLook lock_on_not_caught(StoryEnv& env){
    HeldZl held(env);
    std::optional<bool> caught = held.wait_for_plate(LOCK_ON_WAIT);
    for (int flicks = 0; caught == true && flicks < 3; flicks++){
        caught = held.flick();
    }
    if (!caught){
        return ZoneLook::NO_PLATE;
    }
    return *caught ? ZoneLook::ALL_CAUGHT : ZoneLook::NOT_CAUGHT;
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

//  A wild battle on (a Mareep often turns on the player, the
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

std::string_view look_name(ZoneLook look){
    switch (look){
    case ZoneLook::NO_PLATE:
        return "no name plate";
    case ZoneLook::ALL_CAUGHT:
        return "only species already caught";
    case ZoneLook::NOT_CAUGHT:
        return "a species not caught";
    case ZoneLook::WILD_BATTLE:
        return "a wild battle";
    }
    return "?";
}

std::string_view leg_name(ZoneLeg leg){
    switch (leg){
    case ZoneLeg::TOWARD:
        return "toward the objective";
    case ZoneLeg::AWAY:
        return "away from the objective";
    case ZoneLeg::LEFT:
        return "left";
    case ZoneLeg::RIGHT:
        return "right";
    }
    return "?";
}

//  A look at the scan's camera step: a wild battle on, else the ZL
//  lock-on's plates (lock_on_not_caught()).
ZoneLook look_in_zone(StoryEnv& env){
    if (wild_battle_on(env)){
        return ZoneLook::WILD_BATTLE;
    }
    return lock_on_not_caught(env);
}

//  The left stick for `leg`, the camera on the objective marker.
JoystickPosition leg_stick(ZoneLeg leg){
    switch (leg){
    case ZoneLeg::TOWARD:
        return {0, 1};
    case ZoneLeg::AWAY:
        return {0, -1};
    case ZoneLeg::LEFT:
        return {-1, 0};
    case ZoneLeg::RIGHT:
        return {1, 0};
    }
    return {0, 0};
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
    return read_marker_distance(env, backend, std::nullopt);
}

//  One leg of the search: an agreed distance read (read_at_marker()) for
//  ZoneSearch::leg()'s floor, then LEG_HOLD that way, the camera on the
//  marker. With no marker found the leg is taken by the camera as it is,
//  logged.
void walk_leg(StoryEnv& env, PlzaBackend& backend, ZoneSearch& search){
    const std::optional<int> distance = read_at_marker(env, backend);
    const ZoneLeg leg = search.leg(distance);
    const ZoneOffset at = search.offset();
    backend.log(std::format(
        "[PLZA] catch_in_zone: nothing new in a full turn: a leg {} (distance {}), now {} toward and {} right of the spot in legs",
        leg_name(leg), distance ? std::to_string(*distance) : std::string("not read"), at.toward, at.right
    ));
    pbf_move_left_joystick(env.context, leg_stick(leg), LEG_HOLD, /*release=*/Milliseconds(300));
}

//  The marker walk from `at` to the search spot (ZONE_SPOT_DISTANCE):
//  back from the objective when nearer it, else on toward it, never below
//  ROAD_FLOOR. Returns the last read taken.
std::optional<int> walk_to_zone_spot(StoryEnv& env, PlzaBackend& backend, int at, Milliseconds limit){
    backend.log(std::format("[PLZA] catch_in_zone: the marker walk to {} on the road, from {}", ZONE_SPOT_DISTANCE, at));
    if (at < ZONE_SPOT_DISTANCE){
        return walk_back_from_marker(env, backend, ZONE_SPOT_DISTANCE, limit);
    }
    return walk_toward_marker(env, backend, ZONE_SPOT_DISTANCE, ROAD_FLOOR, limit);
}

//  After a catch, a fight or a round of legs: the marker walk back to the
//  search spot, so the legs go out from it again (ZoneSearch::at_spot()).
//  Short of it, the legs go on from where the player stands, logged.
void back_to_zone_spot(StoryEnv& env, PlzaBackend& backend, ZoneSearch& search){
    const std::optional<int> at = read_at_marker(env, backend);
    const std::optional<int> reached = at ? walk_to_zone_spot(env, backend, *at, STOP_WALK_LIMIT) : std::nullopt;
    if (at_zone_spot(reached)){
        search.at_spot();
        return;
    }
    backend.log(std::format(
        "[PLZA] catch_in_zone: not back at the spot (last read {}): the legs go on from here",
        reached ? std::to_string(*reached) : std::string("none")
    ));
}

std::string_view start_move_name(ZoneStartMove move){
    switch (move){
    case ZoneStartMove::WALK_TO_SPOT:
        return "off the rocks: the marker walk to the spot";
    case ZoneStartMove::SEARCH:
        return "at the spot: the search";
    case ZoneStartMove::ROCKS_NOT_LEFT:
        return "not the read off the rocks the route expects: stop";
    case ZoneStartMove::SPOT_NOT_REACHED:
        return "the marker walk ended off the spot: stop";
    }
    return "?";
}

//  From the start save at Taunie's rocks to the search spot by `route`
//  (ZoneStart): L puts the camera behind the player, who faces the rocks
//  and the marker; the leg off the rocks, an agreed read in its range,
//  then the marker walk. Fails as "rocks_not_left" or
//  "zone_spot_not_reached", before any search.
Pokemon::Outcome go_to_zone_spot(StoryEnv& env, PlzaBackend& backend, const ZoneStartRoute& route){
    backend.press(BUTTON_L, /*hold=*/Milliseconds(100), /*release=*/Milliseconds(150));
    backend.observe();
    backend.log(std::format("[PLZA] catch_in_zone: at Taunie's rocks, the arrow reads {:.0f} deg", minimap_arrow(env)()));
    pbf_move_left_joystick(env.context, /*position=*/{route.leg_x, route.leg_y}, route.leg_hold, /*release=*/Milliseconds(300));
    ZoneStart start(route);
    std::optional<int> distance = read_at_marker(env, backend);
    while (true){
        backend.scope().throw_if_cancelled();
        const ZoneStartMove move = start.next(distance);
        backend.log(std::format(
            "[PLZA] catch_in_zone: distance {}: {}",
            distance ? std::to_string(*distance) : std::string("not read"), start_move_name(move)
        ));
        switch (move){
        case ZoneStartMove::WALK_TO_SPOT:
            distance = walk_toward_marker(env, backend, route.walk_stop, ROAD_FLOOR, route.walk_limit);
            continue;
        case ZoneStartMove::SEARCH:
            return Pokemon::outcome_ok();
        case ZoneStartMove::ROCKS_NOT_LEFT:
            return Pokemon::outcome_failed("rocks_not_left", std::format(
                "the read off Taunie's rocks was {}, not {} to {}",
                distance ? std::to_string(*distance) : std::string("none"), route.leg_read_min, route.leg_read_max
            ));
        case ZoneStartMove::SPOT_NOT_REACHED:
            return Pokemon::outcome_failed("zone_spot_not_reached", std::format(
                "the marker walk to {} ended at {}", route.walk_stop,
                distance ? std::to_string(*distance) : std::string("no agreed read")
            ));
        }
    }
}

//  A throw at the target ZL is held on, then weaken_and_throw() when it
//  broke out into a wild battle. Returns the throws.
int throw_at_lock(StoryEnv& env, PlzaBackend& backend, int max_throws, TrainerBattleState& battle_state){
    throw_ball(env.context, STORY_THROW);
    press_a_through_lines(backend, LINES_QUIET, LINES_LIMIT);
    return 1 + weaken_and_throw(env, backend, max_throws - 1, battle_state);
}

//  What the search came to: the party bar's last read, the throws.
struct SearchResult{
    std::optional<int> slots;
    int throws = 0;
};

//  The search (ZoneSearch), look by look, until the party bar, read after
//  each catch or fight that threw, shows `goal.party_species`, or
//  `goal.max_throws` or SEARCH_LIMIT run out. `slots` is the bar's read
//  before the first look.
SearchResult search_zone(StoryEnv& env, PlzaBackend& backend, const ZoneCatchGoal& goal, std::optional<int> slots){
    SearchResult result{.slots = slots};
    ZoneSearch search(SCAN_STEPS);
    TrainerBattleState battle_state(env.battle_ai);
    const WallClock deadline = current_time() + SEARCH_LIMIT;
    while (result.throws < goal.max_throws && current_time() < deadline){
        backend.scope().throw_if_cancelled();
        const size_t step = search.step();
        const ZoneLook look = look_in_zone(env);
        const ZoneMove move = search.look(look);
        backend.log(std::format("[PLZA] catch_in_zone: scan step {} of {}: {}", step + 1, SCAN_STEPS, look_name(look)));
        int thrown = 0;
        switch (move){
        case ZoneMove::TURN:
            pbf_move_right_joystick(env.context, /*position=*/{1, 0}, SCAN_STEP_TURN, /*release=*/Milliseconds(300));
            continue;
        case ZoneMove::CATCH:
            thrown = throw_at_lock(env, backend, goal.max_throws - result.throws, battle_state);
            break;
        case ZoneMove::FIGHT:
            thrown = fight_back(env, backend, goal.max_throws - result.throws, battle_state);
            break;
        case ZoneMove::WALK:
            walk_leg(env, backend, search);
            continue;
        case ZoneMove::PAUSE:
            back_to_zone_spot(env, backend, search);
            backend.log(std::format("[PLZA] catch_in_zone: every leg walked out and back: standing {} s for spawns", SEARCH_PAUSE.count() / 1000));
            level_camera(env);
            backend.wait(SEARCH_PAUSE);
            continue;
        }
        level_camera(env);
        result.throws += thrown;
        backend.log(std::format("[PLZA] catch_in_zone: {} throws ({} in all)", thrown, result.throws));
        if (thrown > 0){
            result.slots = read_party_slots(env, backend);
        }
        if (party_done(result.slots, goal)){
            break;
        }
        back_to_zone_spot(env, backend, search);
    }
    return result;
}

//  Walks to stop `stop` from `last` (none: from the start, further out
//  than every stop): back from the objective when the stop is further.
std::optional<int> walk_to_stop(StoryEnv& env, PlzaBackend& backend, std::optional<int> last, int stop){
    if (last && stop > *last){
        return walk_back_from_marker(env, backend, stop, STOP_WALK_LIMIT);
    }
    return walk_toward_marker(env, backend, stop, ROAD_FLOOR, last ? STOP_WALK_LIMIT : FIRST_WALK_LIMIT);
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
    const std::optional<int> slots = read_party_slots(env, backend);
    if (party_done(slots, goal)){
        backend.log("[PLZA] catch_in_zone: the party already holds the species: nothing to catch");
        return Pokemon::outcome_ok();
    }
    const Pokemon::Outcome at_spot = go_to_zone_spot(env, backend, TAUNIE_ROCKS_ROUTE);
    if (!at_spot.succeeded()){
        return at_spot;
    }
    const SearchResult result = search_zone(env, backend, goal, slots);
    if (party_done(result.slots, goal)){
        return Pokemon::outcome_ok();
    }
    return Pokemon::outcome_failed("short_of_species", std::format(
        "the party bar shows {} of {} species after {} throws",
        result.slots ? std::to_string(*result.slots) : std::string("no read"), goal.party_species, result.throws
    ));
}

Pokemon::Outcome walk_back_to_zone_road(StoryEnv& env, PlzaBackend& backend){
    std::optional<int> at = read_marker_distance(env, backend, std::nullopt);
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
