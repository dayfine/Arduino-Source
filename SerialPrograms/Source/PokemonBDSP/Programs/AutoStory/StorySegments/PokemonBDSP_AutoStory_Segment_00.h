/*  BDSP AutoStory - Segment 00
 *  From beginning cinematic until after choosing the starter Pokemon.
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_Segment00_H
#define PokemonAutomation_PokemonBDSP_AutoStory_Segment00_H

#include <vector>
#include "Pokemon/Navigation/Pokemon_Grid.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStoryTools.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_StoryTables.h"

namespace PokemonAutomation {
namespace NintendoSwitch {
namespace PokemonBDSP {


// Segment 00 as a table of checkpoints: 00.00 to 00.05, with 00.04 in two
// rows (00.04 to the catch, 00.04b after it), each saving at its start.
Pokemon::Segment make_segment_00(StoryStepRunner& runner);

// DIAGNOSTIC ONLY: Remove before pushing to production.
// Starts at the post-catch save and runs only the rest of checkpoint 00.04.
void diagnostic_checkpoint_0004_after_catch(SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    AutoStoryOptions options,
    AutoStoryStats& stats);

// DIAGNOSTIC ONLY: Remove before pushing to production.
// 00.04's walk after the Potions, for the event-model diagnostic
// (PokemonBDSP_AutoStory_SandgemCenterEvents.h): from below Sandgem's
// Pokemon Center to Route 202's save spot, without saving. Fails with
// a screenshot as 00.04 does. `at`: the tiles in Sandgem we may stand on.
void walk_from_sandgem_center_to_route_202_save_spot(
    VideoStream& stream, ProControllerContext& context,
    const std::vector<Pokemon::TilePos>& at
);

class AutoStory_Segment_00 : public AutoStory_Segment {
public:
    virtual std::string name() const override;
    virtual std::string start_text() const override;
    virtual std::string end_text() const override;
    virtual void run_segment(SingleSwitchProgramEnvironment& env,
        ProControllerContext& context,
        AutoStoryOptions options,
        AutoStoryStats& stats) const override;
};

class AutoStory_Checkpoint_0000 : public AutoStory_Checkpoint {
public:
    virtual std::string name() const override;
    virtual std::string start_text() const override;
    virtual std::string end_text() const override;
    virtual void run_checkpoint(SingleSwitchProgramEnvironment& env,
        ProControllerContext& context,
        AutoStoryOptions options,
        AutoStoryStats& stats) const override;
};

class AutoStory_Checkpoint_0001 : public AutoStory_Checkpoint {
public:
    virtual std::string name() const override;
    virtual std::string start_text() const override;
    virtual std::string end_text() const override;
    virtual void run_checkpoint(SingleSwitchProgramEnvironment& env,
        ProControllerContext& context,
        AutoStoryOptions options,
        AutoStoryStats& stats) const override;
};

class AutoStory_Checkpoint_0002 : public AutoStory_Checkpoint {
public:
    virtual std::string name() const override;
    virtual std::string start_text() const override;
    virtual std::string end_text() const override;
    virtual void run_checkpoint(SingleSwitchProgramEnvironment& env,
        ProControllerContext& context,
        AutoStoryOptions options,
        AutoStoryStats& stats) const override;
};

class AutoStory_Checkpoint_0003 : public AutoStory_Checkpoint {
public:
    virtual std::string name() const override;
    virtual std::string start_text() const override;
    virtual std::string end_text() const override;
    virtual void run_checkpoint(SingleSwitchProgramEnvironment& env,
        ProControllerContext& context,
        AutoStoryOptions options,
        AutoStoryStats& stats) const override;
};

class AutoStory_Checkpoint_0004 : public AutoStory_Checkpoint {
public:
    virtual std::string name() const override;
    virtual std::string start_text() const override;
    virtual std::string end_text() const override;
    virtual void run_checkpoint(SingleSwitchProgramEnvironment& env,
        ProControllerContext& context,
        AutoStoryOptions options,
        AutoStoryStats& stats) const override;
};

class AutoStory_Checkpoint_0005 : public AutoStory_Checkpoint {
public:
    virtual std::string name() const override;
    virtual std::string start_text() const override;
    virtual std::string end_text() const override;

    virtual void run_checkpoint(SingleSwitchProgramEnvironment& env,
        ProControllerContext& context,
        AutoStoryOptions options,
        AutoStoryStats& stats) const override;
};

// start: game launched, in front of the TV.
// end:   Standing outside lake varety before starter
//Segment 00-00 begins
void checkpoint_000(SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    EventNotificationOption& notif_status_update,
    AutoStoryStats& stats);

// start: Standing outside lake varety before starter
// end:   Standing outside lake varety after starter battle
//Segment 00-001 begins
void checkpoint_001(SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    EventNotificationOption& notif_status_update,
    AutoStoryStats& stats);

// start: Standing outside lake varety after starter battle
// end:   recieved Pokedex, standing in front of the professor
//Segment 00-002 begins
void checkpoint_002(SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    EventNotificationOption& notif_status_update,
    AutoStoryStats& stats);

// start: recieved Pokedex, standing in front of the professor
// end:   Standing in the entrance to Jubilife City after speaking to dawn
//Segment 00-003 begins
void checkpoint_003(SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    EventNotificationOption& notif_status_update,
    AutoStoryStats& stats);

// start: Standing in the entrance to Jubilife City after speaking to dawn
// end:   Standing in the Pokemon Center ready to deposit pokemon after recieving poketech.
//Segment 00-004 begins
void checkpoint_004(SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    EventNotificationOption& notif_status_update,
    AutoStoryStats& stats);
}
}
}
#endif
