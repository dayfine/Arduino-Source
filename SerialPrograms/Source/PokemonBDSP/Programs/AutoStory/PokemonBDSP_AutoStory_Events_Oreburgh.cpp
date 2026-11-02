/*  BDSP AutoStory - Events: Oreburgh (01.03 to 01.05)
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Segment 01's walks in Oreburgh: the mine and Roark, the Gym, and back
 *  out of the Oreburgh Gate, as they ran on the console (2026-10-29 to
 *  10-31).
 *
 */

#include <cstdlib>
#include "CommonFramework/GlobalAutoPaths.h"
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/PrettyPrint.h"
#include "CommonFramework/Globals.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "PokemonBDSP/Inference/PokemonBDSP_DialogDetector.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_Maps.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_StoryMaps.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Location.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_PlannedRoute.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_RouteWalk.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Walk.h"
#include "PokemonBDSP_AutoStory_Talks.h"
#include "PokemonBDSP_AutoStory_Walks.h"
#include "PokemonBDSP_AutoStory_WildBattle.h"
#include "PokemonBDSP_AutoStory_Events.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace std::chrono_literals;
using Pokemon::Direction;


namespace{

//  The Center's floor: Oreburgh's sand reads up to 34 % warm outside
//  (r189a), the floor 56 to 64 % inside.
bool oreburgh_center_floor(const ImageViewRGB32& screen){
    size_t percent = 0;
    center_floor_on_screen(screen, &percent);
    return percent >= 45;
}
bool oreburgh_center_inside_now(VideoStream& stream){
    const VideoSnapshot frame = stream.video().snapshot();
    return frame && oreburgh_center_floor(*frame.frame);
}

//  North into the Center's door from this column: whether we are inside.
//  Someone in the door (Barry, the first time) is talked to and the walk
//  tried again.
bool into_oreburgh_center_door(VideoStream& stream, ProControllerContext& context){
    // The door's fade often reads as the time limit, not a black screen
    // (r183a went in and on to the counter): look at the floor.
    // Stop as soon as the floor shows: walking on to the counter left the
    // door's column, and the walk back out drifted into the yard (r188b).
    const WalkResult in = walk_until(stream, context, run_from_wild_battle, {0, 1}, 2500ms, "north into the Center's door",
        oreburgh_center_floor);
    if (in.stop == WalkStop::ARRIVED || ((in.stop == WalkStop::TRANSITION || in.stop == WalkStop::LIMIT) && oreburgh_center_inside_now(stream))){
        return true;
    }
    if (in.stop == WalkStop::DIALOG){
        talk_or_fight(stream, context, "Someone at the Center's door");
    }else{
        pbf_press_button(context, BUTTON_A, 80ms, 200ms);
        if (!dialogue_opens(stream, context, 2000ms)){
            return false;
        }
        finish_dialogue(stream, context, BUTTON_B, "Barry at the Center's door");
        context.wait_for(2000ms);    // he runs off
    }
    // Talked in the door's column: the door is free now.
    walk_until(stream, context, run_from_wild_battle, {0, 1}, 2500ms, "north into the Center's door",
        oreburgh_center_floor);
    return oreburgh_center_inside_now(stream);
}

//  Oreburgh: from a row or two below the Pokemon Center's front, west of
//  the fence below its east corner, into the Center by its door, stopping
//  just inside. Talks to Barry if he stands in the door (01.03, the first
//  time).
void enter_oreburgh_center(VideoStream& stream, ProControllerContext& context){
    // The save stands us a row or two below the Center's front, a little
    // west of its door. The posts and the road beside the Center make north
    // from there unreliable (r191a, r192a walked up the road to the city's
    // north exit), and counted taps east drift. The fence running south from
    // the Center's east corner stops a walk east on these rows a tile east of
    // the door (by hand): walk to it, north to the Center's front, tap back
    // one, and shift one tap either side.
    walk_to_wall(stream, context, run_from_wild_battle, {1, 0}, 5000ms, "east to the fence below the Center");
    walk_to_wall(stream, context, run_from_wild_battle, {0, 1}, 3000ms, "north to the Center's front");
    // The door wants us near its middle: a turn and a tap west (half a tile
    // too far, by hand) or a turn alone leave north blocked by the facade
    // beside it. The facade blocks north for two tiles either side of the
    // door, so look for it in nudges of about a third of a tile.
    route_turn(stream, context, Direction::WEST);
    context.wait_for_all_requests();
    bool inside = false;
    const int offsets[] = {0, -1, -2, 1, -3, 2, -4, 3};   //  nudges from here
    int nudged = 0;
    for (size_t i = 0; i < 8 && !inside; i++){
        const int offset = offsets[i];
        if (offset != nudged){
            // No turn first: a turn is a tap too and moves us (see
            // talk_with_nudges()); a nudge both turns and moves a little.
            for (; nudged != offset; nudged += offset > nudged ? 1 : -1){
                pbf_move_left_joystick(context, {offset > nudged ? 1.0 : -1.0, 0}, ROUTE_NUDGE_TAP, 300ms);
            }
            context.wait_for_all_requests();
        }
        inside = into_oreburgh_center_door(stream, context);
    }
    if (!inside || !oreburgh_center_inside_now(stream)){
        fail_step(stream, "Oreburgh: no column east of the save went into the Pokemon Center.");
    }
}

//  Oreburgh: from just inside the Pokemon Center's door, up to the nurse
//  and heal; ends at the counter facing her. The door is straight south of
//  her (by hand, 2026-10-31: north from the door stopped at the counter).
void heal_at_oreburgh_counter(VideoStream& stream, ProControllerContext& context){
    if (walk_to_talk(stream, context, run_from_wild_battle, {0, 1}, 6000ms, "to the nurse").stop != WalkStop::DIALOG){
        fail_step(stream, "Oreburgh Pokemon Center: the nurse did not answer.");
    }
    // A throughout: Yes to "rest your Pokemon?" (B would answer No).
    finish_dialogue(stream, context, BUTTON_A, "Nurse");
    // Healing shows no text box for a few seconds, which reads as the end;
    // her "Thank you for waiting" lines follow it.
    if (dialogue_opens(stream, context, 6000ms)){
        finish_dialogue(stream, context, BUTTON_B, "Nurse, after healing");
    }
}



//  Oreburgh, from the street in front of the Pokemon Center (its door or its
//  front wall) into the gym, as walked by hand on 2026-10-29: west to the
//  cliff, south to the posts, a row up, then 17 tiles east is the gym door's
//  column. The hold is checked by the door itself: walk north, and if the
//  gym's wall stops us, try the columns either side.
//  D-pad moves from the Center's doorstep (as walk_out_of_center() leaves us
//  there) to below the gym's door: 20. The program's own runs landed 2 tiles
//  west with 22 and 1 tile west with 21 (r217d, r218d, the same tiles each
//  time). By hand, after a stick exit, it took 22 taps; the hand exit and
//  count did not carry over to the program's. Stick holds drift: 160 ms moved
//  36 to 48 px, so 18 or 19 of them missed the door by a part of a tile.
constexpr size_t GYM_DPAD_TAPS_WEST_OF_CENTER = 20;

void oreburgh_street_to_gym(VideoStream& stream, ProControllerContext& context){
    // From the Center's doorstep, straight west along its row to below the
    // gym's door, then north through it, all on the D-pad (whole tiles; see
    // GYM_DPAD_TAPS_WEST_OF_CENTER).
    dpad_tiles(stream, context, DPAD_LEFT, GYM_DPAD_TAPS_WEST_OF_CENTER, "west to the gym's door");
    if (dpad_through_door_north(stream, context, "Oreburgh Gym door")){
        return;
    }
    fail_step(stream, "Oreburgh: no column near the gym's door went in.");
}

//  Inside the Oreburgh Gym: straight up the middle, fighting the trainers
//  who stop us, to Roark on the top tier. Returns whether Roark was beaten;
//  false after a whiteout, which leaves us in the Pokemon Center.
//  Walking straight up meets both gym trainers (one at the entrance, one on
//  the second tier); their experience helps an under-levelled team, and
//  after a whiteout they stay beaten. Butters' route goes round them.
bool oreburgh_gym_battles(VideoStream& stream, ProControllerContext& context){
    // The entry camera pans for about 7.5 s (Butters' reference video).
    context.wait_for(8000ms);
    if (dialogue_opens(stream, context, 3000ms)){
        if (talk_or_fight(stream, context, "Gym trainer at the entrance") && in_pokemon_center(stream)){
            return false;
        }
    }
    size_t stuck = 0;
    for (size_t leg = 0; leg < 10; leg++){
        const WalkResult up = walk_until_no_wild(stream, context, {0, 1}, 8000ms, "north up the gym");
        if (up.stop == WalkStop::DIALOG){
            if (talk_or_fight(stream, context, "Gym trainer") && in_pokemon_center(stream)){
                return false;
            }
            continue;
        }
        if (up.stop != WalkStop::BLOCKED){
            continue;
        }
        // Roark stands at the top facing us; a wall or a stair edge says
        // nothing to A. Only Roark starts a battle from our A here. The walk
        // left us facing north: a turn tap that way would be a step.
        pbf_press_button(context, BUTTON_A, 80ms, 200ms);
        if (dialogue_opens(stream, context, 3000ms)){
            // His lines pause for more than 4 s before the battle (r210d: "only
            // talked"), so the battle is expected.
            if (talk_or_fight(stream, context, "Roark", TalkExpect::BATTLE)){
                if (in_pokemon_center(stream)){
                    return false;
                }
                finish_dialogue(stream, context, BUTTON_B, "Roark after the battle");
                return true;
            }
            continue;
        }
        if (up.walked <= 800ms && ++stuck >= 3){
            fail_step(stream, "Oreburgh Gym: stuck on the way up, and nobody answers A.");
        }
    }
    fail_step(stream, "Oreburgh Gym: did not reach Roark.");
}

//  01.03: into the Pokemon Center by its door (talking to Barry if he stands in it).
void oreburgh_center_door(VideoStream& stream, ProControllerContext& context){
    // Planned on Bulbapedia's Oreburgh map (devtools/maps/examples/
    // oreburgh_center_to_mine.png): the column of the Center's door runs
    // straight south down the yard's stairs, between the conveyors, into
    // the mine (walked north up it by hand, 2026-10-31). The door is the one
    // place that pins that column: go in and back out, and we stand on it
    // facing south. The save stands us two tiles west of the door and a row
    // down; north stops against the Center's front or its post, then a tile
    // at a time east until north goes in. Barry stands in the door the first
    // time: walking into him reads as a wall, so A there; he tells us Roark
    // went to the mine, which Roark needs before he answers (2026-10-29: he
    // ignored A in r101 and r104 until then). A save after his talk has him
    // gone already.
    //
    enter_oreburgh_center(stream, context);
    context.wait_for(1000ms);
}

//  01.03: straight south from the door into the mine.
void south_into_the_mine(VideoStream& stream, ProControllerContext& context){
    // Straight back south: out of the door and on into the mine. Fades go
    // unseen at times (r184b missed the mine's, r185a the Center's and walked
    // on into the mine hall), so count: the Center's door and the mine are
    // two transitions, or about 1 s inside and 8.6 s outside of walking (by
    // hand). A worker walks about on the sand path and stops the walk as a
    // wall would (r95, r96): wait for him to move and walk on. Step 3 copes
    // with ending in the hall or in the corridor.
    bool in_mine = false;
    size_t transitions = 0;
    Milliseconds walked = 0ms;
    for (size_t attempt = 0; attempt < 6 && !in_mine; attempt++){
        const WalkResult down = walk_dir(stream, context, Direction::SOUTH, 15000ms, "south out of the Center and into the mine");
        walked += down.walked;
        if (down.stop == WalkStop::TRANSITION){
            transitions++;
            context.wait_for(1000ms);
        }else if (down.stop == WalkStop::DIALOG){
            talk_or_fight(stream, context, "Someone on the way to the mine");
        }else if (down.stop == WalkStop::BLOCKED){
            context.wait_for(2000ms);
        }
        // Counting alone took a walk that drifted into the yard (r188b) for the
        // mine: it also has to look like the mine.
        in_mine = (transitions >= 2 || walked >= 9500ms) && in_dim_cave(stream);
    }
    if (!in_mine){
        fail_step(stream, "Oreburgh: the walk south from the Center's door did not reach the mine.");
    }
    context.wait_for(1500ms);
}

//  01.03: through the mine's entrance hall to the rocky corridor.
void mine_hall(VideoStream& stream, ProControllerContext& context){
    // If the fade into the mine was missed, we are already in the corridor
    // and this walk stops at its boulder; otherwise it crosses the hall.
    const WalkResult hall = walk_dir(stream, context, Direction::SOUTH, 10000ms, "through the mine's entrance hall");
    if (hall.stop == WalkStop::TRANSITION){
        context.wait_for(1500ms);
    }
}

//  After a wild battle on the way: back to the east end of the bottom row,
//  three taps west and north to the wall on his row (Maps/OreburghMine.json:
//  columns 23 to 25 have that wall above).
void line_up_below_roark_again(VideoStream& stream, ProControllerContext& context){
    // The battle may leave us on his row or the bottom one.
    walk_to_wall(stream, context, run_from_wild_battle, {0, -1}, 2000ms, "south to the bottom row");
    walk_to_wall(stream, context, run_from_wild_battle, {1, 0}, 6000ms, "east along the bottom row");
    route_turn(stream, context, Direction::WEST);
    for (size_t tap = 0; tap < 3; tap++){
        pbf_move_left_joystick(context, {-1, 0}, 120ms, 300ms);
    }
    walk_to_wall(stream, context, run_from_wild_battle, {0, 1}, 2000ms, "north to the wall on Roark's row");
}

//  From his row east of his rock: west to the rock, down and two taps west
//  below him, and A with nudges. Whether his talk opened.
bool talk_to_roark_from_his_row(VideoStream& stream, ProControllerContext& context){
    // A wild battle on the way eats into the walk's time (r100): walk again.
    for (size_t walk = 0; walk < 3; walk++){
        if (walk_to_wall(stream, context, run_from_wild_battle, {-1, 0}, 6000ms, "west to the rock beside Roark").wild_battles == 0){
            break;
        }
    }
    walk_to_wall(stream, context, run_from_wild_battle, {0, -1}, 2000ms, "south to the bottom wall");
    route_turn(stream, context, Direction::WEST);
    pbf_move_left_joystick(context, {-1, 0}, 120ms, 300ms);
    pbf_move_left_joystick(context, {-1, 0}, 120ms, 300ms);
    route_turn(stream, context, Direction::NORTH);
    context.wait_for_all_requests();
    if (run_from_late_encounter(stream, context, run_from_wild_battle, "below Roark")){
        return false;
    }
    if (VideoSnapshot screen = stream.video().snapshot()){
        const std::string folder = SCREENSHOTS_PATH() + "AutoStory/route/";
        Filesystem::create_directories(Filesystem::Path(folder));
        screen.frame->save(folder + now_to_filestring() + "-below-roark.png");
    }
    // The camera zooms in and his first line, "OK, stand back and watch
    // this.", can take some seconds (r98).
    // His rocks answer A too ("It's a rugged rock..."): his talk is the
    // one that zooms the camera in on us (by hand), which the rock's
    // message does not. Compare the view above the dialogue box.
    return talk_with_nudges(stream, context, run_from_wild_battle, Direction::NORTH, "Roark", 8000ms,
        [&](const ImageViewRGB32& before, const ImageViewRGB32& after){
            const ImageFloatBox above_box{0.0, 0.0, 1.0, 0.6};
            const double change = walk_screen_change(
                extract_box_reference(before, above_box), extract_box_reference(after, above_box)
            );
            stream.log("[01.03] View change when the talk opened: " + std::to_string(change) + " (Roark zooms in).");
            return change > 35;     //  by hand: 50 for his zoom
        });
}

//  His scene (by hand, 2026-10-31): see roark_in_the_mine(). Fails if he
//  is still there after it.
void wait_out_roarks_scene(VideoStream& stream, ProControllerContext& context){
    bool gone = false;
    for (size_t round = 0; round < 3 && !gone; round++){
        for (size_t press = 0; press < 14; press++){
            pbf_press_button(context, BUTTON_B, 80ms, 2420ms);
        }
        pbf_press_button(context, BUTTON_A, 80ms, 200ms);
        gone = !dialogue_opens(stream, context, 2500ms);
    }
    if (!gone){
        fail_step(stream, "Oreburgh Mine: Roark is still there after the talk.");
    }
}

//  From below Roark, back up the corridor and out of the mine.
void out_of_oreburgh_mine(
    VideoStream& stream, ProControllerContext& context, const StoryWorld& maps,
    RouteTrainers& no_trainers, const Pokemon::TilePos& roark
){
    const auto world = [&](const Pokemon::TilePos& pos){ return maps.to_world(MAP_OREBURGH_MINE, pos); };
    walk_dir(stream, context, Direction::SOUTH, 2000ms, "back to the bottom wall");
    std::vector<Pokemon::TilePos> at{roark.next(Direction::SOUTH)};

    // Back up the corridor, then north out through the hall. A route leg
    // north to the top's wall walks on through it into the hall (r99), and
    // no wall stops us a row short of it, so the route ends on the corner
    // at (14,10), where that leg started, and the walk does the rest.
    const Pokemon::TilePos exit = world(map_point("OreburghMine", "corridor_exit_start"));
    at = walk_route(stream, context, run_from_wild_battle, maps, at, no_trainers, RouteWalkPlan{[&](const Pokemon::TilePos& pos){ return pos == exit; }, "Back to the corridor's top", {}, route_plan_costs()});
    // Up into the entrance hall (a transition, as on the way in), then on
    // north out of the mine: in r103 that walk left the mine without a
    // transition we saw and ended against the sign above the stairs, which
    // the moving belts make read as the time limit.
    if (walk_dir(stream, context, Direction::NORTH, 10000ms, "into the entrance hall").stop != WalkStop::TRANSITION){
        fail_step(stream, "Oreburgh Mine: walking north from the corridor did not reach the entrance hall.");
    }
    context.wait_for(1500ms);
    if (walk_dir(stream, context, Direction::NORTH, 10000ms, "out of the mine").stop == WalkStop::DIALOG){
        fail_step(stream, "Oreburgh Mine: someone stopped us on the way out of the mine.");
    }
    context.wait_for(1500ms);
}

//  01.03: the planned route down the corridor to Roark, his talk, and back out of the mine.
void roark_in_the_mine(VideoStream& stream, ProControllerContext& context){
    // Planned on the coal room's tile grid (Maps/OreburghMine.json): the
    // corridor's rocks zigzag a row at a time, the room's second row passes
    // under the worker on the first, and the east side leads down to the
    // bottom row below Roark.
    const StoryWorld& maps = oreburgh_mine_maps();
    auto world = [&](const Pokemon::TilePos& pos){ return maps.to_world(MAP_OREBURGH_MINE, pos); };
    RouteTrainers no_trainers{{}, [&](const std::string& who){ talk_or_fight(stream, context, who); }};
    std::vector<Pokemon::TilePos> at;
    for (const Pokemon::TilePos& pos : map_region("OreburghMine", "corridor_top")){
        at.push_back(world(pos));
    }
    // Roark stands with a rock on each side, so the rock east of him is the
    // anchor: plan to his row's open stretch east of the rock, walk north to
    // the wall above it and west until the rock stops us, two tiles east of
    // him (on the console, r97). Then south to the bottom wall and two tiles
    // west puts us below him.
    const Pokemon::TilePos roark = world(map_point("OreburghMine", "roark"));
    at = walk_route(stream, context, run_from_wild_battle, maps, at, no_trainers, RouteWalkPlan{[&](const Pokemon::TilePos& pos){ return pos.y >= roark.y && pos.y <= roark.y + 1 && pos.x >= roark.x + 2 && pos.x <= roark.x + 7; }, "To the stretch east of Roark", {}, route_plan_costs()});
    walk_to_wall(stream, context, run_from_wild_battle, {0, 1}, 2000ms, "north to the wall on Roark's row");
    // His rock and the bottom wall end these legs (see walk_to_wall()). The
    // two taps west from there land within a third of a tile or so of his
    // column (s03e2 missed him by that much), so look for him with nudges
    // either side. Any tap here can start a wild battle, which leaves us
    // off his column (r180a, even on his row west of his rock): then line up
    // again from the east end of the bottom row, three taps west and north to the wall on his row
    // (Maps/OreburghMine.json: columns 23 to 25 have that wall above).
    bool talked = false;
    for (size_t attempt = 0; attempt < 3 && !talked; attempt++){
        if (attempt > 0){
            // The battle may leave us on his row or the bottom one.
            line_up_below_roark_again(stream, context);
        }
        talked = talk_to_roark_from_his_row(stream, context);
    }
    if (!talked){
        fail_step(stream, "Oreburgh Mine: no talk opened below Roark (has Barry told us about the mine?).");
    }
    // His scene (by hand, 2026-10-31): the camera zooms in, about eight
    // lines that each wait for a press ("OK, stand back and watch this.",
    // Rock Smash, "...That'd be me!"), then he smashes his rock and leaves.
    // finish_dialogue() ended early on it, and stepping onto his tile could
    // not tell him from the time the stop takes to read (r184, r186 "passed"
    // with him still there, and Barry kept the gym shut). Press B through it
    // on a clock, then ask his tile: once he is gone, A there opens nothing.
    // B and A in the overworld facing a wall do nothing.
    wait_out_roarks_scene(stream, context);
    out_of_oreburgh_mine(stream, context, maps, no_trainers, roark);
}

//  01.03: into the Pokemon Center and heal at the counter.
void oreburgh_center_heal(VideoStream& stream, ProControllerContext& context){
    // End at the nurse, healed: the counter is a fixed start for 01.04,
    // where the mine's mouth is not (2026-10-31, at the user's request). Out
    // of the mine we stand below its sign; a tile east is the Center door's
    // column, and north on it goes in, or stops on the front rows.
    hold_tiles(stream, context, Direction::EAST, 1);
    walk_until(stream, context, run_from_wild_battle, {0, 1}, 12000ms, "north into the Center's door",
        [](const ImageViewRGB32& screen){ size_t percent = 0; center_floor_on_screen(screen, &percent); return percent >= 45; });
    size_t floor = 0;
    if (VideoSnapshot frame = stream.video().snapshot()){
        center_floor_on_screen(*frame.frame, &floor);
    }
    if (floor < 45){
        route_turn(stream, context, Direction::SOUTH);
        pbf_move_left_joystick(context, {0, -1}, 480ms, 300ms);
        enter_oreburgh_center(stream, context);
    }
    heal_at_oreburgh_counter(stream, context);
}

//  01.04: out of the Pokemon Center from the counter.
void out_of_the_center(VideoStream& stream, ProControllerContext& context){
    // 01.03 leaves us at the Center's counter, facing the nurse; the door
    // is straight south.
    walk_out_of_center(stream, context, "Oreburgh");
}

//  01.04: west along the row into the gym and up it to Roark, again from the Center after a loss.
void gym_until_roark_beaten(VideoStream& stream, ProControllerContext& context){
    // Losing costs some money and keeps the experience, so each try comes
    // back stronger: Roark won twice by hand against Piplup Lv 10 and 14 and
    // lost the third time.
    bool won = false;
    for (size_t attempt = 0; attempt < 5 && !won; attempt++){
        oreburgh_street_to_gym(stream, context);
        won = oreburgh_gym_battles(stream, context);
        if (!won){
            stream.log("[01.04] Lost in the gym (try " + std::to_string(attempt + 1) + "); back from the Center.", COLOR_ORANGE);
            walk_out_of_center(stream, context, "Oreburgh");
        }
    }
    if (!won){
        fail_step(stream, "Oreburgh Gym: lost to Roark five times.");
    }
}

//  01.05: out of the gym onto its doorstep.
void out_of_the_gym(VideoStream& stream, ProControllerContext& context){
    // 01.04 ends in front of Roark, just after his battle: its save at
    // 01.05's start is right after the win (2026-10-31, at the user's
    // request), so a failure later never has to beat him again.
    // No trip back to the Oreburgh Center (2026-10-31, the user): talk_or_fight()
    // gives a Potion after Roark when HP is low, and Jubilife's Center is on
    // the way. Out of the gym onto its doorstep, which is on the Center door's
    // row: west along it reaches the stairs' cliff.
    if (!dpad_out_of_building(stream, context, 40, "out of the gym")){
        fail_step(stream, "Oreburgh Gym: did not leave by its door.");
    }
}

//  01.05: up to the platform by the Oreburgh Gate and into it (Barry bumps into us there).
void into_oreburgh_gate(VideoStream& stream, ProControllerContext& context){
    // From the gym's doorstep, as walked by hand on 2026-10-29.
    walk_dir(stream, context, Direction::WEST, 20000ms, "west to the stairs' cliff");
    walk_dir(stream, context, Direction::NORTH, 10000ms, "north along the cliff");
    hold_tiles(stream, context, Direction::EAST, 2);
    walk_dir(stream, context, Direction::NORTH, 5000ms, "north up the stairs");
    walk_dir(stream, context, Direction::WEST, 6000ms, "west along the platform");
    hold_tiles(stream, context, Direction::SOUTH, 1);
    // Barry runs into us on the platform ("*thud*") the first time; his lines
    // end with no battle.
    west_until_transition(stream, context, "Into Oreburgh Gate", 8);
}

//  01.05: west through the Gate until Route 203's banner shows.
void out_of_oreburgh_gate(VideoStream& stream, ProControllerContext& context){
    // The cave's fade out of the Gate does not always read as a transition
    // (r89), so the walk west ends on Route 203's location banner, which
    // shows for a few seconds as we step out. 01.06 starts there, from a save.
    west_until_transition(stream, context, "West out of Oreburgh Gate", 8, location_banner_showing);
}

}


void add_oreburgh_events(StoryEvents& events){
    events.emplace("oreburgh_center_door", console_event(oreburgh_center_door));
    events.emplace("south_into_the_mine", console_event(south_into_the_mine));
    events.emplace("mine_hall", console_event(mine_hall));
    events.emplace("roark_in_the_mine", console_event(roark_in_the_mine));
    events.emplace("oreburgh_center_heal", console_event(oreburgh_center_heal));
    events.emplace("out_of_the_center", console_event(out_of_the_center));
    events.emplace("gym_until_roark_beaten", console_event(gym_until_roark_beaten));
    events.emplace("out_of_the_gym", console_event(out_of_the_gym));
    events.emplace("into_oreburgh_gate", console_event(into_oreburgh_gate));
    events.emplace("out_of_oreburgh_gate", console_event(out_of_oreburgh_gate));
}



}
}
}
