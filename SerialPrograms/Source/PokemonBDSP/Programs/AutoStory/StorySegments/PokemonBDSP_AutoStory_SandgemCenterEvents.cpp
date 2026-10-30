/*  BDSP AutoStory - Route 202 to Sandgem's Pokemon Center, as events
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/VisualDetectors/BlackScreenDetector.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "PokemonBDSP/Inference/PokemonBDSP_DialogDetector.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_Route202.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_SandgemTown.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_StoryMaps.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStoryTools.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Location.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_RouteWalk.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Segment_00_tools.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Walk.h"
#include "PokemonBDSP_AutoStory_SandgemCenterEvents.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace std::chrono_literals;
using Pokemon::Checkpoint;
using Pokemon::Condition;
using Pokemon::Direction;
using Pokemon::Event;
using Pokemon::Interrupt;
using Pokemon::Outcome;
using Pokemon::Pose;
using Pokemon::PoseCertainty;
using Pokemon::Recovery;
using Pokemon::RecoveryKind;
using Pokemon::RunState;
using Pokemon::TilePos;
using Pokemon::condition;
using Pokemon::outcome_failed;
using Pokemon::outcome_ok;
using Pokemon::pose_known;


const char* const ROUTE202_TO_SANDGEM_CENTER_NAME = "Route 202 to Sandgem's Pokemon Center (events)";

TilePos sandgem_entry_stop(){
    return story_maps().to_world(MAP_SANDGEM_TOWN, {29, 2});
}

TilePos below_sandgem_center_sign(){
    return story_maps().to_world(MAP_SANDGEM_TOWN, SandgemTown::POKEMON_CENTER_SIGN.next(Direction::SOUTH));
}

bool center_doorway_at_front_row(const LitDoorway& doorway){
    //  From below the sign the door is three tiles west; a tile either way
    //  for where the walk stopped, and the rows as tiles_short_of_door()
    //  counts the front row (1.5).
    return doorway.rows_up > 0 && doorway.rows_up <= 1.5 &&
        doorway.tiles_across >= -4.5 && doorway.tiles_across <= 1.5;
}

bool inside_past_door(bool screen_shows, const std::optional<LitDoorway>& doorway){
    if (!screen_shows){
        return false;
    }
    return !doorway || !(doorway->rows_up > 0 && doorway->rows_up <= 1.5 && std::abs(doorway->tiles_across) <= 1);
}


namespace{

//  The pose of a player who may be on any of `tiles`: on their map if they
//  all share one.
Pose pose_on(const std::vector<TilePos>& tiles, std::optional<Direction> facing = std::nullopt){
    const Pokemon::MapSet& maps = story_maps();
    Pose pose;
    pose.tiles = tiles;
    pose.facing = facing;
    if (tiles.empty()){
        return pose;
    }
    const std::optional<Pokemon::MapTile> first = maps.to_local(tiles.front());
    const bool one_map = first && std::all_of(tiles.begin(), tiles.end(), [&](const TilePos& pos){
        const std::optional<Pokemon::MapTile> tile = maps.to_local(pos);
        return tile && tile->map == first->map;
    });
    pose.map = one_map ? first->map : "";
    pose.certainty = tiles.size() == 1 ? PoseCertainty::TILE : PoseCertainty::SET;
    return pose;
}

Recovery recovery(std::string on, RecoveryKind kind, size_t attempts, Pokemon::Act prepare, std::string why){
    Recovery recovery;
    recovery.on = std::move(on);
    recovery.kind = kind;
    recovery.attempts = attempts;
    recovery.prepare = std::move(prepare);
    recovery.why = std::move(why);
    return recovery;
}

Condition center_doorway_ahead(SandgemCenterActions& game){
    return condition("the Center's doorway at the front row", [&game](RunState&){
        const std::optional<LitDoorway> doorway = game.lit_doorway();
        return doorway && center_doorway_at_front_row(*doorway);
    });
}

Condition inside_center(SandgemCenterActions& game){
    return condition("inside, past the door", [&game](RunState&){
        return inside_past_door(game.screen_shows(), game.lit_doorway());
    });
}

const char* const FLAG_THROUGH_DOOR = "through_center_door";


//  1. The save the checkpoint starts from is Route 202's save spot: the
//  save screen's card says which map, and its walls pin the tile.
Event confirm_on_route_202(SandgemCenterActions& game){
    std::shared_ptr<std::string> read = std::make_shared<std::string>();
    Event event;
    event.name = "confirm_on_route_202";
    event.why = "the save must stand at Route 202's save spot";
    event.act = [&game, read](RunState&){
        *read = game.save_screen_location();
        return outcome_ok();
    };
    event.expect.push_back(condition("the save screen reads Route 202", [read](RunState&){
        return location_banner_matches(*read, MAP_ROUTE_202);
    }));
    event.pose_learned = pose_on({story_maps().to_world(MAP_ROUTE_202, Route202::SAVE_SPOT)}, Direction::EAST);
    event.recoveries.push_back(recovery("expect_failed", RecoveryKind::WAIT, 1, nullptr,
        "OCR may have misread the card; read it again"));
    return event;
}

//  2. South along the trees east of Route 202, into Sandgem Town. Column
//  30 has no tall grass, and no trainer's sight reaches it below the ledge.
//  walk_route() fails unless Sandgem's banner shows on the way.
Event walk_to_sandgem(SandgemCenterActions& game){
    Event event;
    event.name = "walk_to_sandgem";
    event.why = "south out of Route 202, past Sandgem's banner";
    event.skip_if.push_back(pose_known(MAP_SANDGEM_TOWN, PoseCertainty::SET));
    event.require.push_back(pose_known(MAP_ROUTE_202, PoseCertainty::SET));
    event.act = [&game](RunState& state){
        std::vector<TilePos> at = state.pose.tiles;
        const TilePos stop = sandgem_entry_stop();
        const Outcome walked = game.walk(at, [stop](const TilePos& pos){ return pos == stop; }, "Route 202 to Sandgem Town");
        if (walked.succeeded()){
            state.pose = pose_on(at, Direction::SOUTH);
        }
        return walked;
    };
    event.expect.push_back(pose_known(MAP_SANDGEM_TOWN, PoseCertainty::SET));
    //  From anywhere on Route 202 under the ledge, or Sandgem's top rows,
    //  east to the trees and south to the wall ends at the same tile.
    event.recoveries.push_back(recovery("", RecoveryKind::REANCHOR, 1, [&game](RunState& state){
        if (!game.east_then_south_into_sandgem()){
            return outcome_failed("no_sandgem_banner", "east to the trees and south showed no Sandgem Town banner");
        }
        state.pose = pose_on({sandgem_entry_stop()}, Direction::SOUTH);
        return outcome_ok();
    }, "east to the trees and south to the wall, watching for Sandgem's banner"));
    return event;
}

//  3. To below the Center's sign, where 00.04's heal walks to; the Center's
//  doorway is then in sight three tiles west. The doors' row has no wall to
//  stop on, so the plan is 00.04's: round the east and south of town on
//  walls, then west along the house and north up the sign's column, whose
//  sign stops us (13 legs). Someone in the way stops an open leg short
//  without the walk knowing: the doorway, further up the screen than the
//  front row, then says where we are.
Event walk_to_center_door(SandgemCenterActions& game){
    Event event;
    event.name = "walk_to_center_door";
    event.why = "to the row below the Center's door, its doorway in sight";
    event.skip_if.push_back(center_doorway_ahead(game));
    event.require.push_back(pose_known(MAP_SANDGEM_TOWN, PoseCertainty::SET));
    event.act = [&game](RunState& state){
        std::vector<TilePos> at = state.pose.tiles;
        const TilePos goal = below_sandgem_center_sign();
        const Outcome walked = game.walk(at, [goal](const TilePos& pos){ return pos == goal; }, "To the Pokemon Center");
        if (walked.succeeded()){
            state.pose = pose_on(at);
        }
        return walked;
    };
    event.expect.push_back(center_doorway_ahead(game));
    event.recoveries.push_back(recovery("expect_failed", RecoveryKind::REANCHOR, 2, [&game](RunState& state){
        const std::optional<LitDoorway> doorway = game.lit_doorway();
        if (!doorway){
            return outcome_failed("no_doorway", "no lit doorway on screen to place us by");
        }
        const Pokemon::MapSet& maps = story_maps();
        const TilePos door = maps.to_world(MAP_SANDGEM_TOWN, SandgemTown::POKEMON_CENTER_DOOR);
        const std::vector<TilePos> tiles = tiles_by_lit_doorway(*doorway, maps.world(), door);
        if (tiles.empty()){
            return outcome_failed("no_doorway", "the doorway puts us on no standable tile");
        }
        state.pose = pose_on(tiles);
        return outcome_ok();
    }, "short of the door: place us by the doorway and walk again"));
    return event;
}

//  4. In through the door. enter_lit_door() lines up with the doorway by
//  sight and walks in; its fade is not always black enough to read, so
//  inside is checked after: not black, and no doorway straight ahead.
Event enter_center(SandgemCenterActions& game){
    Event event;
    event.name = "enter_center";
    event.why = "through the Center's door to the inside";
    event.skip_if.push_back(condition("went through the door", [](RunState& state){
        return state.flags.count(FLAG_THROUGH_DOOR) > 0;
    }));
    event.skip_if.push_back(inside_center(game));
    event.act = [&game](RunState& state){
        const Outcome entered = game.enter_lit_door();
        if (entered.succeeded()){
            state.flags.insert(FLAG_THROUGH_DOOR);
        }
        return entered;
    };
    event.expect.push_back(inside_center(game));
    event.recoveries.push_back(recovery("expect_failed", RecoveryKind::WAIT, 1, [&game](RunState&){
        //  The fade may still be on: look again in a moment.
        return game.screen_shows() ? outcome_ok() : outcome_failed("fade_on", "the screen is still black");
    }, "the inside may not have drawn yet"));
    return event;
}

//  A townsperson who talks stops a walk where it stands, and walk_route()
//  fails on a talk no trainer explains: close it and run the event again,
//  from wherever its recoveries can place us.
Interrupt dialogue_interrupt(SandgemCenterActions& game){
    Interrupt interrupt;
    interrupt.name = "dialogue";
    interrupt.detect = [&game](RunState&){ return game.dialogue_showing(); };
    interrupt.handle = [&game](RunState&){ return game.finish_dialogue(); };
    interrupt.pose = Pokemon::InterruptPose::LOST;
    interrupt.resume = Pokemon::ResumePolicy::RETRY_EVENT;
    return interrupt;
}


//  The console's side of the checkpoint.
class ConsoleSandgemCenterActions : public SandgemCenterActions{
public:
    ConsoleSandgemCenterActions(VideoStream& stream, ProControllerContext& context)
        : m_stream(stream)
        , m_context(context)
        , m_trainers(story_trainer_sights())
    {}

    virtual std::string save_screen_location() override{
        return read_location_from_save_screen(m_stream, m_context);
    }

    virtual Outcome walk(
        std::vector<TilePos>& at,
        const std::function<bool(const TilePos&)>& goal,
        const std::string& label
    ) override{
        //  No trainer sees this way; one who does is a failure to look at.
        const RouteTrainerFight fight = [this](const std::string& trainer){
            OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                "Trainer " + trainer + " saw us on a way no trainer should see.", m_stream);
        };
        try{
            at = walk_route(m_stream, m_context, story_maps(), at, goal, m_trainers, fight, label);
            return outcome_ok();
        }catch (OperationFailedException& e){
            return outcome_failed("walk_failed", e.message());
        }
    }

    virtual bool east_then_south_into_sandgem() override{
        bool saw_sandgem = false;
        const auto banner = [&saw_sandgem](const ImageViewRGB32& screen){
            saw_sandgem = saw_sandgem || location_banner_matches(read_location_banner(screen), MAP_SANDGEM_TOWN);
            return false;
        };
        const WalkResult east = walk_until(m_stream, m_context, 1, 0, 6000ms, "East to the trees");
        const WalkResult south = walk_until(m_stream, m_context, 0, -1, 9000ms, "South to the wall", banner);
        for (const WalkResult& walk : {east, south}){
            if (walk.stop == WalkStop::TRANSITION || walk.stop == WalkStop::DIALOG){
                route_status(m_stream, std::string("Re-anchor: ") + walk_stop_name(walk.stop) + " instead of a wall.");
                return false;
            }
        }
        return saw_sandgem;
    }

    virtual std::optional<LitDoorway> lit_doorway() override{
        m_context.wait_for_all_requests();
        VideoSnapshot screen = m_stream.video().snapshot();
        return screen ? find_lit_doorway(*screen.frame) : std::nullopt;
    }

    virtual Outcome enter_lit_door() override{
        try{
            PokemonBDSP::enter_lit_door(m_stream, m_context, "Sandgem Pokemon Center: through the door");
            return outcome_ok();
        }catch (OperationFailedException& e){
            return outcome_failed("door_failed", e.message());
        }
    }

    virtual bool screen_shows() override{
        m_context.wait_for_all_requests();
        VideoSnapshot screen = m_stream.video().snapshot();
        return screen && !BlackScreenDetector().detect(*screen.frame);
    }

    virtual bool dialogue_showing() override{
        m_context.wait_for_all_requests();
        VideoSnapshot screen = m_stream.video().snapshot();
        return screen && ShortDialogDetector().detect(*screen.frame);
    }

    virtual Outcome finish_dialogue() override{
        try{
            PokemonBDSP::finish_dialogue(m_stream, m_context, BUTTON_B, "Sandgem: a talk on the way");
            return outcome_ok();
        }catch (OperationFailedException& e){
            return outcome_failed("dialogue_failed", e.message());
        }
    }

private:
    VideoStream& m_stream;
    ProControllerContext& m_context;
    std::vector<Pokemon::TrainerSight> m_trainers;
};

}


Checkpoint route202_to_sandgem_center_checkpoint(SandgemCenterActions& game){
    Checkpoint checkpoint;
    checkpoint.id = "sandgem_center";
    checkpoint.interrupts.push_back(dialogue_interrupt(game));
    checkpoint.events.push_back(confirm_on_route_202(game));
    checkpoint.events.push_back(walk_to_sandgem(game));
    checkpoint.events.push_back(walk_to_center_door(game));
    checkpoint.events.push_back(enter_center(game));
    //  No save: the save stays on Route 202, so the run can be repeated.
    return checkpoint;
}

void diagnostic_route202_to_sandgem_center(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    //  The doorways are found by their lit colour: pin the clock as 00.04
    //  does, so a reloaded save sees the same lighting.
    set_time_fast(env, context, DateTime{2026, 1, 1, 0, 0, 50});
    pbf_wait(context, 1500ms);
    context.wait_for_all_requests();

    ConsoleSandgemCenterActions game(env.console, context);
    Checkpoint checkpoint = route202_to_sandgem_center_checkpoint(game);
    const std::vector<std::string> problems = Pokemon::lint_checkpoint(checkpoint);
    for (const std::string& problem : problems){
        env.log("[Checkpoint] lint: " + problem, COLOR_RED);
    }
    RunState state;
    Pokemon::EventLogger log = [&env](const std::string& line){
        env.log(line, COLOR_BLUE);
        add_wrapped_log(env.console.overlay(), line, COLOR_WHITE);
    };
    const Pokemon::CheckpointReport report = Pokemon::run_checkpoint(state, checkpoint, log);
    if (!report.completed){
        const std::string where = report.failed_event == 0 ? "" : " at event " +
            std::to_string(report.failed_event) + " " + report.failed_name;
        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
            std::string(ROUTE202_TO_SANDGEM_CENTER_NAME) + ": failed" + where + ": " +
            report.failure.failure + (report.failure.detail.empty() ? "" : ": " + report.failure.detail), env.console);
    }
}



}
}
}
