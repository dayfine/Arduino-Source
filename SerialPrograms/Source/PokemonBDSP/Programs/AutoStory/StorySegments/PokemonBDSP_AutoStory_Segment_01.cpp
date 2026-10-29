/*  BDSP AutoStory - Segment 01
 *  From the Jubilife Pokemon Center to Oreburgh City.
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "PokemonBDSP_AutoStory_Segment_01.h"
#include "../Data/PokemonBDSP_AutoStory_OreburghMine.h"
#include "../Data/PokemonBDSP_AutoStory_StoryMaps.h"
#include "../Detect/PokemonBDSP_AutoStory_OverworldDetector.h"
#include "../Utils/PokemonBDSP_AutoStory_RouteWalk.h"
#include "../Utils/PokemonBDSP_AutoStory_Segment_00_tools.h"
#include "../Utils/PokemonBDSP_AutoStory_Walk.h"
#include "Common/Cpp/PrettyPrint.h"
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "CommonFramework/Globals.h"

#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/VisualDetectors/BlackScreenDetector.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "PokemonBDSP/Inference/PokemonBDSP_DialogDetector.h"
#include "PokemonBDSP/Inference/Battles/PokemonBDSP_BattleMenuDetector.h"

#include <cstdlib>

using namespace std::chrono_literals;

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using Pokemon::GridDirection;


namespace{

[[noreturn]] void fail(VideoStream& stream, const std::string& message){
    OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT, message, stream);
}

//  One tile south, for a player facing another way.
void step_south(VideoStream& stream, ProControllerContext& context){
    route_turn(stream, context, GridDirection::SOUTH);
    pbf_move_left_joystick(context, {0, -1}, route_open_hold(GridDirection::SOUTH, 1), 200ms);
    context.wait_for_all_requests();
}

//  True if a dialogue opens within `timeout`: someone who stopped us as the
//  walk ended reads as BLOCKED, and their box comes up a moment later.
bool dialogue_opens(VideoStream& stream, ProControllerContext& context, Milliseconds timeout){
    context.wait_for_all_requests();
    ShortDialogWatcher dialog(COLOR_CYAN);
    return wait_until(stream, context, timeout, {{dialog}}) == 0;
}

//  For a dialogue that has opened: B through it. A trainer's ends in a
//  battle, which is fought with the best move (Potions when HP runs low),
//  followed by a Potion if HP is under half. Anyone else's ends back in the
//  overworld. Returns whether there was a battle. Between a trainer's last
//  line and the battle the overworld shows with no dialogue for a moment,
//  so "only talked" needs 4 s of it, and never comes when `battle_expected`.
bool talk_or_fight(VideoStream& stream, ProControllerContext& context, const std::string& label, bool battle_expected = false){
    ShortDialogDetector dialog;
    OverworldDetector overworld;
    BattleMenuDetector battle_menu(BattleType::TRAINER);
    BlackScreenDetector black;
    auto quiet_since = std::chrono::steady_clock::now();
    for (size_t check = 0; check < 120; check++){
        context.wait_for_all_requests();
        const auto frame = stream.video().snapshot();
        if (!frame){
            fail(stream, label + ": No video frame.");
        }
        if (battle_menu.detect(*frame.frame) || black.detect(*frame.frame)){
            route_status(stream, "Fighting " + label + ".");
            fight_trainer(stream, context, label);
            use_potion_first_pokemon(stream, context, 0.5);
            return true;
        }
        const bool talking = dialog.detect(*frame.frame);
        if (!talking && overworld.detect(*frame.frame)){
            if (!battle_expected && std::chrono::steady_clock::now() - quiet_since >= 4s){
                route_status(stream, label + " only talked.");
                return false;
            }
            pbf_wait(context, 300ms);
            continue;
        }
        quiet_since = std::chrono::steady_clock::now();
        if (talking){
            pbf_press_button(context, BUTTON_B, 80ms, 1100ms);
        }else{
            pbf_wait(context, 300ms);
        }
    }
    fail(stream, label + ": neither a battle nor the overworld after the dialogue.");
}

//  Logs a checkpoint's steps up front, then each step as it starts, with the
//  one after it, in the log and the overlay.
class StepLog{
public:
    StepLog(VideoStream& stream, std::string checkpoint, std::string intro, std::vector<std::string> steps)
        : m_stream(stream), m_checkpoint(std::move(checkpoint)), m_steps(std::move(steps))
    {
        std::string text = "[" + m_checkpoint + "] " + intro + ", the steps are:";
        for (size_t i = 0; i < m_steps.size(); i++){
            text += "\n    " + std::to_string(i + 1) + ". " + m_steps[i];
        }
        m_stream.log(text, COLOR_BLUE);
    }
    void operator()(size_t number){
        const std::string count = std::to_string(number) + "/" + std::to_string(m_steps.size());
        const bool last = number >= m_steps.size();
        m_stream.log("[" + m_checkpoint + " step " + count + "] " + m_steps[number - 1] +
            (last ? std::string(" -- last step") : " -- next: " + m_steps[number]), COLOR_BLUE);
        add_wrapped_log(m_stream.overlay(), m_checkpoint + " step " + count + ": " + m_steps[number - 1], COLOR_WHITE);
        if (!last){
            add_wrapped_log(m_stream.overlay(), "Next: " + m_steps[number], COLOR_GRAY);
        }
    }

private:
    VideoStream& m_stream;
    std::string m_checkpoint;
    std::vector<std::string> m_steps;
};

void jubilife_to_barry(VideoStream& stream, ProControllerContext& context){
    // What 01.01 does, stepped through on the console before writing it.
    StepLog step(stream, "01.01", "From the Jubilife Pokemon Center to Barry", {
        "Heal at the nurse, then walk out of the Pokemon Center",
        "Walk to Jubilife's east exit and battle Barry at the Route 203 entrance",
    });

    step(1);
    // The save stands us at the PC, right of the counter, facing it; west
    // is the counter's end. Two tiles south clears it, then three west is
    // the floor emblem's column, with the nurse north and the door south.
    route_turn(stream, context, GridDirection::SOUTH);
    pbf_move_left_joystick(context, {0, -1}, route_open_hold(GridDirection::SOUTH, 2), 200ms);
    route_turn(stream, context, GridDirection::WEST);
    pbf_move_left_joystick(context, {-1, 0}, route_open_hold(GridDirection::WEST, 3), 200ms);
    if (walk_to_talk(stream, context, 0, 1, 3000ms, "to the nurse").stop != WalkStop::DIALOG){
        fail(stream, "Jubilife Pokemon Center: the nurse did not answer.");
    }
    // Two lines, then Yes to "rest your Pokemon?" (B would answer No), and
    // one more A for "I'll take your Pokemon"; B for the rest.
    finish_dialogue(stream, context, 4, "Nurse");
    // Healing shows no text box for a few seconds, which reads as the end;
    // her "Thank you for waiting" lines follow it.
    if (dialogue_opens(stream, context, 6000ms)){
        finish_dialogue(stream, context, 0, "Nurse, after healing");
    }
    // South through the door until a girl standing below it stops us. The
    // door's fade may or may not read as a transition.
    WalkResult out{WalkStop::LIMIT, 0ms, 0};
    for (size_t walk = 0; walk < 3 && out.stop != WalkStop::BLOCKED; walk++){
        out = walk_until(stream, context, 0, -1, 8000ms, "out of the Pokemon Center");
        if (out.stop == WalkStop::DIALOG || out.stop == WalkStop::ARRIVED){
            break;
        }
    }
    if (out.stop != WalkStop::BLOCKED){
        fail(stream, std::string("Jubilife Pokemon Center: walking out ended with ") + walk_stop_name(out.stop) + ", not at the girls below the door.");
    }

    step(2);
    // Jubilife's plaza has too few walls the grid gets right to plan on (a
    // leg to a wall the map shows walked on 19 tiles). These are the holds
    // that worked on the console: west onto the main road, north to the
    // street east to Route 203, and back a couple of tiles.
    route_turn(stream, context, GridDirection::WEST);
    pbf_move_left_joystick(context, {-1, 0}, 720ms, 300ms);
    route_turn(stream, context, GridDirection::NORTH);
    pbf_move_left_joystick(context, {0, 1}, 3960ms, 300ms);
    route_turn(stream, context, GridDirection::SOUTH);
    pbf_move_left_joystick(context, {0, -1}, 600ms, 300ms);
    context.wait_for_all_requests();
    // East along the street. The opening to the route is a few rows high;
    // Barry stops us a little way in. A wall means we are a row off it: try
    // rows to the south, then to the north.
    static const int ROW_CHANGES[] = {0, 1, 1, -3, -1};
    bool barry = false;
    for (int change : ROW_CHANGES){
        if (barry){
            break;
        }
        for (int i = 0; i < std::abs(change); i++){
            const GridDirection dir = change > 0 ? GridDirection::SOUTH : GridDirection::NORTH;
            route_turn(stream, context, dir);
            pbf_move_left_joystick(context, {0, change > 0 ? -1.0 : 1.0}, route_open_hold(dir, 1), 200ms);
        }
        context.wait_for_all_requests();
        WalkResult walk{WalkStop::LIMIT, 0ms, 0};
        for (size_t stretch = 0; stretch < 3 && walk.stop == WalkStop::LIMIT; stretch++){
            walk = walk_until(stream, context, 1, 0, 9000ms, "east to Route 203");
        }
        barry = walk.stop == WalkStop::DIALOG;
    }
    if (!barry){
        fail(stream, "Route 203: Barry did not stop us at the entrance.");
    }
    if (!talk_or_fight(stream, context, "Barry", true)){
        fail(stream, "Route 203: Barry talked but did not battle.");
    }
}

void route203_to_oreburgh(VideoStream& stream, ProControllerContext& context){
    StepLog step(stream, "01.02", "From Barry on Route 203 to Oreburgh City", {
        "Walk east along Route 203 into the Oreburgh Gate, fighting the trainers who see us",
        "Listen to the Hiker, who gives HM Rock Smash",
        "Walk east through the Gate past Curtis and Diana, out to Oreburgh City",
        "Listen to the man who meets us at the city's entrance",
    });

    step(1);
    // Route 203 only goes east, over three stretches joined by stairs, and
    // the Gate's opening is in the cliff at its north-east end. Who stops us
    // differs from run to run (a trainer's sight depends on our row), so
    // there is no fixed list: walk east, talk or fight whoever stops us, and
    // at a wall find a row from which east goes on.
    size_t fights = 0;
    bool inside = false;
    //  On Route 203 only trainers start a talk; one with no battle is the
    //  Hiker, who meets us inside the Gate (whose fade may not read as a
    //  transition).
    bool met_hiker = false;
    auto stopped_by = [&](){
        if (talk_or_fight(stream, context, "Route 203 trainer " + std::to_string(fights + 1))){
            fights++;
        }else{
            route_status(stream, "No battle: that was the Hiker, inside the Oreburgh Gate.");
            met_hiker = true;
            inside = true;
        }
    };
    for (size_t leg = 0; leg < 40 && !inside; leg++){
        const WalkResult walk = walk_until(stream, context, 1, 0, 10000ms, "east along Route 203");
        if (walk.stop == WalkStop::TRANSITION){
            inside = true;
            break;
        }
        if (walk.stop == WalkStop::LIMIT){
            continue;
        }
        if (walk.stop == WalkStop::DIALOG || dialogue_opens(stream, context, 1500ms)){
            stopped_by();
            continue;
        }
        // A wall. Find the row from which east goes on: north to the top wall
        // first (a step north there does not move us, so rows are counted
        // from it), then a row at a time south. At the route's end this is the
        // corner by the second sign, with the Gate's opening two rows down.
        walk_until(stream, context, 0, 1, 6000ms, "north to the top wall");
        bool moved_on = false;
        for (size_t row = 1; row <= 10 && !moved_on; row++){
            step_south(stream, context);
            const WalkResult next = walk_until(stream, context, 1, 0, 10000ms, "east along Route 203, row " + std::to_string(row) + " below the top");
            if (next.stop == WalkStop::TRANSITION){
                inside = true;
            }
            if (next.stop == WalkStop::DIALOG || (next.stop == WalkStop::BLOCKED && dialogue_opens(stream, context, 1500ms))){
                stopped_by();
            }
            //  Two still chunks are the least a wall takes; more is progress.
            moved_on = inside || next.stop != WalkStop::BLOCKED || next.walked > 1200ms;
        }
        if (!moved_on){
            fail(stream, "Route 203: walls east on every row down to ten below the top wall.");
        }
    }
    if (!inside){
        fail(stream, "Route 203: did not reach the Oreburgh Gate.");
    }
    route_status(stream, "Route 203: " + std::to_string(fights) + " trainer battle(s) on the way.");
    context.wait_for(1500ms);

    step(2);
    // The Hiker steps up to us as we walk east.
    if (!met_hiker){
        const WalkResult to_hiker = walk_until(stream, context, 1, 0, 4000ms, "to the Hiker");
        if (to_hiker.stop != WalkStop::DIALOG && !dialogue_opens(stream, context, 2000ms)){
            fail(stream, "Oreburgh Gate: the Hiker did not stop us.");
        }
        finish_dialogue(stream, context, 0, "Hiker");
    }
    // Then a "Hidden Moves" tutorial card, closed with B. It is no dialogue
    // box, and it can come up a moment after his last line.
    pbf_wait(context, 1500ms);
    pbf_press_button(context, BUTTON_B, 80ms, 1500ms);
    pbf_press_button(context, BUTTON_B, 80ms, 1000ms);
    context.wait_for_all_requests();

    step(3);
    // East: Camper Curtis and Picnicker Diana see us; past them the exit's
    // white fade, and the man in Oreburgh stops us right outside.
    fights = 0;
    bool city = false;
    size_t stuck = 0;
    for (size_t leg = 0; leg < 12 && !city; leg++){
        const WalkResult walk = walk_until(stream, context, 1, 0, 10000ms, "east through the Oreburgh Gate");
        if (walk.stop == WalkStop::LIMIT || walk.stop == WalkStop::TRANSITION){
            continue;
        }
        if (walk.stop != WalkStop::DIALOG && !dialogue_opens(stream, context, 2000ms)){
            // A card or menu left up holds the screen still: B once, then retry.
            if (++stuck > 1){
                fail(stream, "Oreburgh Gate: a wall stopped us walking east.");
            }
            pbf_press_button(context, BUTTON_B, 80ms, 1500ms);
            continue;
        }
        stuck = 0;
        if (fights >= 2){
            city = true;
            break;
        }
        fights += talk_or_fight(stream, context, "Oreburgh Gate trainer " + std::to_string(fights + 1)) ? 1 : 0;
    }
    if (!city){
        fail(stream, "Oreburgh Gate: did not reach Oreburgh City.");
    }

    step(4);
    // He walks us to the Pokemon Center between his lines, and Barry joins
    // there: more lines after each walk.
    finish_dialogue(stream, context, 0, "Man at Oreburgh's entrance");
    for (size_t scene = 0; scene < 6 && dialogue_opens(stream, context, 6000ms); scene++){
        finish_dialogue(stream, context, 0, "Oreburgh scene");
    }
    context.wait_for_all_requests();
}

//  Holds the stick toward `dir` for `tiles` tiles, turning first.
void hold_tiles(VideoStream& stream, ProControllerContext& context, GridDirection dir, size_t tiles){
    route_turn(stream, context, dir);
    const double x = dir == GridDirection::EAST ? 1 : dir == GridDirection::WEST ? -1 : 0;
    const double y = dir == GridDirection::NORTH ? 1 : dir == GridDirection::SOUTH ? -1 : 0;
    pbf_move_left_joystick(context, {x, y}, route_open_hold(dir, tiles), 200ms);
    context.wait_for_all_requests();
}

WalkResult walk_dir(VideoStream& stream, ProControllerContext& context, GridDirection dir, Milliseconds limit, const std::string& label){
    const double x = dir == GridDirection::EAST ? 1 : dir == GridDirection::WEST ? -1 : 0;
    const double y = dir == GridDirection::NORTH ? 1 : dir == GridDirection::SOUTH ? -1 : 0;
    return walk_until(stream, context, x, y, limit, label);
}

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
    hold_tiles(stream, context, GridDirection::EAST, 3);
    walk_dir(stream, context, GridDirection::NORTH, 3000ms, "north to the Center's front");
    hold_tiles(stream, context, GridDirection::EAST, 2);
    walk_dir(stream, context, GridDirection::NORTH, 2500ms, "north beside Barry");
    if (walk_dir(stream, context, GridDirection::SOUTH, 8000ms, "south to the stone ledge").stop != WalkStop::BLOCKED){
        fail(stream, "Oreburgh: walking south did not stop at the stone ledge.");
    }
    // A townsperson wandering across the row can stop us a tile or two short
    // (r95), so walk east again until a walk stops without moving.
    bool at_fence = false;
    for (size_t attempt = 0; attempt < 5 && !at_fence; attempt++){
        const WalkResult east = walk_dir(stream, context, GridDirection::EAST, 10000ms, "east to the fence below the Poke Mart");
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
        down = walk_dir(stream, context, GridDirection::SOUTH, 12000ms, "south to the right-hand pillar");
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
    auto step_tiles = [&](GridDirection side, size_t tiles){
        route_turn(stream, context, side);
        for (size_t tile = 0; tile < tiles; tile++){
            pbf_move_left_joystick(context, {side == GridDirection::EAST ? 1.0 : -1.0, 0}, 120ms, 300ms);
        }
        context.wait_for_all_requests();
    };
    if (!in_mine){
        walk_dir(stream, context, GridDirection::WEST, 2500ms, "west to the left-hand pillars");
        step_tiles(GridDirection::EAST, 1);
        in_mine = walk_dir(stream, context, GridDirection::SOUTH, 15000ms, "south into the mine").stop == WalkStop::TRANSITION;
    }
    if (!in_mine){
        step_tiles(GridDirection::WEST, 2);     // the sign's other side
        in_mine = walk_dir(stream, context, GridDirection::SOUTH, 5000ms, "south down the mine's stairs").stop == WalkStop::TRANSITION;
    }
    if (!in_mine){
        fail(stream, "Oreburgh: no column south of the Center led into the mine.");
    }
    context.wait_for(1500ms);

    step(3);
    // If the fade into the mine was missed, we are already in the corridor
    // and this walk stops at its boulder; otherwise it crosses the hall.
    const WalkResult hall = walk_dir(stream, context, GridDirection::SOUTH, 10000ms, "through the mine's entrance hall");
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
    walk_dir(stream, context, GridDirection::NORTH, 2000ms, "north to the wall on Roark's row");
    if (walk_dir(stream, context, GridDirection::WEST, 3000ms, "west to the rock beside Roark").stop != WalkStop::BLOCKED){
        fail(stream, "Oreburgh Mine: walking west along Roark's row did not stop at his rock.");
    }
    if (walk_dir(stream, context, GridDirection::SOUTH, 2000ms, "south to the bottom wall").stop != WalkStop::BLOCKED){
        fail(stream, "Oreburgh Mine: walking south from Roark's rock did not stop at the bottom wall.");
    }
    route_turn(stream, context, GridDirection::WEST);
    pbf_move_left_joystick(context, {-1, 0}, 120ms, 300ms);
    pbf_move_left_joystick(context, {-1, 0}, 120ms, 300ms);
    route_turn(stream, context, GridDirection::NORTH);
    context.wait_for_all_requests();
    //  Where we stand to talk to him, to check against the plan after a run.
    if (VideoSnapshot screen = stream.video().snapshot()){
        const std::string folder = SCREENSHOTS_PATH() + "AutoStory/route/";
        Filesystem::create_directories(Filesystem::Path(folder));
        screen.frame->save(folder + now_to_filestring() + "-below-roark.png");
    }
    pbf_press_button(context, BUTTON_A, 80ms, 200ms);
    if (!dialogue_opens(stream, context, 2000ms)){
        fail(stream, "Oreburgh Mine: no talk opened below Roark.");
    }
    at = {roark.next(GridDirection::SOUTH)};
    finish_dialogue(stream, context, 0, "Roark");

    step(5);
    // Back to the top of the corridor, then north out through the hall.
    const Pokemon::TilePos exit = world(OreburghMine::corridor_top()[1]);
    at = walk_route(stream, context, maps, at,
        [&](const Pokemon::TilePos& pos){ return pos.y == exit.y && pos.x >= exit.x - 1 && pos.x <= exit.x + 1; },
        no_trainers, in_the_way, "Back to the corridor's top");
    if (walk_dir(stream, context, GridDirection::NORTH, 10000ms, "into the entrance hall").stop != WalkStop::TRANSITION){
        fail(stream, "Oreburgh Mine: walking north from the corridor did not reach the entrance hall.");
    }
    context.wait_for(1500ms);
    if (walk_dir(stream, context, GridDirection::NORTH, 10000ms, "out of the mine").stop != WalkStop::TRANSITION){
        fail(stream, "Oreburgh Mine: did not leave by the entrance.");
    }
    context.wait_for(1500ms);
}

void checkpoint_0101(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    AutoStoryOptions options,
    AutoStoryStats& stats
){
    checkpoint_reattempt_loop(env, context, options, stats,
        [&](size_t /*attempt*/){
            jubilife_to_barry(env.console, context);
        },
        true
    );
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

