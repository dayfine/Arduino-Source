/*  PokemonLZA AutoStory - Fault Sweep Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The shared fault sweep (Pokemon_StoryFaultSweep.h) over PLZA's rows, on
 *  a row sim (RowSim below) of the steps that need no console: the timed
 *  walks ("walk"), the walk to a wild zone's gate
 *  ("walk_to_zone_gate_entrance") and A through a gate or door's prompt
 *  ("mash_to_move_hints" with press_a_first). A step the sim does not play
 *  (a console step, a scene) puts the player at its end, if it started at
 *  its start with no fade under way; else the player is off the story. A
 *  look with no input taken since the look before shows the screen still
 *  (SCREEN_STILL), as the console's frame diff does.
 *
 *  The sim faults it injects, each a class the console showed or could:
 *
 *  - LOST_TAP: each input the sim acts on (a stick, a hold, an A), one at
 *    a time, dropped by the game.
 *  - LATE_FADE: every prompt's fade starts LATE_FADE later.
 *  - WARP_NO_OP: each prompt's fade, one at a time, leaves the player
 *    where it was, prompt and all.
 *  - SLOW_FADE_IN: after every fade the game takes no input for
 *    SLOW_FADE_IN.
 *  - SCENE_END_POSE: the player facing SCENE_END_TURN degrees off, either
 *    way, at the row's start (a load) and after each step the game ends
 *    (a prompt's scene).
 *
 *  Not swept: UNSEEN_STEP (PLZA's walks read no steps), MOUNT_LOST (no
 *  mount yet), DARK_CAVE, SCENE_EARLY and BATTLE_OPENING (none on these
 *  rows), WILD_BATTLES (real-time wild Pokemon, which the sim does not
 *  model). The PLZA failures seen on console B that no sim fault injects
 *  are classed by their failure names instead (PokemonLZA_AutoStoryFailures.h):
 *  a missed gate (no_prompt, screen_unread), stuck on the title
 *  (launch_failed), the zone start short by 2 (zone_start_missed,
 *  walk_short).
 *
 *  The sim's world is small: the player is at the end of a row's step (a
 *  spot), facing a compass heading. A timed walk reaches its end only on the
 *  inputs and facings of the row's clean run (or, for a walk into a wall
 *  that funnels it, the inputs and a facing within FUNNEL_TOLERANCE); a
 *  walk to a gate gets there when it walks the gate's way. Each row runs
 *  clean first; a variant fails when the row then fails or ends elsewhere.
 *
 */

#include <cmath>
#include <format>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/CancellableScope.h"
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Pokemon_BuiltinStoryActions.h"
#include "Pokemon/AutoStory/Pokemon_StoryFaultSweep.h"
#include "Pokemon/AutoStory/Pokemon_StoryOutcomeClass.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryEngine.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryFakeBackend.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Actions/PokemonLZA_AutoStoryGateWalk.h"
#include "PokemonLZA_AutoStoryGame.h"
#include "PokemonLZA_AutoStoryReadings.h"
#include "PokemonLZA_AutoStoryStoryFiles.h"
#include "PokemonLZA_AutoStoryFaultSweep_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;
using Pokemon::FaultClass;
using Pokemon::FaultSweepResult;
using Pokemon::Observation;
using Pokemon::StoryCheckpoint;
using Pokemon::StoryStep;
using Pokemon::ZonePose;


