/*  BDSP AutoStory - Route 202 to Sandgem's Pokemon Center, as events
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>
#include "Common/Cpp/PrettyPrint.h"
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/Language.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/OCR/OCR_RawTesseractOCR.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "PokemonBDSP/Inference/PokemonBDSP_DialogDetector.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_Route201.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_Route202.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_SandgemTown.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_StoryMaps.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStoryTools.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Location.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_RouteWalk.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Segment_00_tools.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Walk.h"
#include "PokemonBDSP/Programs/AutoStory/StorySegments/PokemonBDSP_AutoStory_Segment_00.h"
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

bool doorway_lined_up(const LitDoorway& doorway){
    return doorway.rows_up > 0 && doorway.rows_up <= 1.5 && std::abs(doorway.tiles_across) <= 0.5;
}

bool center_doorway_at_front_row(const LitDoorway& doorway){
    //  From below the sign the door is three tiles west; a tile either way
    //  for where the walk stopped, and the rows as tiles_short_of_door()
    //  counts the front row (1.5).
    return doorway.rows_up > 0 && doorway.rows_up <= 1.5 &&
        doorway.tiles_across >= -4.5 && doorway.tiles_across <= 1.5;
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

Condition outside(SandgemCenterActions& game){
    return condition("outside, not in a Pokemon Center", [&game](RunState&){
        return !game.inside_pokemon_center();
    });
}

//  The doorway alone cannot tell the Center's from the Mart's, so the pose
//  must also be in front of the Center: on its doors' row or a few rows
//  south, within a few columns of its door.
Condition center_doorway_ahead(SandgemCenterActions& game){
    return condition("the Center's doorway at the front row", [&game](RunState& state){
        const TilePos door = story_maps().to_world(MAP_SANDGEM_TOWN, SandgemTown::POKEMON_CENTER_DOOR);
        const bool near = state.pose.map == MAP_SANDGEM_TOWN && !state.pose.tiles.empty() &&
            std::all_of(state.pose.tiles.begin(), state.pose.tiles.end(), [&](const TilePos& pos){
                return std::abs(pos.x - door.x) <= 4 && pos.y > door.y && pos.y <= door.y + 4;
            });
        if (!near){
            return false;
        }
        const std::optional<LitDoorway> doorway = game.lit_doorway();
        return doorway && center_doorway_at_front_row(*doorway);
    });
}

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

//  Places us by the doorway in sight, the Center's. With `mart_if_west`
//  (after east_then_north_in_sandgem()), a doorway west of us is the
//  Mart's: east then north ends at the trees east of it, or in the west of
//  town, west of the Center's door.
Outcome place_by_doorway(SandgemCenterActions& game, RunState& state, bool mart_if_west){
    const std::optional<LitDoorway> doorway = game.lit_doorway();
    if (!doorway){
        return outcome_failed("no_doorway", "no lit doorway on screen to place us by");
    }
    const Pokemon::MapSet& maps = story_maps();
    const bool mart = mart_if_west && doorway->tiles_across < 0;
    const TilePos door = maps.to_world(MAP_SANDGEM_TOWN, mart ? SandgemTown::POKE_MART_DOOR : SandgemTown::POKEMON_CENTER_DOOR);
    const std::vector<TilePos> tiles = tiles_by_lit_doorway(*doorway, maps.world(), door);
    if (tiles.empty()){
        return outcome_failed("no_doorway", "the doorway puts us on no standable tile");
    }
    state.pose = pose_on(tiles);
    return outcome_ok();
}

//  3. To below the Center's sign, where 00.04's heal walks to; the Center's
//  doorway is then in sight three tiles west. The doors' row has no wall to
//  stop on, so the plan is 00.04's: round the east and south of town on
//  walls, then west along the house and north up the sign's column, whose
//  sign stops us (13 legs). Someone in the way stops an open leg short
//  without the walk knowing: the doorway, further up the screen than the
//  front row, then says where we are.
//  A walk that fails on its way (a leg to a wall that is not there, r127a)
//  is re-anchored on the doors' row by the doorway in sight, and planned
//  again; one that went through a door is not.
Event walk_to_center_door(SandgemCenterActions& game, std::string name){
    Event event;
    event.name = std::move(name);
    event.why = "to the row below the Center's door, its doorway in sight";
    event.skip_if.push_back(center_doorway_ahead(game));
    event.require.push_back(pose_known("", PoseCertainty::SET));
    event.act = [&game](RunState& state){
        std::vector<TilePos> at = state.pose.tiles;
        const TilePos goal = below_sandgem_center_sign();
        const Outcome walked = game.walk(at, [goal](const TilePos& pos){ return pos == goal; }, "To the Pokemon Center");
        if (walked.succeeded()){
            state.pose = pose_on(at);
        }
        return walked;
    };
    event.expect.push_back(outside(game));
    event.expect.push_back(center_doorway_ahead(game));
    event.recoveries.push_back(recovery("expect_failed", RecoveryKind::REANCHOR, 2, [&game](RunState& state){
        return place_by_doorway(game, state, false);
    }, "short of the door: place us by the doorway and walk again"));
    event.recoveries.push_back(recovery("walk_failed", RecoveryKind::REANCHOR, 1, [&game](RunState& state){
        const Outcome anchored = game.east_then_north_in_sandgem();
        return anchored.succeeded() ? place_by_doorway(game, state, true) : anchored;
    }, "lost on the way: east and north to the doors' row, placed by the doorway there"));
    return event;
}

//  4. In front of the door. From below the sign the doorway is three tiles
//  west: step across by its offset, as enter_lit_door() does, without going
//  in or turning north (a turn there took r127c in). The expects look after
//  the last move has settled: still outside, the doorway straight ahead.
//  The tile below the door is learned.
Event at_center_door(SandgemCenterActions& game, std::string name){
    Event event;
    event.name = std::move(name);
    event.why = "in front of the Center's door";
    event.act = [&game](RunState&){ return game.line_up_with_door(); };
    event.expect.push_back(outside(game));
    event.expect.push_back(condition("the doorway straight ahead, a row up", [&game](RunState&){
        const std::optional<LitDoorway> doorway = game.lit_doorway();
        return doorway && doorway_lined_up(*doorway);
    }));
    event.pose_learned = pose_on({story_maps().to_world(MAP_SANDGEM_TOWN,
        SandgemTown::POKEMON_CENTER_DOOR.next(Direction::SOUTH))});
    event.recoveries.push_back(recovery("expect_failed", RecoveryKind::WAIT, 1, [&game](RunState&){
        //  Inside, lining up again would not get us out.
        return game.inside_pokemon_center() ? outcome_failed("went_in", "a Pokemon Center's inside shows") : outcome_ok();
    }, "someone may have stood in the way: look and line up again"));
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
            //  Through a door: nothing to re-anchor on in there.
            const bool through_door = e.message().find("transition") != std::string::npos;
            return outcome_failed(through_door ? "walk_transition" : "walk_failed", e.message());
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

    virtual Outcome line_up_with_door() override{
        if (!line_up_with_lit_door(m_stream, m_context, "Sandgem Pokemon Center: in front of the door")){
            return outcome_failed("not_lined_up", "no lit doorway, inside, or not lined up after 6 steps");
        }
        return outcome_ok();
    }

    virtual bool inside_pokemon_center() override{
        m_context.wait_for_all_requests();
        VideoSnapshot screen = m_stream.video().snapshot();
        return screen && pokemon_center_inside(*screen.frame);
    }

    virtual Outcome east_then_north_in_sandgem() override{
        const WalkResult east = walk_until(m_stream, m_context, 1, 0, 6000ms, "Sandgem: east to a wall");
        const WalkResult north = walk_until(m_stream, m_context, 0, 1, 6000ms, "Sandgem: north to a wall");
        for (const WalkResult& walk : {east, north}){
            if (walk.stop != WalkStop::BLOCKED){
                return outcome_failed("not_anchored", std::string("Sandgem: ") + walk_stop_name(walk.stop) + " instead of a wall");
            }
        }
        return outcome_ok();
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

protected:
    VideoStream& m_stream;
    ProControllerContext& m_context;
    std::vector<Pokemon::TrainerSight> m_trainers;
};


// ---------------------------------------------------------------------------
//  The Poke Mart lady
// ---------------------------------------------------------------------------

//  A planned walk on the story maps to the tile `goal`, from where the pose
//  says we are on `from_map`.
Event walk_to_tile(MartLadyActions& game, std::string name, std::string why, const char* from_map, TilePos goal){
    Event event;
    event.name = std::move(name);
    event.why = std::move(why);
    event.require.push_back(pose_known(from_map, PoseCertainty::SET));
    const std::string label = event.why;
    event.act = [&game, goal, label](RunState& state){
        std::vector<TilePos> at = state.pose.tiles;
        const Outcome walked = game.walk(at, [goal](const TilePos& pos){ return pos == goal; }, label);
        if (walked.succeeded()){
            state.pose = pose_on(at);
        }
        return walked;
    };
    event.expect.push_back(condition("on " + story_maps().describe(goal), [goal](RunState& state){
        return state.pose.certainty == PoseCertainty::TILE && state.pose.tiles.front() == goal;
    }));
    return event;
}

//  The gate: from the tile west of Route 201's border sign, facing east, A
//  reads it, and B closes it. Only that tile reads the sign, so the read
//  confirms where we stand. A walk that stopped a tile short is put right by
//  walking east until the sign stops us.
Event read_route_201_sign(MartLadyActions& game, std::string name){
    std::shared_ptr<DialogueRead> read = std::make_shared<DialogueRead>();
    Event event;
    event.name = std::move(name);
    event.why = "read Route 201's border sign from the tile west of it";
    event.act = [&game, read](RunState&){
        *read = game.talk(Direction::EAST, BUTTON_B, "Route 201 sign");
        return outcome_ok();
    };
    event.expect.push_back(condition("the sign's dialogue opened and closed", [read](RunState&){
        return read->opened && read->closed;
    }));
    event.pose_learned = pose_on({route201_sign_west()}, Direction::EAST);
    event.recoveries.push_back(recovery("expect_failed", RecoveryKind::REANCHOR, 1, [&game](RunState&){
        return game.walk_until_stopped(Direction::EAST, 2, "East to the sign");
    }, "not at the sign: east until it stops us"));
    return event;
}

//  From the sign, north to the trees and west along the top row of her
//  grass until she stops us, a tile east of her; then one A per line.
//  She may have wandered: walk on west and ask again.
Event talk_to_mart_lady(MartLadyActions& game){
    std::shared_ptr<DialogueRead> read = std::make_shared<DialogueRead>();
    Event event;
    event.name = "talk_to_mart_lady";
    event.why = "the Poke Mart lady on Route 201 talks (she gave Potions to new games)";
    event.require.push_back(pose_known(MAP_ROUTE_201, PoseCertainty::SET));
    event.act = [&game, read](RunState&){
        const Outcome north = game.walk_until_stopped(Direction::NORTH, 1, "North to the trees");
        if (!north.succeeded()){
            return north;
        }
        const Outcome west = game.walk_until_stopped(Direction::WEST, 10, "West along the lady's row");
        if (!west.succeeded()){
            return west;
        }
        *read = game.talk(Direction::WEST, BUTTON_A, "Poke Mart lady");
        return outcome_ok();
    };
    event.expect.push_back(condition("a dialogue opened and was closed", [read](RunState&){
        return read->opened && read->closed;
    }));
    event.pose_learned = pose_on({story_maps().to_world(MAP_ROUTE_201, Route201::POTION_GIVER_TALK_FROM)}, Direction::WEST);
    event.recoveries.push_back(recovery("expect_failed", RecoveryKind::WAIT, 2, nullptr,
        "she may have wandered: walk on west and ask again"));
    return event;
}

//  Back to the sign from the lady's grass. A wild battle in the grass
//  stops an open leg somewhere: north to the trees and east to Sandgem's
//  fence pins the row's east end, and the walk is planned again from there.
Event back_to_route_201_sign(MartLadyActions& game){
    Event event = walk_to_tile(game, "back_to_route_201_sign", "Back to the Route 201 sign", MAP_ROUTE_201, route201_sign_west());
    event.recoveries.push_back(recovery("", RecoveryKind::REANCHOR, 1, [&game](RunState& state){
        const Outcome north = game.walk_until_stopped(Direction::NORTH, 12, "North to the trees");
        if (!north.succeeded()){
            return north;
        }
        const Outcome east = game.walk_until_stopped(Direction::EAST, 25, "East to Sandgem's fence");
        if (!east.succeeded()){
            return east;
        }
        state.pose = pose_on({route201_row_3_east_end()}, Direction::EAST);
        return outcome_ok();
    }, "north to the trees and east to Sandgem's fence"));
    return event;
}

//  A step of 00.04 that Segment 00 walks as one function, wrapped as an
//  event: done once `flag` is set.
Event wrapped_step(std::string name, std::string why, std::string flag, Pokemon::Act step){
    Event event;
    event.name = std::move(name);
    event.why = std::move(why);
    event.skip_if.push_back(condition(flag, [flag](RunState& state){ return state.flags.count(flag) > 0; }));
    event.act = [flag, step = std::move(step)](RunState& state) mutable{
        const Outcome done = step(state);
        if (done.succeeded()){
            state.flags.insert(flag);
        }
        return done;
    };
    event.expect.push_back(condition(flag, [flag](RunState& state){ return state.flags.count(flag) > 0; }));
    return event;
}


//  The console's side of the Mart lady checkpoint.
class ConsoleMartLadyActions : public MartLadyActions{
public:
    ConsoleMartLadyActions(SingleSwitchProgramEnvironment& env, ProControllerContext& context)
        : m_env(env)
        , m_context(context)
        , m_center(env.console, context)
    {}

    virtual std::string save_screen_location() override{ return m_center.save_screen_location(); }
    virtual Outcome walk(
        std::vector<TilePos>& at,
        const std::function<bool(const TilePos&)>& goal,
        const std::string& label
    ) override{
        return m_center.walk(at, goal, label);
    }
    virtual bool east_then_south_into_sandgem() override{ return m_center.east_then_south_into_sandgem(); }
    virtual std::optional<LitDoorway> lit_doorway() override{ return m_center.lit_doorway(); }
    virtual Outcome line_up_with_door() override{ return m_center.line_up_with_door(); }
    virtual bool inside_pokemon_center() override{ return m_center.inside_pokemon_center(); }
    virtual Outcome east_then_north_in_sandgem() override{ return m_center.east_then_north_in_sandgem(); }
    virtual bool dialogue_showing() override{ return m_center.dialogue_showing(); }
    virtual Outcome finish_dialogue() override{ return m_center.finish_dialogue(); }

    virtual Outcome walk_until_stopped(Direction direction, size_t tiles, const std::string& label) override{
        const bool across = direction == Direction::EAST || direction == Direction::WEST;
        const double x = direction == Direction::EAST ? 1 : direction == Direction::WEST ? -1 : 0;
        const double y = direction == Direction::NORTH ? 1 : direction == Direction::SOUTH ? -1 : 0;
        //  Time for every tile, and for the wall to read as still.
        const Milliseconds limit = ROUTE_START_DELAY + 800ms +
            (int64_t)tiles * (across ? ROUTE_MS_PER_TILE_ACROSS : ROUTE_MS_PER_TILE_UP_DOWN);
        const WalkResult walked = walk_until(m_env.console, m_context, x, y, limit, label);
        if (walked.stop == WalkStop::TRANSITION || walked.stop == WalkStop::DIALOG){
            return outcome_failed("walk_failed", label + ": " + walk_stop_name(walked.stop) + " instead of a stop");
        }
        return outcome_ok();
    }

    virtual DialogueRead talk(Direction direction, Button next, const std::string& label) override{
        VideoStream& stream = m_env.console;
        DialogueRead read;
        route_turn(stream, m_context, direction);
        pbf_press_button(m_context, BUTTON_A, 80ms, 0ms);
        m_context.wait_for_all_requests();
        ShortDialogWatcher opened(COLOR_CYAN);
        if (wait_until(stream, m_context, 2000ms, {{opened}}) != 0){
            stream.log("[Talk] " + label + ": no dialogue opened.");
            return read;
        }
        read.opened = true;
        //  The first box's text, inside its white margins.
        if (VideoSnapshot screen = stream.video().snapshot()){
            read.text = OCR::tesseract_ocr_read(Language::English, extract_box_reference(*screen.frame, ImageFloatBox(0.23, 0.80, 0.56, 0.16)));
        }
        stream.log("[Talk] " + label + ": the dialogue reads \"" + read.text + "\".");
        //  One press per line, 1.5 s apart, and only while a box shows: a
        //  press after it has closed would start the talk over.
        size_t quiet = 0;
        for (size_t presses = 0; presses < 20 && quiet < 2; presses++){
            ShortDialogWatcher dialog(COLOR_YELLOW);
            if (wait_until(stream, m_context, 1500ms, {{dialog}}) == 0){
                quiet = 0;
                pbf_press_button(m_context, next, 80ms, 0ms);
            }else{
                quiet++;
            }
        }
        read.closed = !dialogue_showing();
        stream.log("[Talk] " + label + (read.closed ? ": closed." : ": still open."));
        return read;
    }

    virtual Outcome walk_to_route_202_save_spot() override{
        return wrap("walk_failed", [&]{ walk_from_sandgem_center_to_route_202_save_spot(m_env.console, m_context); });
    }
    virtual Outcome save_on_route_202() override{
        bool saved = false;
        const Outcome outcome = wrap("save_failed", [&]{ saved = save_game(m_env, m_context, MAP_ROUTE_202); });
        if (outcome.succeeded() && !saved){
            return outcome_failed("save_failed", "the save screen did not read Route 202, or the save did not finish");
        }
        return outcome;
    }

private:
    static Outcome wrap(const char* failure, const std::function<void()>& step){
        try{
            step();
            return outcome_ok();
        }catch (OperationFailedException& e){
            return outcome_failed(failure, e.message());
        }
    }

    SingleSwitchProgramEnvironment& m_env;
    ProControllerContext& m_context;
    ConsoleSandgemCenterActions m_center;
};


void save_end_screenshot(SingleSwitchProgramEnvironment& env, const std::string& name){
    if (VideoSnapshot screen = env.console.video().snapshot()){
        const std::string folder = SCREENSHOTS_PATH() + "AutoStory/route/";
        Filesystem::create_directories(Filesystem::Path(folder));
        const std::string path = folder + now_to_filestring() + "-" + name + ".png";
        screen.frame->save(path);
        env.log("[Checkpoint] Screenshot: " + path);
    }
}

void pin_clock(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    //  The doorways are found by their lit colour: pin the clock as 00.04
    //  does, so a reloaded save sees the same lighting.
    set_time_fast(env, context, DateTime{2026, 1, 1, 0, 0, 50});
    pbf_wait(context, 1500ms);
    context.wait_for_all_requests();
}

//  Runs `checkpoint`, logging its events, and fails with a screenshot
//  naming the event and why if it does not complete.
void run_on_console(SingleSwitchProgramEnvironment& env, Checkpoint& checkpoint, const char* name){
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
    //  Where it ended, to check by eye.
    save_end_screenshot(env, "end-" + checkpoint.id);
    if (!report.completed){
        const std::string where = report.failed_event == 0 ? "" : " at event " +
            std::to_string(report.failed_event) + " " + report.failed_name;
        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
            std::string(name) + ": failed" + where + ": " +
            report.failure.failure + (report.failure.detail.empty() ? "" : ": " + report.failure.detail), env.console);
    }
}

}


Checkpoint route202_to_sandgem_center_checkpoint(SandgemCenterActions& game){
    Checkpoint checkpoint;
    checkpoint.id = "sandgem_center";
    checkpoint.interrupts.push_back(dialogue_interrupt(game));
    checkpoint.events.push_back(confirm_on_route_202(game));
    checkpoint.events.push_back(walk_to_sandgem(game));
    checkpoint.events.push_back(walk_to_center_door(game, "walk_to_center_door"));
    checkpoint.events.push_back(at_center_door(game, "at_center_door"));
    //  No save: the save stays on Route 202, so the run can be repeated.
    return checkpoint;
}

void diagnostic_route202_to_sandgem_center(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    pin_clock(env, context);
    ConsoleSandgemCenterActions game(env.console, context);
    Checkpoint checkpoint = route202_to_sandgem_center_checkpoint(game);
    run_on_console(env, checkpoint, ROUTE202_TO_SANDGEM_CENTER_NAME);
}


const char* const SANDGEM_MART_LADY_NAME = "Sandgem's Pokemon Center to the Poke Mart lady and back to Route 202 (events)";

TilePos route201_sign_west(){
    return story_maps().to_world(MAP_ROUTE_201, Route201::BORDER_SIGN.next(Direction::WEST));
}

TilePos route201_row_3_east_end(){
    //  Row 3 is open east of her grass to Sandgem's fence: (64,3) is
    //  Sandgem's (0,6), a wall.
    return story_maps().to_world(MAP_ROUTE_201, {63, 3});
}

Checkpoint sandgem_mart_lady_checkpoint(MartLadyActions& game){
    Checkpoint checkpoint;
    checkpoint.id = "mart_lady";
    checkpoint.interrupts.push_back(dialogue_interrupt(game));
    checkpoint.events.push_back(at_center_door(game, "at_center_door"));
    checkpoint.events.push_back(walk_to_tile(game, "walk_to_route_201_sign", "West to the Route 201 sign",
        MAP_SANDGEM_TOWN, route201_sign_west()));
    checkpoint.events.push_back(read_route_201_sign(game, "read_route_201_sign"));
    checkpoint.events.push_back(talk_to_mart_lady(game));
    checkpoint.events.push_back(back_to_route_201_sign(game));
    checkpoint.events.push_back(read_route_201_sign(game, "read_route_201_sign_again"));
    checkpoint.events.push_back(walk_to_center_door(game, "back_to_center_door"));
    checkpoint.events.push_back(at_center_door(game, "at_center_door_again"));
    //  Back to where 00.04 saves with its Potions, as Segment 00 walks it.
    checkpoint.events.push_back(wrapped_step("walk_to_route_202_save_spot",
        "east to the trees, north through the gap between the signs, to Route 202's save spot", "at_route_202_save_spot",
        [&game](RunState& state){
            const Outcome walked = game.walk_to_route_202_save_spot();
            if (walked.succeeded()){
                state.pose = pose_on({story_maps().to_world(MAP_ROUTE_202, Route202::SAVE_SPOT)});
            }
            return walked;
        }));
    //  The save the checkpoint starts from stands there, so it can be repeated.
    checkpoint.save = [&game](RunState&){ return game.save_on_route_202(); };
    return checkpoint;
}

void diagnostic_sandgem_mart_lady(SingleSwitchProgramEnvironment& env, ProControllerContext& context, bool pin){
    if (pin){
        pin_clock(env, context);
    }
    ConsoleMartLadyActions game(env, context);
    Checkpoint checkpoint = sandgem_mart_lady_checkpoint(game);
    run_on_console(env, checkpoint, SANDGEM_MART_LADY_NAME);
}



}
}
}
