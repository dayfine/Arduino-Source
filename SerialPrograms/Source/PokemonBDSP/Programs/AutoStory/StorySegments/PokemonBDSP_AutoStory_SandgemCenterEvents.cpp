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
#include "CommonTools/OCR/OCR_Routines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "PokemonBDSP/Inference/PokemonBDSP_DialogDetector.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_Route201.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_Route202.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_SandgemTown.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_StoryMaps.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStoryTools.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Location.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_PokemonCenter.h"
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

TilePos east_of_lab_sign(){
    return story_maps().to_world(MAP_SANDGEM_TOWN, {6, 7});
}

bool sign_text_matches(const std::string& text, const std::string& key){
    std::string lower;
    for (char ch : text){
        lower += (char)std::tolower((unsigned char)ch);
    }
    return lower.find(key) != std::string::npos;
}

bool doorway_lined_up(const LitDoorway& doorway){
    return doorway.rows_up > 0 && doorway.rows_up <= 1.5 && std::abs(doorway.tiles_across) <= 0.5;
}

bool center_doorway_at_front_row(const LitDoorway& doorway){
    //  From below the sign the door is three tiles west; a tile either way
    //  for where the walk stopped, and the rows as tiles_short_of_door()
    //  counts the front row (1.5).
    //  Inside, the lit floor reads as a doorway well under a row up (0.28
    //  in r129c).
    return doorway.rows_up >= 0.7 && doorway.rows_up <= 1.5 &&
        doorway.tiles_across >= -4.5 && doorway.tiles_across <= 1.5;
}



namespace{

//  Where a sign's box shows: the bottom of the screen, as a dialogue's. The
//  Lab's is grey, Route 201's green; opening either changes the area far
//  more than the overworld behind it moves standing still.
const ImageFloatBox SIGN_BOX(0.19, 0.80, 0.64, 0.18);
//  Its text, inside the margins.
const ImageFloatBox SIGN_TEXT(0.22, 0.82, 0.58, 0.15);
constexpr double SIGN_BOX_OPENED = 20.0;

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

//  3. From where the walk south stops to below the Center's sign, planned
//  with town costs: west to the wall, a tile east into the gap's column,
//  straight south through the gap between the signs, a tile west (the
//  user's route, 2026-10-30; the plan test checks the planner finds it).
//  With walls only, the plan went round the town in 13 legs, into the Lab
//  (r127a) and the Mart (r128b); judging the row by the doorway's height put
//  us a row up, level with the signs (r132a, r133a).
Event walk_to_center_sign(SandgemCenterActions& game){
    Event event;
    event.name = "walk_to_center_sign";
    event.why = "through the signs' gap to below the Center's sign";
    event.require.push_back(pose_known(MAP_SANDGEM_TOWN, PoseCertainty::SET));
    event.act = [&game](RunState& state){
        std::vector<TilePos> at = state.pose.tiles;
        const TilePos goal = below_sandgem_center_sign();
        const Outcome walked = game.walk_in_town(at, [goal](const TilePos& pos){ return pos == goal; }, "To below the Center's sign");
        if (walked.succeeded()){
            state.pose = pose_on(at);
        }
        return walked;
    };
    event.expect.push_back(outside(game));
    return event;
}

//  4. The gate: the Center's sign, at the building's bottom right, reads
//  from the tile below it facing north, and only from there.
Event read_center_sign(SandgemCenterActions& game){
    std::shared_ptr<DialogueRead> read = std::make_shared<DialogueRead>();
    Event event;
    event.name = "read_center_sign";
    event.why = "read the Center's sign from below it";
    event.act = [&game, read](RunState&){
        *read = game.read_sign(Direction::NORTH, "the Center's sign");
        return outcome_ok();
    };
    event.expect.push_back(condition("the Center's sign's box opened and closed", [read](RunState&){
        return read->opened && read->closed;
    }));
    event.expect.push_back(condition("the Center's sign's text, if read, has \"center\"", [read](RunState&){
        return read->text.empty() || sign_text_matches(read->text, "center");
    }));
    event.pose_learned = pose_on({below_sandgem_center_sign()}, Direction::NORTH);
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
        return walk_planned(at, goal, label, Pokemon::RobustPlanCosts());
    }

