/*  AutoStory Row Sim
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  FRLG's story on the simulation, through the same action table as the
 *  console (make_story_action_table()): each binding plays the actions it
 *  can and stubs the rest, which need the console. StoryMapsSim runs the
 *  whole story on the story maps; OverworldStepSim walks a row's steps on
 *  the OverworldSim, for the simulation tests of each stage's rows.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryRowSim_H
#define PokemonAutomation_PokemonFRLG_AutoStoryRowSim_H

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>
#include "Pokemon/AutoStory/Pokemon_StoryActions.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldRoute.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldSim.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryStepInterrupts.h"
#include "PokemonFRLG_AutoStoryStepTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The steps of one attempt at a row that a simulation did not play.
struct RowStubs{
    std::string row;
    std::vector<std::string> steps;
};

//  The steps a simulation stubs, by row: the stand-in it binds to each
//  action it cannot play.
class SimStubs{
public:
    //  An attempt at `row` begins; its stubs are listed apart from the last.
    void begin_row(std::string row);
    //  Needs the console; adds the step to the row's stubs, then does
    //  `then` (what the simulation keeps of the step: where pacing leaves
    //  the player), or passes with the pose as it was.
    Pokemon::StoryAction stub(Pokemon::StoryActionFunction then = {});
    const std::vector<RowStubs>& rows() const{
        return m_rows;
    }

private:
    void record(const StoryStep& step);

    std::vector<RowStubs> m_rows;
};

//  Each attempt's stubs, a line a row: "00.03: 2 stubbed: oak, rival".
std::string stub_report(const std::vector<RowStubs>& rows);

//  A failed step fails the run as it threw: a simulation has no save to
//  reload.
class SimStepGuard : public Pokemon::StepGuard{
public:
    virtual bool stops_on_failure(const Pokemon::StoryStep&) const override{
        return true;
    }
};


//  What happens on the story maps besides the story's steps.
struct StorySimKnobs{
    //  Whether a dialog pops up right before row `row`'s step `step`:
    //  someone walks up and talks between two steps. Until it is pressed
    //  through, it stops a step that walks or talks, as on the console.
    std::function<bool(const std::string& row, const std::string& step)> dialog_before = nullptr;
    //  Whether the executor clears what came up between steps
    //  (step_interrupts()), as the console does; off, as before it did.
    bool clear_interrupts = true;
};

//  The whole story on the story maps, without a walker: each ROUTE planned
//  and followed from where the run stands (turning to a wall when the
//  facing is unknown, as the program does), each LEGS laid out, a step
//  into a trainer's line taken until a trainer can see the player, a
//  READY's way to the nurse planned, the Viridian Mart's orders checked
//  against its list, and a GAME leaving the player where pacing would
//  (paced_pose()). The rest is stubbed. The dialogs `knobs` pops up are
//  the only interrupts between steps it shows.
class StoryMapsSim : public Pokemon::TableExecutor{
public:
    //  The table and guard are members, built after the base: it only keeps
    //  references to them until a step runs.
    explicit StoryMapsSim(StorySimKnobs knobs = {});
    StoryMapsSim(const StoryMapsSim&) = delete;
    void operator=(const StoryMapsSim&) = delete;

    virtual void begin_attempt(const Pokemon::StoryCheckpoint& data) override;
    //  The step, after a dialog pops up before it if the knobs say so.
    virtual Pokemon::Outcome run(Pokemon::RunState& state, const StoryStep& step) override;
    //  The story's actions as this simulation binds them.
    const Pokemon::StoryActionTable& actions() const{
        return m_table;
    }
    const SimStubs& stubs() const{
        return m_stubs;
    }
    //  What the Mart sold, by its list's names.
    const std::map<std::string, int>& bag() const{
        return m_bag;
    }
    //  "00.03/oak: a dialog pops up before it.", and how it was cleared.
    const std::vector<std::string>& dialog_notes() const{
        return m_dialog_notes;
    }

protected:
    //  A dialog that popped up and is still up stops a step that walks or
    //  talks; a step that presses through text closes it.
    virtual Pokemon::Outcome run_action(Pokemon::RunState& state, const StoryStep& step) override;

private:
    Pokemon::StoryAction action(StoryActionId id);
    Pokemon::Outcome walk_to(const Pokemon::Pose& pose, const StoryStep& step) const;
    Pokemon::Outcome legs(const Pokemon::Pose& pose, const StoryStep& step) const;
    Pokemon::Outcome step_ahead(const Pokemon::Pose& pose, const StoryStep& step) const;
    Pokemon::Outcome shop(const StoryStep& step);
    //  The screen between steps: the dialog that popped up, if one did.
    Pokemon::ScreenLook look();
    StepInterruptActions sim_interrupt_actions();

    SimStepGuard m_guard;
    SimStubs m_stubs;
    std::map<std::string, int> m_bag;
    Pokemon::StoryActionTable m_table;
    StorySimKnobs m_knobs;
    Pokemon::StoryInterrupts m_interrupts;
    std::string m_row;
    //  A dialog popped up (StorySimKnobs::dialog_before) and is still up;
    //  it rose since the last look.
    bool m_dialog_up = false;
    bool m_dialog_rose = false;
    std::vector<std::string> m_dialog_notes;
};


//  The route the program walks for `step`, a ROUTE or LEGS, from `at`:
//  planned, or the legs laid out.
std::optional<std::vector<RouteStep>> walked_route(const StoryPose& at, const StoryStep& step);

//  A row's steps on the OverworldSim as the program does them: routes and
//  legs by the sim's walker from where the table says the player stands,
//  fighting a trainer who sees the player on the way; a step one tap at a
//  time, or for a step into a trainer's line by step_until_seen(), each
//  watched for the trainer; a battle from the trainer's message, which
//  must be up; a cut tree cut and stepped onto. The rest is stubbed.
class OverworldStepSim : public Pokemon::TableExecutor{
public:
    //  `sim` must outlive the executor.
    OverworldStepSim(OverworldSim& sim, std::string row, StoryPose at);
    OverworldStepSim(const OverworldStepSim&) = delete;
    void operator=(const OverworldStepSim&) = delete;

    //  Where the table says the player stands: moved to where it says each
    //  step ends.
    const StoryPose& at() const{
        return m_at;
    }
    const Pokemon::StoryActionTable& actions() const{
        return m_table;
    }
    const SimStubs& stubs() const{
        return m_stubs;
    }

private:
    Pokemon::StoryAction action(StoryActionId id);
    std::string walk(const StoryStep& step);
    std::string step_ahead(const StoryStep& step);
    std::string cut(const StoryStep& step);
    std::string battle(const StoryStep& step);

    OverworldSim& m_sim;
    StoryPose m_at;
    SimStepGuard m_guard;
    SimStubs m_stubs;
    Pokemon::StoryActionTable m_table;
};

//  Runs `step` of row `id` on `sim` through OverworldStepSim, from `at`,
//  and moves `at` to where the table says the step ends. Returns the
//  failure, "<id>/<step>: <why> On <where>.", or empty.
std::string row_step_on_sim(OverworldSim& sim, StoryPose& at, const StoryStep& step, const std::string& id);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
