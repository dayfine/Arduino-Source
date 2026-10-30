/*  BDSP AutoStory - Segment 01.03: Oreburgh Mine
 *  From the Oreburgh Pokemon Center into the mine to talk to Roark, and
 *  back out.
 *
 *  Start: in front of the Oreburgh Pokemon Center.
 *  End:   outside the Oreburgh Mine, after talking to Roark.
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/PrettyPrint.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"

#include "../Data/PokemonBDSP_AutoStory_OreburghMine.h"
#include "../Data/PokemonBDSP_AutoStory_StoryMaps.h"
#include "../Detect/PokemonBDSP_AutoStory_OverworldDetector.h"
#include "../Utils/PokemonBDSP_AutoStory_RouteWalk.h"

#include "PokemonBDSP_AutoStory_Segment_01.h"
#include "PokemonBDSP_AutoStory_Segment_01_Shared.h"

using namespace std::chrono_literals;

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using Pokemon::Direction;


namespace{

void oreburgh_to_roark(VideoStream& stream, ProControllerContext& context){
    // What 01.03 does, stepped through on the console before writing it.
    StepLog step(stream, "01.03", "From the Oreburgh Pokemon Center to Roark in the mine", {
        "Walk past Barry at the Center's door to the stone ledge, then east to the fence below the Poke Mart",
        "South to the right-hand conveyor pillar, west to the left-hand pillars, then south into the mine",
        "Through the mine's entrance hall to the rocky corridor",
        "Walk the planned route down the corridor and round the coal block, and talk to Roark",
        "Walk back to the corridor's top and out of the mine",
    });

    step(1);
    // The save stands us in front of the Center, two tiles west of Barry at
    // its door. These are the holds that worked on the console.
    hold_tiles(stream, context, Direction::EAST, 3);
    walk_dir(stream, context, Direction::NORTH, 3000ms, "north to the Center's front");
    hold_tiles(stream, context, Direction::EAST, 2);
    walk_dir(stream, context, Direction::NORTH, 2500ms, "north beside Barry");
    // Roark answers in the mine only after Barry has told us he is there
    // (2026-10-29: he ignored A in r101 and r104 and by hand, and answered at
    // once after a talk with Barry). Barry is just east of us.
    route_turn(stream, context, Direction::EAST);
    pbf_press_button(context, BUTTON_A, 80ms, 200ms);
    if (!dialogue_opens(stream, context, 3000ms)){
        fail(stream, "Oreburgh: Barry did not talk at the gym door.");
    }
    finish_dialogue(stream, context, BUTTON_B, "Barry at the gym door");
    if (walk_dir(stream, context, Direction::SOUTH, 8000ms, "south to the stone ledge").stop != WalkStop::BLOCKED){
        fail(stream, "Oreburgh: walking south did not stop at the stone ledge.");
    }
    // A townsperson wandering across the row can stop us a tile or two short
    // (r95), so walk east again until a walk stops without moving.
    bool at_fence = false;
    for (size_t attempt = 0; attempt < 5 && !at_fence; attempt++){
        const WalkResult east = walk_dir(stream, context, Direction::EAST, 10000ms, "east to the fence below the Poke Mart");
        if (east.stop != WalkStop::BLOCKED){
            fail(stream, "Oreburgh: walking east did not stop at the fence below the Poke Mart.");
        }
        at_fence = attempt > 0 && east.walked <= 800ms;
        if (!at_fence){
            context.wait_for(1500ms);
        }
    }
    if (!at_fence){
        fail(stream, "Oreburgh: something kept stopping the walk east to the fence below the Poke Mart.");
    }

    step(2);
    // The mine's stairs are straight south of here, two columns west of
    // the fence (Bulbapedia's Oreburgh map: column 46, between the conveyor
    // pillars at 42-44 and 48-50). One walk south goes down the yard's
    // stairs, under the conveyors and into the mine; on the console it ran
    // on through the entrance hall into the rocky corridor. A worker walks
    // about on the sand path: wait for him, then try the columns beside.
    // Timed holds of a tile or two move one tile or two unpredictably, so
    // the column comes from walls. Down the path along its east fence ends
    // against the right-hand conveyor pillar in the yard (column 48); from
    // there west stops against the left-hand pillars, on column 45, which
    // runs clear south into the mine. The belts keep the screen moving, so
    // these stops read as the walk's time limit, not as a wall.
    //
    // The worker by the fence wanders onto the path and stops the walk south
    // as a wall would (r95, r96): wait for him to move and walk on.
    WalkResult down{WalkStop::BLOCKED, 0ms, 0};
    for (size_t attempt = 0; attempt < 5; attempt++){
        down = walk_dir(stream, context, Direction::SOUTH, 12000ms, "south to the right-hand pillar");
        if (down.stop == WalkStop::DIALOG){
            talk_or_fight(stream, context, "Someone on the way to the mine");
        }else if (down.stop != WalkStop::BLOCKED){
            break;
        }
        context.wait_for(2000ms);
    }
    bool in_mine = down.stop == WalkStop::TRANSITION;   // already in the mine's column
    if (!in_mine && down.stop != WalkStop::LIMIT){
        fail(stream, "Oreburgh: something kept stopping the walk south to the conveyor pillars.");
    }
    // West stops against the left-hand pillars, on the column of the sign
    // above the mine's stairs (on the console, r96): the stairs are open a
    // tile either side of it, so step back one tile east and walk south.
    auto step_tiles = [&](Direction side, size_t tiles){
        route_turn(stream, context, side);
        for (size_t tile = 0; tile < tiles; tile++){
            pbf_move_left_joystick(context, {side == Direction::EAST ? 1.0 : -1.0, 0}, 120ms, 300ms);
        }
        context.wait_for_all_requests();
    };
    if (!in_mine){
        walk_dir(stream, context, Direction::WEST, 2500ms, "west to the left-hand pillars");
        step_tiles(Direction::EAST, 1);
        in_mine = walk_dir(stream, context, Direction::SOUTH, 15000ms, "south into the mine").stop == WalkStop::TRANSITION;
    }
    if (!in_mine){
        step_tiles(Direction::WEST, 2);     // the sign's other side
        in_mine = walk_dir(stream, context, Direction::SOUTH, 5000ms, "south down the mine's stairs").stop == WalkStop::TRANSITION;
    }
    if (!in_mine){
        fail(stream, "Oreburgh: no column south of the Center led into the mine.");
    }
    context.wait_for(1500ms);

    step(3);
    // If the fade into the mine was missed, we are already in the corridor
    // and this walk stops at its boulder; otherwise it crosses the hall.
    const WalkResult hall = walk_dir(stream, context, Direction::SOUTH, 10000ms, "through the mine's entrance hall");
    if (hall.stop == WalkStop::TRANSITION){
        context.wait_for(1500ms);
    }

    step(4);
    // Planned on the coal room's tile grid (Data/OreburghMine): the
    // corridor's rocks zigzag a row at a time, the room's second row passes
    // under the worker on the first, and the east side leads down to the
    // bottom row below Roark.
    const Pokemon::MapSet& maps = oreburgh_mine_maps();
    auto world = [&](const Pokemon::TilePos& pos){ return maps.to_world(MAP_OREBURGH_MINE, pos); };
    std::vector<Pokemon::TrainerSight> no_trainers;
    auto in_the_way = [&](const std::string& who){ talk_or_fight(stream, context, who); };
    std::vector<Pokemon::TilePos> at;
    for (const Pokemon::TilePos& pos : OreburghMine::corridor_top()){
        at.push_back(world(pos));
    }
    // Roark stands with a rock on each side, so the rock east of him is the
    // anchor: plan to his row's open stretch east of the rock, walk north to
    // the wall above it and west until the rock stops us, two tiles east of
    // him (on the console, r97). Then south to the bottom wall and two tiles
    // west puts us below him.
    const Pokemon::TilePos roark = world(OreburghMine::ROARK);
    at = walk_route(stream, context, maps, at,
        [&](const Pokemon::TilePos& pos){ return pos.y >= roark.y && pos.y <= roark.y + 1 && pos.x >= roark.x + 2 && pos.x <= roark.x + 7; },
        no_trainers, in_the_way, "To the stretch east of Roark");
    walk_dir(stream, context, Direction::NORTH, 2000ms, "north to the wall on Roark's row");
    // A wild battle on the way eats into the walk's time (r100): walk again.
    WalkStop west = WalkStop::LIMIT;
    for (size_t attempt = 0; attempt < 3 && west == WalkStop::LIMIT; attempt++){
        west = walk_dir(stream, context, Direction::WEST, 6000ms, "west to the rock beside Roark").stop;
    }
    if (west != WalkStop::BLOCKED){
        fail(stream, "Oreburgh Mine: walking west along Roark's row did not stop at his rock.");
    }
    if (walk_dir(stream, context, Direction::SOUTH, 2000ms, "south to the bottom wall").stop != WalkStop::BLOCKED){
        fail(stream, "Oreburgh Mine: walking south from Roark's rock did not stop at the bottom wall.");
    }
    route_turn(stream, context, Direction::WEST);
    pbf_move_left_joystick(context, {-1, 0}, 120ms, 300ms);
    pbf_move_left_joystick(context, {-1, 0}, 120ms, 300ms);
    route_turn(stream, context, Direction::NORTH);
    context.wait_for_all_requests();
    // Movement is by whole tiles: a sideways nudge only steps onto his rock's
    // tile (r105, r106 read the rock's line as his). Turn to him and press A
    // once; the camera zooms in and his first line, "OK, stand back and watch
    // this.", can take some seconds (r98).
    bool talked = false;
    for (size_t attempt = 0; attempt < 3 && !talked; attempt++){
        route_turn(stream, context, Direction::EAST);
        route_turn(stream, context, Direction::NORTH);
        context.wait_for(500ms);
        if (VideoSnapshot screen = stream.video().snapshot()){
            const std::string folder = SCREENSHOTS_PATH() + "AutoStory/route/";
            Filesystem::create_directories(Filesystem::Path(folder));
            screen.frame->save(folder + now_to_filestring() + "-below-roark-" + std::to_string(attempt) + ".png");
        }
        pbf_press_button(context, BUTTON_A, 80ms, 200ms);
        talked = dialogue_opens(stream, context, 8000ms);
    }
    if (!talked){
        fail(stream, "Oreburgh Mine: no talk opened below Roark.");
    }
    finish_dialogue(stream, context, BUTTON_B, "Roark");
    // His scene ends with him smashing his rock and leaving: his tile is
    // free now. Step onto it and back, or fail if he is still there.
    if (walk_dir(stream, context, Direction::NORTH, 2000ms, "onto Roark's tile").walked <= 800ms){
        fail(stream, "Oreburgh Mine: Roark is still there after the talk.");
    }
    walk_dir(stream, context, Direction::SOUTH, 2000ms, "back to the bottom wall");
    at = {roark.next(Direction::SOUTH)};

    step(5);
    // Back up the corridor, then north out through the hall. A route leg
    // north to the top's wall walks on through it into the hall (r99), and
    // no wall stops us a row short of it, so the route ends on the corner
    // at (14,11), where that leg started, and the walk does the rest.
    const Pokemon::TilePos exit = world(OreburghMine::CORRIDOR_EXIT_START);
    at = walk_route(stream, context, maps, at,
        [&](const Pokemon::TilePos& pos){ return pos == exit; },
        no_trainers, in_the_way, "Back to the corridor's top");
    // Up into the entrance hall (a transition, as on the way in), then on
    // north out of the mine: in r103 that walk left the mine without a
    // transition we saw and ended against the sign above the stairs, which
    // the moving belts make read as the time limit.
    if (walk_dir(stream, context, Direction::NORTH, 10000ms, "into the entrance hall").stop != WalkStop::TRANSITION){
        fail(stream, "Oreburgh Mine: walking north from the corridor did not reach the entrance hall.");
    }
    context.wait_for(1500ms);
    if (walk_dir(stream, context, Direction::NORTH, 10000ms, "out of the mine").stop == WalkStop::DIALOG){
        fail(stream, "Oreburgh Mine: someone stopped us on the way out of the mine.");
    }
    context.wait_for(1500ms);
}

void checkpoint_0103(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    AutoStoryOptions options,
    AutoStoryStats& stats
){
    checkpoint_reattempt_loop(env, context, options, stats,
        [&](size_t /*attempt*/){
            oreburgh_to_roark(env.console, context);
        },
        true
    );
}
}


std::string AutoStory_Checkpoint_0103::name()       const { return "01.03 - Oreburgh Mine to Roark"; }
std::string AutoStory_Checkpoint_0103::start_text() const { return AutoStory_Checkpoint_0102().end_text(); }
std::string AutoStory_Checkpoint_0103::end_text()   const { return "Outside the Oreburgh Mine, after talking to Roark."; }

void AutoStory_Checkpoint_0103::run_checkpoint(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    AutoStoryOptions options,
    AutoStoryStats& stats
) const{
    checkpoint_0103(env, context, options, stats);
}


}
}
}