    Outcome walk_planned(
        std::vector<TilePos>& at,
        const std::function<bool(const TilePos&)>& goal,
        const std::string& label,
        const Pokemon::RobustPlanCosts& costs
    ){
        //  No trainer sees this way; one who does is a failure to look at.
        const RouteTrainerFight fight = [this](const std::string& trainer){
            OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                "Trainer " + trainer + " saw us on a way no trainer should see.", m_stream);
        };
        try{
            at = walk_route(m_stream, m_context, story_maps(), at, goal, m_trainers, fight, label, {}, costs);
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

    virtual Outcome walk_in_town(
        std::vector<TilePos>& at,
        const std::function<bool(const TilePos&)>& goal,
        const std::string& label
    ) override{
        return walk_planned(at, goal, label, town_plan_costs());
    }

    virtual DialogueRead read_sign(Direction facing, const std::string& label) override{
        VideoStream& stream = m_stream;
        DialogueRead read;
        route_turn(stream, m_context, facing);
        //  Let the camera settle after the turn: its drift read as a box
        //  opening (r132a).
        pbf_wait(m_context, 800ms);
        m_context.wait_for_all_requests();
        VideoSnapshot before = stream.video().snapshot();
        //  The text types out over about a second; B before it has finished
        //  only finishes it (r131a).
        pbf_press_button(m_context, BUTTON_A, 80ms, 1500ms);
        m_context.wait_for_all_requests();
        VideoSnapshot shown = stream.video().snapshot();
        if (!before || !shown){
            return read;
        }
        const double opened = walk_screen_change(extract_box_reference(*before.frame, SIGN_BOX), extract_box_reference(*shown.frame, SIGN_BOX));
        read.opened = opened > SIGN_BOX_OPENED;
        if (!read.opened){
            stream.log("[Sign] " + label + ": no box opened (change " + std::to_string(opened) + ").");
            return read;
        }
        //  Tesseract read the Lab's white-on-grey text as "e R O e IO" (r131a).
        read.text = OCR::ocr_read(Language::English, extract_box_reference(*shown.frame, SIGN_TEXT), OCR::PageSegMode::SINGLE_BLOCK);
        stream.log("[Sign] " + label + ": reads \"" + read.text + "\" (change " + std::to_string(opened) + ").");
        for (size_t presses = 0; presses < 3 && !read.closed; presses++){
            pbf_press_button(m_context, BUTTON_B, 80ms, 1000ms);
            m_context.wait_for_all_requests();
            VideoSnapshot after = stream.video().snapshot();
            read.closed = after && walk_screen_change(extract_box_reference(*before.frame, SIGN_BOX), extract_box_reference(*after.frame, SIGN_BOX)) < SIGN_BOX_OPENED;
        }
        stream.log("[Sign] " + label + (read.closed ? ": closed." : ": still open."));
        return read;
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

//  A gate: the sign we walked into, read with A facing it. Only the tiles
//  beside it read it, so the read says which row we are on; the text, when
//  OCR gets it, says which sign. A walk that stopped short of it is put
//  right by walking on until the sign stops us.
Event read_sign(MartLadyActions& game, std::string name, std::string sign, Direction facing, std::string key){
    std::shared_ptr<DialogueRead> read = std::make_shared<DialogueRead>();
    Event event;
    event.name = std::move(name);
    event.why = "read " + sign + " facing " + std::string(Pokemon::direction_name(facing));
    event.act = [&game, read, sign, facing](RunState&){
        *read = game.read_sign(facing, sign);
        return outcome_ok();
    };
    event.expect.push_back(condition(sign + "'s box opened and closed", [read](RunState&){
        return read->opened && read->closed;
    }));
    event.expect.push_back(condition(sign + "'s text, if read, has \"" + key + "\"", [read, key](RunState&){
        return read->text.empty() || sign_text_matches(read->text, key);
    }));
    event.recoveries.push_back(recovery("expect_failed", RecoveryKind::REANCHOR, 1, [&game, facing](RunState&){
        return game.walk_until_stopped(facing, 2, "On to the sign");
    }, "not at the sign: on until it stops us"));
    return event;
}

//  She stands on the top row of her grass, under the trees, a tile or two
//  in from its west edge. From the edge's top tile: east until she stops us,
//  pressing A; then one A per line. Missed: back west to the edge, again.
Event talk_to_mart_lady(MartLadyActions& game){
    std::shared_ptr<DialogueRead> read = std::make_shared<DialogueRead>();
    Event event;
    event.name = "talk_to_mart_lady";
    event.why = "the Poke Mart lady on Route 201 talks (she gives new games Potions)";
    event.act = [&game, read](RunState&){
        *read = game.talk_walking(Direction::EAST, 4, "Poke Mart lady");
        return outcome_ok();
    };
    event.expect.push_back(condition("a dialogue opened and was closed", [read](RunState&){
        return read->opened && read->closed;
    }));
    event.recoveries.push_back(recovery("expect_failed", RecoveryKind::REANCHOR, 2, [&game](RunState&){
        return game.walk_until_stopped(Direction::WEST, 6, "West to the grass's edge");
    }, "missed her: back west to the grass's edge and again"));
    return event;
}


//  The console's side of the Mart lady checkpoint; Jubilife's actions add
//  to it.
template <typename Base>
class ConsoleMartLadyActionsT : public Base{
public:
    ConsoleMartLadyActionsT(SingleSwitchProgramEnvironment& env, ProControllerContext& context)
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
    virtual Outcome walk_in_town(
        std::vector<TilePos>& at, const std::function<bool(const TilePos&)>& goal, const std::string& label
    ) override{
        return m_center.walk_in_town(at, goal, label);
    }
    virtual DialogueRead read_sign(Direction facing, const std::string& label) override{ return m_center.read_sign(facing, label); }
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
        //  Turn first: the first press of a new way only turns us, the screen
        //  holds still meanwhile, and that read as a wall two tiles into the
        //  walk west to the Lab's sign (r132a).
        route_turn(m_env.console, m_context, direction);
        const WalkResult walked = walk_until(m_env.console, m_context, x, y, limit, label);
        m_env.console.log("[Walk] " + label + ": stopped on " + walk_stop_name(walked.stop) + ".");
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
        finish_talk(read, next, label);
        return read;
    }

    virtual DialogueRead talk_walking(Direction direction, size_t tiles, const std::string& label) override{
        DialogueRead read;
        const double x = direction == Direction::EAST ? 1 : direction == Direction::WEST ? -1 : 0;
        const double y = direction == Direction::NORTH ? 1 : direction == Direction::SOUTH ? -1 : 0;
        const bool across = direction == Direction::EAST || direction == Direction::WEST;
        const Milliseconds limit = (int64_t)tiles * (across ? ROUTE_MS_PER_TILE_ACROSS : ROUTE_MS_PER_TILE_UP_DOWN) + 800ms;
        const WalkResult walked = walk_to_talk(m_env.console, m_context, x, y, limit, label);
        if (walked.stop != WalkStop::DIALOG){
            m_env.console.log("[Talk] " + label + ": nobody answered.");
            return read;
        }
        read.opened = true;
        finish_talk(read, BUTTON_A, label);
        return read;
    }

    virtual Outcome step(Direction direction, const std::string& label) override{
        //  200 ms took exactly one row on the console, turning included; 300 ms
        //  took one or two (2026-10-30).
        const double x = direction == Direction::EAST ? 1 : direction == Direction::WEST ? -1 : 0;
        const double y = direction == Direction::NORTH ? 1 : direction == Direction::SOUTH ? -1 : 0;
        const WalkResult walk = walk_for(m_env.console, m_context, x, y, 200ms, label);
        if (walk.stop == WalkStop::DIALOG || walk.stop == WalkStop::TRANSITION){
            return outcome_failed("walk_failed", label + ": " + walk_stop_name(walk.stop));
        }
        return outcome_ok();
    }


    virtual Outcome walk_to_route_202_save_spot() override{
        return wrap("walk_failed", [&]{ walk_from_sandgem_center_to_route_202_save_spot(
            m_env.console, m_context, {story_maps().to_world(MAP_SANDGEM_TOWN, SandgemTown::FRONT_ROW_EAST_END)}
        ); });
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
    //  After a talk opened: reads the first box, then presses `next` once
    //  per line, 1.5 s apart, and only while a box shows: a press after it
    //  has closed would start the talk over.
    void finish_talk(DialogueRead& read, Button next, const std::string& label){
        VideoStream& stream = m_env.console;
        //  The first box's text, inside its white margins.
        if (VideoSnapshot screen = stream.video().snapshot()){
            read.text = OCR::ocr_read(Language::English, extract_box_reference(*screen.frame, SIGN_TEXT), OCR::PageSegMode::SINGLE_BLOCK);
        }
        stream.log("[Talk] " + label + ": the dialogue reads \"" + read.text + "\".");
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
    }

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
using ConsoleMartLadyActions = ConsoleMartLadyActionsT<MartLadyActions>;


void save_end_screenshot(SingleSwitchProgramEnvironment& env, const std::string& name){
    if (VideoSnapshot screen = env.console.video().snapshot()){
        const std::string folder = SCREENSHOTS_PATH() + "AutoStory/route/";
        Filesystem::create_directories(Filesystem::Path(folder));
        const std::string path = folder + now_to_filestring() + "-" + name + ".png";
        screen.frame->save(path);
        env.log("[Checkpoint] Screenshot: " + path);
    }
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
    //  It ends below the Center's sign, facing it: lining up with the door
    //  itself overshot or walked in (r128, r129), and nothing after needs it.
    checkpoint.events.push_back(walk_to_center_sign(game));
    checkpoint.events.push_back(read_center_sign(game));
    //  No save: the save stays on Route 202, so the run can be repeated.
    return checkpoint;
}

void diagnostic_route202_to_sandgem_center(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
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

//  Where the Route 202 checkpoint ends: below the Center's sign.
Event on_doors_row(SandgemCenterActions& game){
    Event event;
    event.name = "on_doors_row";
    event.why = "below the Center's sign, where the Route 202 checkpoint ends";
    event.act = [](RunState& state){
        state.pose = pose_on({below_sandgem_center_sign()}, Direction::NORTH);
        return outcome_ok();
    };
    event.expect.push_back(outside(game));
    return event;
}

Checkpoint sandgem_mart_lady_checkpoint(MartLadyActions& game){
    //  Measured on the console (2026-10-30) with walks that stop on their
    //  own: from below the Center's sign, west stops at the Lab's sign, which
    //  reads facing west. A row south is the path; west along it runs about
    //  22 tiles to her grass's west edge. North from there stops at the trees
    //  2 rows up, on her row. Back: a row south of her row is the signs' row,
    //  and east along it stops at Route 201's sign, which reads facing east.
    //  A row south again is the path; east along it stops at the trees east
    //  of the Mart, a row below the front row's east end.
    Checkpoint checkpoint;
    checkpoint.id = "mart_lady";
    checkpoint.interrupts.push_back(dialogue_interrupt(game));
    checkpoint.events.push_back(on_doors_row(game));
    checkpoint.events.push_back(wrapped_step("west_to_lab_sign",
        "west along the doors' row until the Lab's sign stops us", "at_lab_sign",
        [&game](RunState&){ return game.walk_until_stopped(Direction::WEST, 20, "West to the Lab's sign"); }));
    checkpoint.events.push_back(read_sign(game, "read_lab_sign", "the Lab's sign", Direction::WEST, "lab"));
    checkpoint.events.push_back(wrapped_step("west_along_the_path",
        "a tile south onto the path, then west to the grass's edge", "at_grass_edge",
        [&game](RunState&){
            const Outcome down = game.step(Direction::SOUTH, "South onto the path");
            return down.succeeded() ? game.walk_until_stopped(Direction::WEST, 28, "West along the path") : down;
        }));
    checkpoint.events.push_back(wrapped_step("north_to_the_trees",
        "north until the trees stop us, on her row", "on_her_row",
        [&game](RunState&){ return game.walk_until_stopped(Direction::NORTH, 3, "North to the trees"); }));
    checkpoint.events.push_back(talk_to_mart_lady(game));
    checkpoint.events.push_back(wrapped_step("back_to_route_201_sign",
        "a tile south onto the signs' row, then east until Route 201's sign stops us", "at_route_201_sign",
        [&game](RunState&){
            const Outcome down = game.step(Direction::SOUTH, "South onto the signs' row");
            return down.succeeded() ? game.walk_until_stopped(Direction::EAST, 16, "East to the Route 201 sign") : down;
        }));
    checkpoint.events.push_back(read_sign(game, "read_route_201_sign", "Route 201's sign", Direction::EAST, "201"));
    checkpoint.events.push_back(wrapped_step("east_along_the_path",
        "a tile south onto the path, east to the trees east of the Mart, north a row", "at_front_row_east_end",
        [&game](RunState&){
            const Outcome down = game.step(Direction::SOUTH, "South onto the path");
            if (!down.succeeded()){
                return down;
            }
            const Outcome east = game.walk_until_stopped(Direction::EAST, 40, "East along the path");
            return east.succeeded() ? game.walk_until_stopped(Direction::NORTH, 2, "North to the front row") : east;
        }));
    //  Back to where 00.04 saves with its Potions, as Segment 00 walks it
    //  from the trees east of the Mart.
    checkpoint.events.push_back(wrapped_step("walk_to_route_202_save_spot",
        "from the trees east of the Mart to Route 202's save spot", "at_route_202_save_spot",
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

void diagnostic_sandgem_mart_lady(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    ConsoleMartLadyActions game(env, context);
    Checkpoint checkpoint = sandgem_mart_lady_checkpoint(game);
    run_on_console(env, checkpoint, SANDGEM_MART_LADY_NAME);
}




// ---------------------------------------------------------------------------
//  Jubilife: 00.05a and 00.05b
// ---------------------------------------------------------------------------

const char* const JUBILIFE_CENTER_NAME = "00.05a Jubilife: steps to the Pokemon Center (events)";
const size_t SCHOOL_TILES_WEST_OF_CENTER = 11;
//  The School's lamp, east of its door, from its doorsteps' row at night
//  (2026-10-30): at 0.65 to 0.69 of the screen across the door takes us in;
//  at 0.61 (right of it) and 0.75 (past it) its front stops us. A tile is
//  0.065 of the screen. The box leaves out the Center's lamp further east.
const double SCHOOL_LAMP_X_AT_DOOR = 0.67;
const ImageFloatBox SCHOOL_LAMP_BOX(0.55, 0.20, 0.30, 0.10);
const char* const JUBILIFE_SCHOOL_NAME = "00.05b Jubilife: Pokemon Center to the Trainers' School (events)";

Checkpoint jubilife_center_checkpoint(JubilifeActions& game){
    Checkpoint checkpoint;
    checkpoint.id = "jubilife_center";
    checkpoint.interrupts.push_back(dialogue_interrupt(game));
    checkpoint.events.push_back(wrapped_step("up_the_steps",
        "a tile west of the save, then up the steps onto the main road", "on_main_road",
        [&game](RunState&){ return game.climb_the_steps(); }));
    //  Every leg walks until a wall stops it, measured on the console
    //  (2026-10-30): the fence's end west of the steps; the sign by the lamp
    //  west of the road; across the road, the house east of it; and north up
    //  the east sidewalk, the Center's doorway. The kids south of the Center
    //  can push us part of a tile aside, so the heal lines up by sight.
    checkpoint.events.push_back(wrapped_step("west_to_the_fence",
        "west until the fence's end stops us", "at_fence_end",
        [&game](RunState&){ return game.walk_until_stopped(Direction::WEST, 8, "West to the fence's end"); }));
    checkpoint.events.push_back(wrapped_step("north_to_the_sign",
        "north until the sign by the lamp west of the road stops us", "at_sign",
        [&game](RunState&){ return game.walk_until_stopped(Direction::NORTH, 8, "North to the sign"); }));
    checkpoint.events.push_back(wrapped_step("east_to_the_house",
        "east across the road until the house east of it stops us", "at_east_house",
        [&game](RunState&){ return game.walk_until_stopped(Direction::EAST, 14, "East to the house"); }));
    checkpoint.events.push_back(wrapped_step("north_to_the_center",
        "north up the east sidewalk until the Pokemon Center stops us", "at_center",
        [&game](RunState&){ return game.walk_until_stopped(Direction::NORTH, 14, "North to the Pokemon Center"); }));
    {
        Event heal = wrapped_step("heal",
            "in, the nurse heals, out onto the doorstep", "healed",
            [&game](RunState&){ return game.heal_at_center(); });
        //  r143a's heal said it had walked out while still inside.
        heal.expect.push_back(condition("out of the Pokemon Center, in the overworld", [&game](RunState&){
            return !game.inside_pokemon_center();
        }));
        checkpoint.events.push_back(std::move(heal));
    }
    return checkpoint;
}

Checkpoint jubilife_school_checkpoint(JubilifeActions& game){
    Checkpoint checkpoint;
    checkpoint.id = "jubilife_school";
    checkpoint.interrupts.push_back(dialogue_interrupt(game));
    Event into = wrapped_step("into_the_school",
        "west from the Center's doorstep, counting tiles, north through the School's door", "in_school",
        [&game](RunState&){
            //  Measured on the console (2026-10-30): from the Center's doorstep,
            //  a tap to face west and eleven one-tile holds put us under the
            //  School's doorway; twelve is past it. A hold ends a part of a
            //  tile off (back east, eleven stopped beside the Center's door),
            //  so if the School's front stops us, nudge half a tile each way.
            game.turn(Direction::WEST);
            for (size_t tiles = 0; tiles < SCHOOL_TILES_WEST_OF_CENTER; tiles++){
                const Outcome moved = game.tile(Direction::WEST, "West to the School's door");
                if (!moved.succeeded()){
                    return moved;
                }
            }
            //  At night the lamp east of the door says how far off we are.
            for (size_t looks = 0; looks < 2; looks++){
                const std::optional<double> across = game.school_door_across();
                if (!across || std::abs(*across) <= 0.5){
                    break;
                }
                game.step_across(*across, "Line up with the School's door by its lamp");
            }
            if (game.through_door_north("Trainers' School door")){
                return outcome_ok();
            }
            for (Direction side : {Direction::EAST, Direction::WEST, Direction::WEST}){
                game.nudge(side, "Line up with the School's door");
                if (game.through_door_north("Trainers' School door")){
                    return outcome_ok();
                }
            }
            return outcome_failed("door_failed", "the School's door did not take us from 11 tiles west of the Center's doorstep, or half a tile either side");
        });
    //  It starts on the Center's doorstep: outside, in the overworld.
    into.require.push_back(condition("out of the Pokemon Center, in the overworld", [&game](RunState&){
        return !game.inside_pokemon_center();
    }));
    checkpoint.events.push_back(std::move(into));
    return checkpoint;
}

const char* const JUBILIFE_BARRY_NAME = "00.05c Jubilife: Barry in the Trainers' School (events)";

Checkpoint jubilife_barry_checkpoint(JubilifeActions& game){
    Checkpoint checkpoint;
    checkpoint.id = "jubilife_barry";
    checkpoint.interrupts.push_back(dialogue_interrupt(game));
    //  Stepped through on the console (2026-10-28, and r146b's end shot):
    //  Barry stands at the blackboard, a tile west of the door's column.
    checkpoint.events.push_back(wrapped_step("up_to_barry",
        "a tile west of the door's column, then north until Barry stops us", "at_barry",
        [&game](RunState&){
            game.turn(Direction::WEST);
            const Outcome west = game.tile(Direction::WEST, "West of the door's column");
            return west.succeeded() ? game.walk_until_stopped(Direction::NORTH, 8, "North to Barry") : west;
        }));
    {
        std::shared_ptr<DialogueRead> read = std::make_shared<DialogueRead>();
        Event talk;
        talk.name = "talk_to_barry";
        talk.why = "Barry gives the Town Map";
        talk.skip_if.push_back(condition("talked_to_barry", [](RunState& state){ return state.flags.count("talked_to_barry") > 0; }));
        talk.act = [&game, read](RunState& state){
            *read = game.talk(Direction::NORTH, BUTTON_A, "Barry");
            if (read->opened && read->closed){
                state.flags.insert("talked_to_barry");
            }
            return outcome_ok();
        };
        talk.expect.push_back(condition("his dialogue opened and was closed", [read](RunState&){
            return read->opened && read->closed;
        }));
        talk.recoveries.push_back(recovery("expect_failed", RecoveryKind::REANCHOR, 1, [&game](RunState&){
            return game.walk_until_stopped(Direction::NORTH, 4, "North to Barry");
        }, "not at him: north until he stops us"));
        checkpoint.events.push_back(std::move(talk));
    }
    checkpoint.events.push_back(wrapped_step("out_of_the_school",
        "south until the front wall stops us, a tile east to the door's column, out", "out_of_school",
        [&game](RunState&){
            const Outcome down = game.walk_until_stopped(Direction::SOUTH, 8, "South to the School's front wall");
            if (!down.succeeded()){
                return down;
            }
            game.turn(Direction::EAST);
            const Outcome east = game.tile(Direction::EAST, "To the door's column");
            return east.succeeded() ? game.leave_south("Out of the Trainers' School") : east;
        }));
    return checkpoint;
}

class ConsoleJubilifeActions : public ConsoleMartLadyActionsT<JubilifeActions>{
public:
    ConsoleJubilifeActions(SingleSwitchProgramEnvironment& env, ProControllerContext& context)
        : ConsoleMartLadyActionsT<JubilifeActions>(env, context)
        , m_stream(env.console)
        , m_context(context)
    {}
    virtual Outcome climb_the_steps() override{
        route_turn(m_stream, m_context, Direction::WEST);
        walk_for(m_stream, m_context, -1, 0, 200ms, "To the steps");
        const WalkResult up = walk_for(m_stream, m_context, 0, 1, 480ms, "Up the steps");
        if (up.stop == WalkStop::DIALOG || up.stop == WalkStop::TRANSITION){
            return outcome_failed("walk_failed", std::string("up the steps: ") + walk_stop_name(up.stop));
        }
        return outcome_ok();
    }
    virtual Outcome heal_at_center() override{
        try{
            heal_at_pokemon_center(m_stream, m_context, "Jubilife Pokemon Center");
        }catch (OperationFailedException& e){
            return outcome_failed("heal_failed", e.message());
        }
        return outcome_ok();
    }
    virtual void turn(Direction direction) override{
        route_turn(m_stream, m_context, direction);
    }
    virtual std::optional<double> school_door_across() override{
        m_context.wait_for_all_requests();
        VideoSnapshot screen = m_stream.video().snapshot();
        if (!screen){
            return std::nullopt;
        }
        const std::optional<double> lamp = find_lit_lamp_x(*screen.frame, SCHOOL_LAMP_BOX);
        if (!lamp){
            m_stream.log("[Route] School's door: no lit lamp on screen.");
            return std::nullopt;
        }
        const double across = tiles_across_at(*lamp) - tiles_across_at(SCHOOL_LAMP_X_AT_DOOR);
        m_stream.log("[Route] School's door: the lamp at " + std::to_string(*lamp) + " of the screen, the door " +
            std::to_string(across) + " tiles across.");
        return across;
    }
    virtual void step_across(double tiles, const std::string& label) override{
        m_stream.log("[Route] " + label + ": " + std::to_string(tiles) + " tiles across.");
        move_across(m_stream, m_context, tiles);
        m_context.wait_for_all_requests();
    }
    virtual Outcome leave_south(const std::string& label) override{
        try{
            leave_door_south(m_stream, m_context, label);
        }catch (OperationFailedException& e){
            return outcome_failed("walk_failed", e.message());
        }
        return outcome_ok();
    }
    virtual void nudge(Direction direction, const std::string& label) override{
        route_turn(m_stream, m_context, direction);
        walk_for(m_stream, m_context, direction == Direction::EAST ? 1 : -1, 0, 80ms, label);
    }
    virtual Outcome tile(Direction direction, const std::string& label) override{
        const double x = direction == Direction::EAST ? 1 : direction == Direction::WEST ? -1 : 0;
        const double y = direction == Direction::NORTH ? 1 : direction == Direction::SOUTH ? -1 : 0;
        const WalkResult walk = walk_for(m_stream, m_context, x, y, 160ms, label);
        if (walk.stop == WalkStop::DIALOG || walk.stop == WalkStop::TRANSITION){
            return outcome_failed("walk_failed", label + ": " + walk_stop_name(walk.stop));
        }
        return outcome_ok();
    }
    virtual bool through_door_north(const std::string& label) override{
        m_context.wait_for_all_requests();
        VideoSnapshot before = m_stream.video().snapshot();
        const WalkResult up = walk_until(m_stream, m_context, 0, 1, 1200ms, label);
        m_context.wait_for(1500ms);
        if (up.stop == WalkStop::TRANSITION){
            return true;
        }
        //  The School's fade is not always black enough to read as one: r146b
        //  went in (changes 156, 261) and read "limit". A whole new scene is
        //  through, as leave_door_south() counts it.
        VideoSnapshot after = m_stream.video().snapshot();
        const bool through = before && after && walk_screen_change(*before.frame, *after.frame) > WALK_SCENE_CHANGE;
        if (through){
            m_stream.log("[Route] " + label + ": a new scene; through the door.");
        }
        return through;
    }
private:
    VideoStream& m_stream;
    ProControllerContext& m_context;
};

void diagnostic_jubilife_center(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    ConsoleJubilifeActions game(env, context);
    Checkpoint checkpoint = jubilife_center_checkpoint(game);
    run_on_console(env, checkpoint, JUBILIFE_CENTER_NAME);
}

void diagnostic_jubilife_barry(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    ConsoleJubilifeActions game(env, context);
    Checkpoint checkpoint = jubilife_barry_checkpoint(game);
    run_on_console(env, checkpoint, JUBILIFE_BARRY_NAME);
}

void diagnostic_jubilife_school(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    ConsoleJubilifeActions game(env, context);
    Checkpoint checkpoint = jubilife_school_checkpoint(game);
    run_on_console(env, checkpoint, JUBILIFE_SCHOOL_NAME);
}


}
}
}
