/*  BDSP AutoStory - Story tables
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A segment of the story as a table of checkpoints on the shared engine
 *  (Pokemon_StorySegments.h). Each checkpoint is the list of step functions
 *  it called in order before it was a table, each step one event. The
 *  checkpoint saves first, as checkpoint_reattempt_loop() did before its
 *  first attempt; a step that fails is answered the way that loop answered
 *  it: a screenshot, a reset to the save, and the checkpoint again from its
 *  first step, up to the same 20 attempts.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_StoryTables_H
#define PokemonAutomation_PokemonBDSP_AutoStory_StoryTables_H

#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "Pokemon/Navigation/Pokemon_StorySegments.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStoryTools.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  What a step runs on.
struct StoryConsole{
    SingleSwitchProgramEnvironment& env;
    ProControllerContext& context;
    const AutoStoryOptions& options;
    AutoStoryStats& stats;
};
using StoryStep = std::function<void(StoryConsole& console)>;

struct StoryStepSpec{
    std::string name;
    std::string why;
    StoryStep step;
};

//  What a checkpoint does when a step fails.
enum class StoryFailure{
    //  Save before the first step; on a failure reset to that save and run
    //  the checkpoint again (checkpoint_reattempt_loop with save=true).
    RELOAD_AND_RETRY,
    //  No save and no retry: the failure ends the program (00.00).
    STOP,
};

struct StoryCheckpointSpec{
    std::string id;
    std::string title;
    StoryFailure on_failure = StoryFailure::RELOAD_AND_RETRY;
    std::vector<StoryStepSpec> steps;
};

//  How a table's events reach the game: the console runner below, or a
//  fake in the tests, which only describe the tables.
class StoryStepRunner{
public:
    virtual ~StoryStepRunner() = default;
    //  Whether checkpoint `id` has saved at its start in this run; a retry
    //  starts from that save and does not save again.
    virtual bool saved_at_start(const std::string& id) const = 0;
    virtual Pokemon::Outcome save_at_start(const std::string& id) = 0;
    virtual Pokemon::Outcome run_step(const std::string& id, StoryFailure on_failure, const StoryStep& step) = 0;
};

//  A row of a segment table: builds the checkpoint's events from `spec`.
Pokemon::CheckpointSpec story_checkpoint(StoryStepRunner& runner, StoryCheckpointSpec spec);

using StorySegmentFactory = std::function<Pokemon::Segment(StoryStepRunner& runner)>;


//  Runs tables on the Switch.
class ConsoleStoryRunner : public StoryStepRunner{
public:
    explicit ConsoleStoryRunner(StoryConsole console);

    virtual bool saved_at_start(const std::string& id) const override;
    virtual Pokemon::Outcome save_at_start(const std::string& id) override;
    virtual Pokemon::Outcome run_step(const std::string& id, StoryFailure on_failure, const StoryStep& step) override;

    //  Runs the rows of `segment` in `range`; throws as the checkpoint
    //  loop did once a checkpoint has used up its attempts.
    void run(Pokemon::Segment& segment, const Pokemon::SegmentRange& range);

private:
    Pokemon::Outcome reload();

    StoryConsole m_console;
    std::set<std::string> m_saved;
    std::map<std::string, size_t> m_failures;
};

//  Builds the table with a console runner and runs the checkpoints from
//  `range.from` to `range.to` (the whole segment when both are unset).
void run_story_table(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context,
    const AutoStoryOptions& options, AutoStoryStats& stats,
    const StorySegmentFactory& make_segment, const Pokemon::SegmentRange& range
);



}
}
}
#endif