namespace{

const std::string TEST_PREFIX = "PokemonLZA::AutoStory";

//  The rows swept.
const std::vector<std::string> SWEPT_ROWS{"03.07d", "03.08"};

//  The sweeps the program fails today, by test name, with the likely fix.
const std::map<std::string, std::string> KNOWN_FAILING{
    {"PokemonLZA::AutoStory - Fault sweep: 03.07d, warp_no_op",
        "enter_zone_1 is done with the player still outside the gate: after the gate's fade the prompt shows again. Pressing A again on a prompt after a fade is not safe: the zone's far side of the gate may show its own prompt (A Leave) where the warp lands, and A there would undo it; telling the two apart needs a reading of the place (the minimap's zone green). On the console, walk_to_zone_1_start's green check fails as zone_start_missed"},
    {"PokemonLZA::AutoStory - Fault sweep: 03.08, warp_no_op",
        "leave_zone_1 or lab_lobby is done with the player still at the prompt: after the fade the prompt shows again. Pressing A again on a prompt after a fade is not safe: the street outside the zone's gate may show the gate's A Enter where the warp lands, and A there would undo it; telling the two apart needs a reading of the place"},
};

//  How much later than the clean run's a late fade starts.
constexpr Milliseconds LATE_FADE = 1500ms;
//  After a fade, how long the game takes no input in SLOW_FADE_IN.
constexpr Milliseconds SLOW_FADE_IN = 1000ms;
//  How far off a scene's end leaves the player's facing.
constexpr double SCENE_END_TURN = 30;

//  A prompt's fade: it starts this long after the A, and lasts this long.
constexpr Milliseconds FADE_START = 300ms;
constexpr Milliseconds FADE_LENGTH = 1500ms;
//  The walk to a gate: how far it starts from the gate's prompt, at what
//  pace (~5 units/s, 03.06's walks, console B 2026-11-08), how far off the
//  gate's bearing the screen shows it and a slice walks toward it.
constexpr double GATE_DISTANCE = 15;
constexpr double UNITS_PER_MS = 0.005;
constexpr double GATE_IN_VIEW = 30;
constexpr double GATE_WALK_CONE = 20;
constexpr double VIEW_DEGREES = 60;
//  A turn: story_turn()'s stick is this short and released.
constexpr Milliseconds TURN_STICK = 150ms;
//  How far off its clean facing a funnel walk still ends against its wall.
constexpr double FUNNEL_TOLERANCE = 45;

//  Walks into a wall that sets the player whatever its facing at the start:
//  03.07d's walk into the nurse's counter, "whatever way the camera turned
//  at the load" (console B, 2026-11-14, 3 of 3).
const std::set<std::string, std::less<>> FUNNEL_WALKS{"into_the_counter"};

//  The degrees from `from` to `to`, -180 to 180.
double turn_between(double from, double to){
    double turn = std::fmod(to - from, 360.0);
    if (turn > 180){
        turn -= 360;
    }else if (turn <= -180){
        turn += 360;
    }
    return turn;
}

//  Which steps the sim plays.
enum class SimStep{
    WALK,       //  "walk": timed legs
    GATE,       //  "walk_to_zone_gate_entrance"
    PROMPT,     //  "mash_to_move_hints" with press_a_first
    NOT_PLAYED, //  a console step or a scene
};

SimStep sim_step(const StoryStep& step){
    if (step.action == "walk"){
        return SimStep::WALK;
    }
    if (step.action == "walk_to_zone_gate_entrance"){
        return SimStep::GATE;
    }
    if (step.action == "mash_to_move_hints" && Pokemon::story_arg<bool>(step.args, "press_a_first")){
        return SimStep::PROMPT;
    }
    return SimStep::NOT_PLAYED;
}

//  The faults one variant plays.
struct SimFaults{
    //  LOST_TAP: the input the game drops, from 0 over those the sim acts on.
    std::optional<size_t> dropped_input = std::nullopt;
    //  LATE_FADE: how much later every prompt's fade starts.
    Milliseconds late_fade = 0ms;
    //  WARP_NO_OP: the prompt's fade, from 0, that leaves the player there.
    std::optional<size_t> no_op_fade = std::nullopt;
    //  SLOW_FADE_IN: no input this long after each fade.
    Milliseconds fade_in = 0ms;
    //  SCENE_END_POSE: the facing off by `turn` after the step named
    //  `after` ("" for the row's start).
    struct PoseOff{
        std::string after;
        double turn = 0;
    };
    std::optional<PoseOff> pose_off = std::nullopt;
};

//  What the row's clean run did, which a faulted run is held to.
struct CleanRun{
    //  Per timed walk: its inputs and the facing at each.
    struct Walk{
        std::vector<std::string> inputs;
        std::vector<double> facings;
    };
    std::map<std::string, Walk, std::less<>> walks;
    //  Per step: the facing it ends on.
    std::map<std::string, double, std::less<>> end_facing;
    size_t inputs = 0;
    size_t fades = 0;
    //  Inputs that came within SLOW_FADE_IN of a fade's end.
    size_t inputs_after_fades = 0;
};

//  A prompt's fade under way.
struct Fade{
    WallClock start;
    WallClock end;
    bool moves;
    std::string to;
};

//  One row's game, played as the inputs come.
//  code-health: allow public_methods -- the backend calls it plays, plus the row driver's step bounds and the clean run's record
class RowSim : public Pokemon::StoryBackend<ZonePose>{
public:
    RowSim(const StoryCheckpoint& row, SimFaults faults, const CleanRun* clean, CancellableScope& scope)
        : m_row(row), m_faults(std::move(faults)), m_clean(clean), m_scope(scope), m_start(current_time())
    {
        for (size_t i = 0; i + 1 < row.steps.size(); i++){
            if (sim_step(row.steps[i + 1]) == SimStep::PROMPT){
                m_prompt_spots.insert(row.steps[i].name);
            }
        }
        turn_if_posed("");
    }

