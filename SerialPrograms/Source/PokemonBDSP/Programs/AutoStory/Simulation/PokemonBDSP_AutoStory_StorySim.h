/*  BDSP AutoStory - Story simulation
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Rows of the story table run as the program runs them (the segment
 *  runner, make_story_segment()), with the program's own walking steps
 *  (StoryWalkSteps: ROUTE, LEGS, STEP, TALK, MASH) on the grid simulation.
 *  After each walking step the simulated player must stand, on the grid,
 *  on a tile the table gives; a step through a door must have warped.
 *
 *  Events, battles, shops and saves are stubs: a dialogue they open is
 *  closed, and the table's pose_after puts the player where they end. A
 *  heal checks that a walk north from where we stand enters a door. A scene
 *  starts when the player steps onto one of the maps' coord events while
 *  its story variable holds (story_vars()). Rooms with no
 *  map (the Trainers' School) are passed over. Each row must start where
 *  the row before left the player.
 *
 *  A tap or hold through a door or cave mouth stops on its tile, as the
 *  console stops at the fade; a hold that does not see the fade goes on
 *  onto the step's pose_after and on along it, and a location banner reads
 *  only a few tiles into its map (SIM_BANNER_TILES).
 *
 *  With a SimDisturbance the run is disturbed as the console disturbs it
 *  (wild battles, drift off the grid, people wandering): the story's Monte
 *  Carlo tests.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_StorySim_H
#define PokemonAutomation_PokemonBDSP_AutoStory_StorySim_H

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Pokemon/AutoStory/Pokemon_StorySegments.h"
#include "Pokemon/AutoStory/Pokemon_StorySteps.h"
#include "PokemonBDSP_AutoStory_GridSim.h"
#include "PokemonBDSP_AutoStory_SimDisturbance.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_CoordEvents.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryPlaces.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryStepIO.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryTable.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryWalkSteps.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Which detections the simulation misses on purpose (#162): the `fade`th
//  door fade (run_into_door(), from 0) or the `banner`th map banner
//  (run_until_map()) of the run goes unseen, though the player did go
//  through. The console missed both (r75, r81). The `dark`th door run reads
//  dark SIM_FALSE_DARK in, short of the door, as lab-B's did on 2026-11-08.
//  The `through`th tap step (tap_to_banner(), from 0) starts on its far side,
//  its banner gone unless `through_banner`: the walk before it slid onto the
//  warp (lab-B 2026-11-08). The counts and flags below are the run's: how
//  many fades and banners it looked for, whether a miss happened since the
//  executor last looked, and whether a missed fade is still unseen.
struct SimMisses{
    std::optional<size_t> fade;
    std::optional<size_t> banner;
    std::optional<size_t> dark = {};
    std::optional<size_t> through = {};
    bool through_banner = false;
    size_t tap_steps = 0;
    size_t fades = 0;
    size_t banners = 0;
    size_t doors = 0;
    bool missed = false;
    bool fade_unseen = false;
};

//  How many tiles a held walk goes on into a map before its location
//  banner reads: "it shows a tile or a few past the border" (the table's
//  route_201_by_the_path(), measured on the console). Standing still, it
//  reads at once. Through a cave mouth the fade comes first, and a hold
//  whose fade went unseen goes on the same way (r75, r81).
constexpr size_t SIM_BANNER_TILES = 3;
//  How far into a door run SimMisses::dark reads dark short of the door.
constexpr Milliseconds SIM_FALSE_DARK = Milliseconds(1800);

//  What a run on the simulation did.
struct SimRun{
    //  The first failure, as "row outcome", or "".
    std::string error;
    //  The rows and steps where an injected miss happened and where the run
    //  first failed, as "row step": "" if none.
    std::string missed_step;
    std::string failed_step;
    //  Walking steps that end where nothing says, as "row step".
    std::vector<std::string> unchecked;
    //  Each row run, with the fades and banners looked for before it.
    struct RowStart{
        std::string row;
        size_t fades;
        size_t banners;
    };
    std::vector<RowStart> rows;
    //  Where the player stood after the last step, if placed.
    const StoryWorld* world = nullptr;
    Pokemon::TilePos end;
    //  Wild battles in the run, and in the step it failed in.
    size_t battles = 0;
    size_t failed_step_battles = 0;
    //  How the run failed: the failure and its message.
    std::string failure;
    std::string failure_detail;
};

//  StoryStepIO on the simulation. A walk through a door or a cave mouth
//  stops on its tile, as the console stops at the fade; a hold that misses
//  the fade carries on, onto `far_side()` (the step's pose_after: the
//  sim's maps have no warps' destinations) and on along it.
class SimStoryStepIO : public StoryStepIO{
public:
    SimStoryStepIO(GridSim& sim, std::vector<std::string>& notes, SimMisses& misses, std::function<bool()> far_side,
        SimDisturbance* noise = nullptr)
        : m_sim(sim)
        , m_noise(noise)
        , m_route(sim, notes, noise)
        , m_misses(misses)
        , m_far_side(std::move(far_side))
    {}

    virtual RouteIO& route() override{
        return m_route;
    }
    virtual void fight(const std::string&) override{
        m_sim.end_dialogue();
    }
    virtual void walk_until_stopped(Pokemon::Direction direction, size_t tiles, const std::string& label) override;
    virtual bool run_until_map(Pokemon::Direction direction, Milliseconds limit, const std::string& map, const std::string&) override;
    virtual bool run_into_door(Pokemon::Direction direction, bool run, Milliseconds timeout = Milliseconds(12000), Milliseconds dark_from = Milliseconds(0)) override{
        const SimPace pace = run ? SimPace::RUN : SimPace::WALK;
        if (false_dark() && dark_from <= SIM_FALSE_DARK && SIM_FALSE_DARK < timeout){
            disturbed_hold(m_sim, m_noise, direction, SIM_FALSE_DARK, pace);
            return true;
        }
        //  A wild battle on the way is fled, and the run goes on.
        const bool warped = disturbed_hold(m_sim, m_noise, direction, timeout, pace).move.stop == SimStop::WARP;
        if (warped && fade_missed()){
            m_misses.fade_unseen = true;
            return false;
        }
        return warped;
    }
    //  A scene may take us to another map (Mom walks us out, 00.04): with a
    //  dialogue open the stub scene plays it. Otherwise only a door fades.
    //  A fade missed on the way in stays missed: it was over before settle()
    //  looked, which then fails as it does on the console after 120 s.
    virtual void settle(Button, const std::string& label, bool fade) override{
        const bool scene_fades = m_sim.player().dialog && m_far_side();
        if (fade && ((!m_sim.player().warped && !scene_fades) || m_misses.fade_unseen)){
            m_route.note(label + ": no fade, so we are not where the step goes (map mismatch).", RouteNote::FAILURE);
        }
        m_misses.fade_unseen = false;
        m_sim.end_dialogue();
    }
    virtual std::optional<size_t> tap_to_banner(Pokemon::Direction direction, const std::string& map, size_t max_taps) override;

private:
    //  Whether the run misses this fade or this banner (SimMisses).
    bool fade_missed(){
        const bool miss = m_misses.fade == m_misses.fades++;
        m_misses.missed |= miss;
        return miss;
    }
    bool false_dark(){
        const bool dark = m_misses.dark == m_misses.doors++;
        m_misses.missed |= dark;
        return dark;
    }
    bool banner_missed(){
        const bool miss = m_misses.banner == m_misses.banners++;
        m_misses.missed |= miss;
        return miss;
    }
    bool on_map(const std::string& map) const{
        return story_to_local(m_sim.world(), map, nearest_tile(m_sim.player().pos)).has_value();
    }

    GridSim& m_sim;
    SimDisturbance* m_noise;
    DisturbedRouteIO m_route;
    SimMisses& m_misses;
    std::function<bool()> m_far_side;
};

//  A scene's trigger the simulation fires from row `row` on, once the step
//  `after` has run ("" from the row's start), on `map`'s `tiles`.
struct RowTrigger{
    std::string row;
    std::string after;
    std::string map;
    SimTrigger trigger;
};

//  When a story variable holds the value its coord events wait for: from
//  row `row` on, once the step `after` has run ("" from the row's start).
struct VarState{
    std::string var;
    int value;
    std::string row;
    std::string after;
};

//  The story variables whose coord events the table walks onto (see the
//  .cpp), and the maps' coord events they arm.
std::vector<VarState> story_vars();
//  The coord events of the maps whose story variable story_vars() holds.
std::vector<RowTrigger> story_triggers();

//  The program's walking steps on the simulation, the rest stubbed. With
//  `noise`, which must outlive it, the run is disturbed: the player may
//  stand part of a tile off (within half a tile of where a step should end
//  counts as there), and the people on the maps wander.
class SimExecutor : public Pokemon::StoryStepExecutor{
public:
    explicit SimExecutor(Logger& logger, SimMisses misses = {}, SimDisturbance* noise = nullptr)
        : m_logger(logger)
        , m_misses(misses)
        , m_noise(noise)
        , m_sim({}, {0, 0}, Pokemon::Direction::NORTH, false)
        , m_io(m_sim, m_notes, m_misses, [this]{ return cross(); }, noise)
        , m_walks(m_io)
    {}

    const SimMisses& misses() const{
        return m_misses;
    }
    const SimRun& result() const{
        return m_result;
    }
    virtual void begin_attempt(const Pokemon::StoryCheckpoint& data) override{
        m_row = data.id;
        m_start_mismatch = data.start ? off_pose(*data.start) : "";
        m_ran.clear();
        m_result.rows.push_back({.row = data.id, .fades = m_misses.fades, .banners = m_misses.banners});
        m_walks.begin_attempt(data.id);
        arm("");
    }
    virtual Pokemon::Outcome run(Pokemon::RunState& state, const Pokemon::StoryStep& step) override;

protected:
    virtual Pokemon::Outcome route(Pokemon::RunState& state, const Pokemon::StoryStep& step) override{ return m_walks.route(state, step); }
    virtual Pokemon::Outcome legs(Pokemon::RunState& state, const Pokemon::StoryStep& step) override{ return m_walks.legs(state, step); }
    virtual Pokemon::Outcome step(Pokemon::RunState& state, const Pokemon::StoryStep& step) override{ return m_walks.step(state, step); }
    virtual Pokemon::Outcome talk(Pokemon::RunState& state, const Pokemon::StoryStep& step) override{ return m_walks.talk(state, step); }
    virtual Pokemon::Outcome mash(Pokemon::RunState& state, const Pokemon::StoryStep& step) override{ return m_walks.mash(state, step); }
    virtual Pokemon::Outcome heal(Pokemon::RunState& state, const Pokemon::StoryStep& step) override;
    virtual Pokemon::Outcome new_game(Pokemon::RunState&, const Pokemon::StoryStep&) override{ return stub(); }
    virtual Pokemon::Outcome options(Pokemon::RunState&, const Pokemon::StoryStep&) override{ return stub(); }
    virtual Pokemon::Outcome answer(Pokemon::RunState&, const Pokemon::StoryStep&) override{ return stub(); }
    virtual Pokemon::Outcome battle(Pokemon::RunState&, const Pokemon::StoryStep&) override{ return stub(); }
    virtual Pokemon::Outcome train(Pokemon::RunState&, const Pokemon::StoryStep&) override{ return stub(); }
    virtual Pokemon::Outcome shop(Pokemon::RunState&, const Pokemon::StoryStep&) override{ return stub(); }
    virtual Pokemon::Outcome save(Pokemon::RunState&, const Pokemon::StoryStep&) override{ return stub(); }
    virtual Pokemon::Outcome game(Pokemon::RunState&, const Pokemon::StoryStep&) override{ return stub(); }

private:
    //  A step the simulation does not play: its dialogue opened and closed.
    Pokemon::Outcome stub(){
        m_sim.end_dialogue();
        return Pokemon::outcome_ok();
    }
    const StoryWorld* sim_world(const std::string& map) const;
    SimWorld setup(const StoryWorld* world) const;
    void arm(const std::string& after);
    bool sync(const Pokemon::Pose& pose, const StoryWorld* on = nullptr);
    bool cross();
    std::string off_pose(const Pokemon::Pose& pose) const;
    std::string check(const Pokemon::StoryStep& step, const Pokemon::Outcome& outcome);
    Pokemon::Outcome run_step(Pokemon::RunState& state, const Pokemon::StoryStep& step);
    void place(const StoryWorld* world, const Pokemon::TilePos& start, std::optional<Pokemon::Direction> facing);
    bool settled() const;
    void record_failure(const Pokemon::Outcome& outcome, const Pokemon::StoryStep& step, size_t battles_before);

    Logger& m_logger;
    std::vector<std::string> m_notes;
    SimMisses m_misses;
    SimDisturbance* m_noise;
    GridSim m_sim;
    SimStoryStepIO m_io;
    StoryWalkSteps m_walks;
    //  The planner's worlds with sim_people() on them, by world and who.
    mutable std::map<std::pair<const StoryWorld*, std::string>, std::unique_ptr<StoryWorld>> m_peopled;
    //  The steps of this row that have run.
    std::set<std::string> m_ran;
    std::string m_row;
    std::vector<RowTrigger> m_armed;
    //  Where the step running now ends, if it says: a door's far side.
    std::optional<Pokemon::Pose> m_far;
    //  Why the player is not on this row's start, or "".
    std::string m_start_mismatch;
    //  Whether the player has been put on a map yet.
    bool m_placed = false;
    //  Whether the step running now is on a room with no map.
    bool m_passed_over = false;
    SimRun m_result;
};

//  Runs `range` of `rows` (the table unless given) on `executor`, stopping
//  at the first failure.
Pokemon::SegmentReport run_rows_on(
    SimExecutor& executor, Logger& logger, const Pokemon::SegmentRange& range,
    const std::vector<StoryCheckpoint>& rows = story_checkpoints()
);
//  The table with row `row`'s step of `step`'s name replaced by `step`.
std::vector<StoryCheckpoint> table_with(const std::string& row, const Pokemon::StoryStep& step);



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
