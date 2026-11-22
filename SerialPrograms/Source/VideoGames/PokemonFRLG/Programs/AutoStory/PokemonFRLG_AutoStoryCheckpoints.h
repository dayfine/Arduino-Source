/*  AutoStory Checkpoints
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  FRLG's story checkpoint data, run plan, and handoff types.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryCheckpoints_H
#define PokemonAutomation_PokemonFRLG_AutoStoryCheckpoints_H

#include <optional>
#include <set>
#include <string>
#include <vector>
#include "Pokemon/AutoStory/Engine/Pokemon_StorySavePolicy.h"
#include "Pokemon/AutoStory/Types/Pokemon_GridPose.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryPostconditions.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryVerify.h"
#include "Pokemon/Navigation/Grid/Planning/Pokemon_GatedTypes.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMap.h"
#include "VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_BlindNavigation.h"
#include "PokemonFRLG_AutoStoryActions.h"
#include "PokemonFRLG_AutoStoryStepTypes.h"
#include "PokemonFRLG_AutoStorySupplies.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  An upstream RNG program the user may run part-way through the story.
//  Each is off unless its option is on.
enum class StoryHandoff{
    NONE,
    SID_HELPER,
    STARTER_RNG,
    GIFT_RNG,
};

//  Lead must survive `steps` of overworld poison damage (1 HP per 4 steps)
//  at the end of a checkpoint.
struct PoisonSurvivalPostcondition{
    int steps = 0;
    int max_hp = 0;

    bool operator==(const PoisonSurvivalPostcondition&) const = default;
};

//  An authored FRLG story checkpoint with its parsed steps, start pose,
//  execution settings, phase (S12), and postconditions.
struct StoryCheckpoint{
    std::string id = {};
    std::string title = {};
    std::optional<Pokemon::GridPose> start = std::nullopt;
    std::vector<StoryStep> steps = {};
    Pokemon::GateState start_gates = {};
    std::set<std::string> start_flags = {};
    Pokemon::CheckpointTestingStatus phase = Pokemon::CheckpointTestingStatus::TESTING;
    Pokemon::StoryPostconditions postconditions = {};
    std::optional<Pokemon::StoryVerify> verify = std::nullopt;
    bool dev_only = false;
    bool autosave = false;
    bool reload_when_chained = false;
    //  The healing items at its start, for a run that loads its save.
    Supplies supplies = {};
    //  The START menu at its end, when it saves.
    StartMenu menu = StartMenu::FULL;
    //  Starts from the game as another program left it, loading no save.
    bool live_start = false;
    //  The RNG program the user may run at this checkpoint.
    StoryHandoff handoff = StoryHandoff::NONE;
    //  With GIFT_RNG: the gift Gift RNG takes there.
    PokemonFRLG_RngTarget gift = PokemonFRLG_RngTarget::magikarp;
    //  Optional overworld poison survival postcondition at checkpoint end.
    std::optional<PoisonSurvivalPostcondition> poison_survival = std::nullopt;
};

//  The pose the start check of `data` compares the loaded save with:
//  nothing for a new game or a live-start row.
std::optional<Pokemon::GridPose> checked_start_pose(const StoryCheckpoint& data);

//  Where `checkpoint` ends: its postconditions' position, else the last
//  step's pose_after or target. Unset if a trailing legs step leaves it unplaced.
std::optional<Pokemon::GridPose> checkpoint_end_pose(const StoryCheckpoint& checkpoint);
inline Pokemon::GateState checkpoint_gates(const StoryCheckpoint& checkpoint, size_t steps_done){
    Pokemon::GateState gates = checkpoint.start_gates;
    const size_t done = std::min(steps_done, checkpoint.steps.size());
    for (size_t c = 0; c < done; c++){
        if (checkpoint.steps[c].gates_after){
            Pokemon::apply_gate_change(gates, *checkpoint.steps[c].gates_after);
        }
    }
    return gates;
}

//  The handoffs a run stops for (AutoStory's options).
struct StoryHandoffs{
    bool sid_helper = false;
    bool starter_rng = false;
    bool gift_rng = false;
};

//  What a run from table row `first` to `last` does.
struct StoryRunPlan{
    std::vector<StoryCheckpoint> rows = {};
    StoryHandoff stop = StoryHandoff::NONE;
};

StoryRunPlan plan_story_run(
    const std::vector<StoryCheckpoint>& checkpoints, size_t first, size_t last, const StoryHandoffs& handoffs
);

//  A way the player at `tile` can turn to without stepping: a wall there,
//  with no one standing in it.
std::optional<Direction> wall_beside(const MapData& map, const TilePos& tile);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