    const CleanRun& record() const{
        return m_record;
    }
    //  Where the row ended, when not at the end of its step `last` with
    //  the game settled: "at 03.07d's to_zone_1_gate", "off the story: ...".
    std::optional<std::string> off_end(std::string_view last) const{
        if (m_spot == last && !m_fade){
            return std::nullopt;
        }
        std::string where = m_spot.starts_with("off") ? m_spot : std::format("at {}", spot_name(m_spot));
        return m_fade ? std::format("{}, in a fade", where) : where;
    }

    //  Step `index` of the row starts.
    void begin_step(size_t index){
        m_step = index;
        m_walk = CleanRun::Walk{};
        m_gate.reset();
        if (sim_step(step()) == SimStep::GATE && m_spot == start_of(index)){
            m_gate = GATE_DISTANCE;
        }
    }
    //  The step started by begin_step() is over.
    void end_step(){
        const StoryStep& now = step();
        switch (sim_step(now)){
        case SimStep::WALK:
            if (walk_reached()){
                arrive(now.name);
            }else if (m_spot == start_of(m_step)){
                m_spot = std::format("off the story after {}", now.name);
            }
            break;
        case SimStep::NOT_PLAYED:
            if (m_spot == start_of(m_step) && !m_fade){
                arrive(now.name);
            }else if (!m_spot.starts_with("off")){
                m_spot = std::format("off the story: {} started {}", now.name, m_fade ? "in a fade" : std::format("at {}", spot_name(m_spot)));
            }
            break;
        default:
            break;
        }
        m_record.walks[now.name] = m_walk;
        m_record.end_facing[now.name] = m_facing;
        turn_if_posed(now.name);
    }

    virtual Pokemon::StoryBackendKind kind() const override{
        return Pokemon::StoryBackendKind::SCRIPT;
    }
    virtual void press(Button button, Milliseconds hold, Milliseconds release) override{
        if (button == BUTTON_A && take_input("press A") && sim_step(step()) == SimStep::PROMPT && prompt_shows()){
            start_fade();
        }
        m_clock += hold + release;
    }
    virtual void tap(Pokemon::Direction) override{
        m_clock += 100ms;
    }
    virtual void hold(Pokemon::Direction, Milliseconds duration, Pokemon::StoryPace) override{
        take_input(std::format("hold {}ms", duration.count()));
        m_clock += duration;
    }
    virtual void stick(double degrees, Milliseconds duration, Milliseconds release, Pokemon::StoryPace) override{
        const bool turn = duration <= TURN_STICK && release > 0ms;
        if (take_input(std::format("{} {:g}deg {}ms", turn ? "turn" : "stick", degrees, duration.count()))){
            if (turn){
                m_facing = std::fmod(m_facing + degrees + 360, 360);
            }else{
                walk_toward_gate(degrees, duration);
            }
        }
        m_clock += duration + release;
    }
    virtual void wait(Milliseconds duration) override{
        m_clock += duration;
    }
    virtual Observation<ZonePose> observe() override;
    virtual Pokemon::Outcome battle(const Pokemon::StoryActionArgs&) override{
        return Pokemon::outcome_failed("no_battle", "the row sim plays no battles");
    }
    virtual void log(std::string_view line) override{
        m_lines.emplace_back(line);
    }
    virtual CancellableScope& scope() override{
        return m_scope;
    }

private:
    const StoryStep& step() const{
        return m_row.steps[m_step];
    }
    //  Where step `index` starts: the end of the one before, "" for the first.
    std::string start_of(size_t index) const{
        return index == 0 ? "" : m_row.steps[index - 1].name;
    }
    static std::string spot_name(const std::string& spot){
        return spot.empty() ? "the row's start" : spot;
    }
    WallClock now() const{
        return m_start + m_clock;
    }
    bool prompt_shows() const{
        return m_prompt_spots.contains(m_spot) && !m_fade;
    }

