/*  BDSP AutoStory - Segment 00
 *  From beginning cinematic until after choosing the starter Pokemon.
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_Segment00_H
#define PokemonAutomation_PokemonBDSP_AutoStory_Segment00_H

#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStoryTools.h"

namespace PokemonAutomation {
namespace NintendoSwitch {
namespace PokemonBDSP {


// DIAGNOSTIC ONLY: Remove before pushing to production.
// Starts at the post-catch save and runs only the rest of checkpoint 00.04.
void diagnostic_checkpoint_0004_after_catch(SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    AutoStoryOptions options,
    AutoStoryStats& stats);

// DIAGNOSTIC ONLY: Remove before pushing to production.
// 00.04's steps after the Potions, one at a time, for the event-model
// diagnostic (PokemonBDSP_AutoStory_SandgemCenterEvents.h). Each fails with
// a screenshot as 00.04 does.
//  From below Sandgem's Pokemon Center door: to Route 202's save spot, without saving.
void walk_from_sandgem_center_to_route_202_save_spot(VideoStream& stream, ProControllerContext& context);
//  From Route 202's save spot: north to Jubilife, fighting trainers who see us.
void walk_route_202_to_jubilife(VideoStream& stream, ProControllerContext& context);
//  Into Jubilife and through Dawn's scene.
void meet_dawn_in_jubilife(VideoStream& stream, ProControllerContext& context);

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