void checkpoint_0102(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    AutoStoryOptions options,
    AutoStoryStats& stats
){
    checkpoint_reattempt_loop(env, context, options, stats,
        [&](size_t /*attempt*/){
            route203_to_oreburgh(env.console, context);
        },
        true
    );
}
}


std::string AutoStory_Segment_01::name()       const { return "01: Jubilife to Oreburgh"; }
std::string AutoStory_Segment_01::start_text() const { return "Start: Standing in front of the Jubilife City Pokemon Center PC."; }
std::string AutoStory_Segment_01::end_text()   const { return "End: In Oreburgh City, after the man at the entrance."; }

void AutoStory_Segment_01::run_segment(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    AutoStoryOptions options,
    AutoStoryStats& stats
) const{
    stats.m_segment++;
    env.update_stats();
    env.console.log("Start Segment " + name(), COLOR_ORANGE);
    AutoStory_Checkpoint_0101().run_checkpoint(env, context, options, stats);
    AutoStory_Checkpoint_0102().run_checkpoint(env, context, options, stats);
    AutoStory_Checkpoint_0103().run_checkpoint(env, context, options, stats);
    env.console.log("End Segment " + name(), COLOR_GREEN);
}

std::string AutoStory_Checkpoint_0101::name()       const { return "01.01 - Jubilife to Barry"; }
std::string AutoStory_Checkpoint_0101::start_text() const { return "Standing in front of the Jubilife City Pokemon Center PC."; }
std::string AutoStory_Checkpoint_0101::end_text()   const { return "On Route 203 by its sign, after beating Barry."; }

void AutoStory_Checkpoint_0101::run_checkpoint(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    AutoStoryOptions options,
    AutoStoryStats& stats
) const{
    checkpoint_0101(env, context, options, stats);
}

std::string AutoStory_Checkpoint_0102::name()       const { return "01.02 - Route 203 to Oreburgh"; }
std::string AutoStory_Checkpoint_0102::start_text() const { return AutoStory_Checkpoint_0101().end_text(); }
std::string AutoStory_Checkpoint_0102::end_text()   const { return "In front of the Oreburgh Pokemon Center, after the man's tour and Barry."; }

void AutoStory_Checkpoint_0102::run_checkpoint(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    AutoStoryOptions options,
    AutoStoryStats& stats
) const{
    checkpoint_0102(env, context, options, stats);
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