    void arrive(const std::string& name){
        m_spot = name;
        if (m_clean != nullptr){
            m_facing = m_clean->end_facing.at(name);
        }
    }
    void turn_if_posed(std::string_view after){
        if (m_faults.pose_off && m_faults.pose_off->after == after){
            m_facing = std::fmod(m_facing + m_faults.pose_off->turn + 360, 360);
        }
    }

    //  Counts `input` and says whether the game takes it: not dropped, not
    //  in a fade nor in the fade-in after one.
    bool take_input(std::string input){
        settle_fade();
        const size_t index = m_record.inputs++;
        if (m_last_fade_end && now() < *m_last_fade_end + SLOW_FADE_IN){
            m_record.inputs_after_fades++;
        }
        const bool fading = m_fade && now() >= m_fade->start;
        const bool locked = m_last_fade_end && now() < *m_last_fade_end + m_faults.fade_in;
        if (index == m_faults.dropped_input || fading || locked){
            return false;
        }
        m_moved = true;
        m_walk.facings.push_back(m_facing);
        m_walk.inputs.push_back(std::move(input));
        return true;
    }

    void start_fade(){
        const size_t index = m_record.fades++;
        const WallClock start = now() + FADE_START + m_faults.late_fade;
        m_fade = Fade{.start = start, .end = start + FADE_LENGTH, .moves = index != m_faults.no_op_fade, .to = step().name};
    }
    //  The fade, once over, moves the player.
    void settle_fade(){
        if (!m_fade || now() < m_fade->end){
            return;
        }
        if (m_fade->moves){
            arrive(m_fade->to);
        }
        m_last_fade_end = m_fade->end;
        m_fade.reset();
        m_moved = true;
    }

    //  A slice of the walk to the gate.
    void walk_toward_gate(double degrees, Milliseconds duration){
        if (!m_gate || sim_step(step()) != SimStep::GATE){
            return;
        }
        const double bearing = (double)std::get<int64_t>(step().args.at("heading"));
        if (std::abs(turn_between(m_facing + degrees, bearing)) > GATE_WALK_CONE){
            return;
        }
        *m_gate -= UNITS_PER_MS * (double)duration.count();
        if (*m_gate <= 0){
            m_gate.reset();
            arrive(step().name);
        }
    }

    //  A timed walk ends where the clean run's did on its inputs, at its
    //  facings (a funnel's within FUNNEL_TOLERANCE).
    bool walk_reached() const{
        if (m_spot != start_of(m_step)){
            return false;
        }
        if (m_clean == nullptr){
            return true;
        }
        const CleanRun::Walk& clean = m_clean->walks.at(step().name);
        if (clean.inputs != m_walk.inputs){
            return false;
        }
        const double tolerance = FUNNEL_WALKS.contains(step().name) ? FUNNEL_TOLERANCE : 1;
        for (size_t i = 0; i < clean.facings.size(); i++){
            if (std::abs(turn_between(clean.facings[i], m_walk.facings[i])) > tolerance){
                return false;
            }
        }
        return true;
    }

    const StoryCheckpoint& m_row;
    const SimFaults m_faults;
    const CleanRun* m_clean;
    CancellableScope& m_scope;
    const WallClock m_start;
    Milliseconds m_clock = 0ms;
    std::set<std::string, std::less<>> m_prompt_spots;
    size_t m_step = 0;
    std::string m_spot;
    double m_facing = 0;
    CleanRun::Walk m_walk;
    //  How far the walk to the gate has left; none when no gate is ahead.
    std::optional<double> m_gate;
    std::optional<Fade> m_fade;
    std::optional<WallClock> m_last_fade_end;
    //  An input was taken or a fade ended since the last look: the screen
    //  changed.
    bool m_moved = false;
    CleanRun m_record;
    std::vector<std::string> m_lines;
};

Observation<ZonePose> RowSim::observe(){
    settle_fade();
    Observation<ZonePose> look;
    look.when = now();
    if (m_fade && now() >= m_fade->start){
        look.events.set(ScreenEvent::FADE_BLACK, true);
        return look;
    }
    look.events.set(ScreenEvent::PLAYER_CAN_MOVE, true);
    look.events.set(ScreenEvent::SCREEN_STILL, !m_moved);
    m_moved = false;
    look.readings[std::string(Pokemon::STORY_HEADING)] = (int)std::lround(m_facing) % 360;
    if (prompt_shows()){
        look.events.set(ScreenEvent::INTERACT_PROMPT, true);
    }
    if (m_gate && sim_step(step()) == SimStep::GATE){
        const double off = turn_between(m_facing, (double)std::get<int64_t>(step().args.at("heading")));
        if (std::abs(off) <= GATE_IN_VIEW){
            look.readings[std::string(ZONE_GATE_X_READING)] = (int)std::lround(500 + off * 1000 / VIEW_DEGREES);
            look.readings[std::string(ZONE_GATE_WIDTH_READING)] = 150;
        }
    }
    return look;
}


//  The row `id`.
const StoryCheckpoint& row_of(std::string_view id){
    for (const StoryCheckpoint& row : story_checkpoints()){
        if (row.id == id){
            return row;
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("No row {}.", id));
}

//  How a row's run ended.
struct RowRun{
    std::optional<FaultSweepResult::Failure> failure;
    CleanRun record;
};

//  `step` alone through PLZA's engine on `sim`: its failure, if any.
std::optional<Pokemon::Outcome> run_played(const StoryCheckpoint& row, const StoryStep& step, RowSim& sim){
    const Pokemon::StoryGame<ZonePose> game = make_plza_story_game(StarterChoice::CHIKORITA);
    StoryCheckpoint one{.id = row.id, .title = row.title};
    one.steps.push_back(step);
    Pokemon::StoryEngine<ZonePose> engine(game, sim);
    Pokemon::RunState state;
    const Pokemon::StoryEngineReport report = engine.run_checkpoint(one, Pokemon::SavePoint::RUN_END, state);
    if (report.verdict == Pokemon::CheckpointVerdict::PASSED){
        return std::nullopt;
    }
    return report.run.failure;
}

//  The row's steps on `sim`: the ones it plays through PLZA's engine, each
//  alone, the others by the sim. Fails on a step that fails or on an end
//  elsewhere.
RowRun run_row(const StoryCheckpoint& row, const std::string& variant, RowSim& sim){
    for (size_t i = 0; i < row.steps.size(); i++){
        const StoryStep& step = row.steps[i];
        sim.begin_step(i);
        const std::optional<Pokemon::Outcome> failed = sim_step(step) == SimStep::NOT_PLAYED ? std::nullopt : run_played(row, step, sim);
        if (failed){
            return RowRun{.failure = FaultSweepResult::Failure{
                .step = step.name, .variant = variant, .stopped = Pokemon::to_string_with_class(*failed),
            }, .record = sim.record()};
        }
        sim.end_step();
    }
    const std::string& last = row.steps.back().name;
    const std::optional<std::string> off = sim.off_end(last);
    if (off){
        return RowRun{.failure = FaultSweepResult::Failure{.step = last, .variant = variant, .stopped = std::format("ended {}", *off)}, .record = sim.record()};
    }
    return RowRun{.failure = std::nullopt, .record = sim.record()};
}

//  The row's clean run; `not_swept` says why when it fails clean.
struct CleanResult{
    CleanRun run;
    std::optional<std::string> not_swept;
};
CleanResult clean_run(const StoryCheckpoint& row, CancellableScope& scope){
    RowSim sim(row, SimFaults{}, nullptr, scope);
    const RowRun run = run_row(row, "clean", sim);
    if (run.failure){
        return CleanResult{.run = run.record, .not_swept = std::format("fails clean at {}: {}", run.failure->step, run.failure->stopped)};
    }
    return CleanResult{.run = run.record, .not_swept = std::nullopt};
}

//  Each variant over `row_id`, after its clean run; `variants_of` lists
//  them from the clean run, with a reason when the fault has no chance.
using Variant = std::pair<std::string, SimFaults>;
FaultSweepResult sweep_row(
    const std::string& row_id,
    const std::function<std::vector<Variant>(const StoryCheckpoint&, const CleanRun&, std::optional<std::string>&)>& variants_of
){
    CancellableHolder<CancellableScope> scope;
    const StoryCheckpoint& row = row_of(row_id);
    const CleanResult clean = clean_run(row, scope);
    FaultSweepResult result;
    if (clean.not_swept){
        result.not_swept = clean.not_swept;
        return result;
    }
    std::optional<std::string> no_chance;
    const std::vector<Variant> variants = variants_of(row, clean.run, no_chance);
    if (no_chance){
        result.not_swept = no_chance;
        return result;
    }
    for (const Variant& variant : variants){
        RowSim sim(row, variant.second, &clean.run, scope);
        const RowRun run = run_row(row, variant.first, sim);
        result.variants++;
        if (run.failure){
            result.failures.push_back(*run.failure);
        }
    }
    return result;
}

std::vector<Variant> lost_tap_variants(const StoryCheckpoint&, const CleanRun& clean, std::optional<std::string>&){
    std::vector<Variant> variants;
    for (size_t i = 0; i < clean.inputs; i++){
        variants.emplace_back(std::format("input {}", i + 1), SimFaults{.dropped_input = i});
    }
    return variants;
}

std::vector<Variant> late_fade_variants(const StoryCheckpoint&, const CleanRun& clean, std::optional<std::string>& no_chance){
    if (clean.fades == 0){
        no_chance = "no fade the sim plays";
        return {};
    }
    return {{"every fade late", SimFaults{.late_fade = LATE_FADE}}};
}

std::vector<Variant> warp_no_op_variants(const StoryCheckpoint&, const CleanRun& clean, std::optional<std::string>& no_chance){
    if (clean.fades == 0){
        no_chance = "no fade the sim plays";
        return {};
    }
    std::vector<Variant> variants;
    for (size_t i = 0; i < clean.fades; i++){
        variants.emplace_back(std::format("fade {}", i + 1), SimFaults{.no_op_fade = i});
    }
    return variants;
}

std::vector<Variant> slow_fade_in_variants(const StoryCheckpoint&, const CleanRun& clean, std::optional<std::string>& no_chance){
    if (clean.inputs_after_fades == 0){
        no_chance = std::format("no input the sim plays comes within {} ms of a fade's end", SLOW_FADE_IN.count());
        return {};
    }
    return {{"every fade-in slow", SimFaults{.fade_in = SLOW_FADE_IN}}};
}

std::vector<Variant> scene_end_pose_variants(const StoryCheckpoint& row, const CleanRun&, std::optional<std::string>&){
    std::vector<std::string> ends{""};
    for (const StoryStep& step : row.steps){
        if (sim_step(step) == SimStep::PROMPT){
            ends.push_back(step.name);
        }
    }
    std::vector<Variant> variants;
    for (const std::string& end : ends){
        for (const double turn : {SCENE_END_TURN, -SCENE_END_TURN}){
            variants.emplace_back(
                std::format("{:+g} deg after {}", turn, end.empty() ? "the load" : end),
                SimFaults{.pose_off = SimFaults::PoseOff{.after = end, .turn = turn}}
            );
        }
    }
    return variants;
}

//  The injector of one sim fault, from its variants.
Pokemon::StoryFaultSweep::Injector injector(
    std::vector<Variant> (*variants_of)(const StoryCheckpoint&, const CleanRun&, std::optional<std::string>&)
){
    return [variants_of](Logger&, const std::string& row){
        return sweep_row(row, variants_of);
    };
}

std::shared_ptr<const Pokemon::StoryFaultSweep> plza_fault_sweep(){
    auto sweep = std::make_shared<Pokemon::StoryFaultSweep>();
    sweep->test_prefix = TEST_PREFIX;
    sweep->rows = SWEPT_ROWS;
    sweep->injectors = {
        {FaultClass::LOST_TAP, injector(lost_tap_variants)},
        {FaultClass::LATE_FADE, injector(late_fade_variants)},
        {FaultClass::WARP_NO_OP, injector(warp_no_op_variants)},
        {FaultClass::SLOW_FADE_IN, injector(slow_fade_in_variants)},
        {FaultClass::SCENE_END_POSE, injector(scene_end_pose_variants)},
    };
    sweep->known_failing = KNOWN_FAILING;
    return sweep;
}


//  A row's walk to a zone's gate through PLZA's engine, on looks facing
//  the step's heading with no gate on screen: the failure line carries the
//  class of PLZA's own failure name.
struct GateClassCase{
    std::string name;
    struct Input{
        std::string row;
    };
    Input input;
    struct Expectation{
        //  In a failure line of the run's log.
        std::string failure;
        std::string tag;
    };
    Expectation expectation;
};

const GateClassCase GATE_CLASS_CASES[] = {
    {
        .name = "03.07d's missed gate is logged as class=pose_lost",
        .input = {.row = "03.07d"},
        .expectation = {.failure = "gate_not_seen", .tag = "(class=pose_lost)"},
    },
};

UnitTestResult check_gate_class(const GateClassCase& test, CancellableScope& scope){
    const StoryCheckpoint& row = row_of(test.input.row);
    StoryCheckpoint one{.id = row.id, .title = row.title};
    for (const StoryStep& step : row.steps){
        if (sim_step(step) == SimStep::GATE){
            one.steps.push_back(step);
        }
    }
    if (one.steps.empty()){
        return std::format("{} has no walk to a gate.", test.input.row);
    }
    Observation<ZonePose> look;
    look.when = current_time();
    look.events.set(ScreenEvent::PLAYER_CAN_MOVE, true);
    look.readings[std::string(Pokemon::STORY_HEADING)] = (int)std::get<int64_t>(one.steps[0].args.at("heading"));
    Pokemon::FakeStoryBackend<ZonePose> backend(Pokemon::FakeStoryScript<ZonePose>{
        .looks = {look}, .battle = Pokemon::outcome_ok(), .kind = Pokemon::StoryBackendKind::SCRIPT,
    }, scope);
    const Pokemon::StoryGame<ZonePose> game = make_plza_story_game(StarterChoice::CHIKORITA);
    Pokemon::StoryEngine<ZonePose> engine(game, backend);
    Pokemon::RunState state;
    engine.run_checkpoint(one, Pokemon::SavePoint::RUN_END, state);
    for (const std::string& line : backend.lines()){
        if (line.find(test.expectation.failure) != std::string::npos && line.find(test.expectation.tag) != std::string::npos){
            return true;
        }
    }
    return std::format("No log line with {} {}.", test.expectation.failure, test.expectation.tag);
}

class Test_GateClass : public UnitTest{
public:
    explicit Test_GateClass(const GateClassCase& test)
        : UnitTest(std::format("{} - Failure class: {}", TEST_PREFIX, test.name))
        , m_test(test)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope& scope) const override{
        return check_gate_class(m_test, scope);
    }

private:
    const GateClassCase& m_test;
};

} // namespace


void add_tests_AutoStoryFaultSweep(UnitTestDatabase& database){
    Pokemon::add_fault_sweep_tests(database, plza_fault_sweep());
    for (const GateClassCase& test : GATE_CLASS_CASES){
        database.add<Test_GateClass>(test);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
