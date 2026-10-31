/*  BDSP AutoStory - Segment 01
 *  From the Jubilife Pokemon Center to Oreburgh City.
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "PokemonBDSP_AutoStory_Segment_01.h"
#include "../Data/PokemonBDSP_AutoStory_OreburghMine.h"
#include "../Data/PokemonBDSP_AutoStory_RavagedPath.h"
#include "../Data/PokemonBDSP_AutoStory_Route204.h"
#include "../Data/PokemonBDSP_AutoStory_StoryMaps.h"
#include "../Detect/PokemonBDSP_AutoStory_OverworldDetector.h"
#include "../Utils/PokemonBDSP_AutoStory_PokemonCenter.h"
#include "../Utils/PokemonBDSP_AutoStory_RouteWalk.h"
#include "../Utils/PokemonBDSP_AutoStory_Segment_00_tools.h"
#include "../Utils/PokemonBDSP_AutoStory_Walk.h"
#include "Common/Cpp/PrettyPrint.h"
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "CommonFramework/Globals.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"

#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/VisualDetectors/BlackScreenDetector.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "PokemonBDSP/Inference/PokemonBDSP_DialogDetector.h"
#include "PokemonBDSP/Inference/Battles/PokemonBDSP_BattleMenuDetector.h"

#include <algorithm>
#include <cstdlib>

using namespace std::chrono_literals;

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using Pokemon::Direction;


namespace{

[[noreturn]] void fail(VideoStream& stream, const std::string& message){
    OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT, message, stream);
}

//  One tile south, for a player facing another way.
void step_south(VideoStream& stream, ProControllerContext& context){
    route_turn(stream, context, Direction::SOUTH);
    pbf_move_left_joystick(context, {0, -1}, route_open_hold(Direction::SOUTH, 1), 200ms);
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
//  `doubles` for tag and twin battles. A lost battle ends in the last
//  Pokemon Center; see in_pokemon_center().
bool in_pokemon_center(VideoStream& stream);
bool talk_or_fight(VideoStream& stream, ProControllerContext& context, const std::string& label, bool battle_expected = false, bool doubles = false){
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
            fight_story_battle(stream, context, label, doubles);
            if (!in_pokemon_center(stream)){
                use_potion_first_pokemon(stream, context, 0.5);
            }
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

    // From 00.05g we are already outdoors on the street east to Route 203,
    // healed on arriving in Jubilife: straight to the walk east.
    bool outdoors = false;
    context.wait_for_all_requests();
    if (VideoSnapshot screen = stream.video().snapshot()){
        outdoors = !indoors(*screen.frame);
    }
    if (outdoors){
        stream.log("[Route] 01.01: outdoors; skipping the Pokemon Center.");
    }else{
    step(1);
    // The save stands us at the PC, right of the counter, facing it; west
    // is the counter's end. Two tiles south clears it, then three west is
    // the floor emblem's column, with the nurse north and the door south.
    route_turn(stream, context, Direction::SOUTH);
    pbf_move_left_joystick(context, {0, -1}, route_open_hold(Direction::SOUTH, 2), 200ms);
    route_turn(stream, context, Direction::WEST);
    pbf_move_left_joystick(context, {-1, 0}, route_open_hold(Direction::WEST, 3), 200ms);
    if (walk_to_talk(stream, context, 0, 1, 3000ms, "to the nurse").stop != WalkStop::DIALOG){
        fail(stream, "Jubilife Pokemon Center: the nurse did not answer.");
    }
    // A throughout: Yes to "rest your Pokemon?" (B would answer No).
    finish_dialogue(stream, context, BUTTON_A, "Nurse");
    // Healing shows no text box for a few seconds, which reads as the end;
    // her "Thank you for waiting" lines follow it.
    if (dialogue_opens(stream, context, 6000ms)){
        finish_dialogue(stream, context, BUTTON_B, "Nurse, after healing");
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
    route_turn(stream, context, Direction::WEST);
    pbf_move_left_joystick(context, {-1, 0}, 720ms, 300ms);
    route_turn(stream, context, Direction::NORTH);
    pbf_move_left_joystick(context, {0, 1}, 3960ms, 300ms);
    route_turn(stream, context, Direction::SOUTH);
    pbf_move_left_joystick(context, {0, -1}, 600ms, 300ms);
    context.wait_for_all_requests();
    }
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
            const Direction dir = change > 0 ? Direction::SOUTH : Direction::NORTH;
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
        finish_dialogue(stream, context, BUTTON_B, "Hiker");
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
    finish_dialogue(stream, context, BUTTON_B, "Man at Oreburgh's entrance");
    for (size_t scene = 0; scene < 6 && dialogue_opens(stream, context, 6000ms); scene++){
        finish_dialogue(stream, context, BUTTON_B, "Oreburgh scene");
    }
    context.wait_for_all_requests();
}

//  Holds the stick toward `dir` for `tiles` tiles, turning first.
void hold_tiles(VideoStream& stream, ProControllerContext& context, Direction dir, size_t tiles){
    route_turn(stream, context, dir);
    const double x = dir == Direction::EAST ? 1 : dir == Direction::WEST ? -1 : 0;
    const double y = dir == Direction::NORTH ? 1 : dir == Direction::SOUTH ? -1 : 0;
    pbf_move_left_joystick(context, {x, y}, route_open_hold(dir, tiles), 200ms);
    context.wait_for_all_requests();
}

WalkResult walk_dir(VideoStream& stream, ProControllerContext& context, Direction dir, Milliseconds limit, const std::string& label){
    const double x = dir == Direction::EAST ? 1 : dir == Direction::WEST ? -1 : 0;
    const double y = dir == Direction::NORTH ? 1 : dir == Direction::SOUTH ? -1 : 0;
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
    hold_tiles(stream, context, Direction::EAST, 3);
    walk_dir(stream, context, Direction::NORTH, 3000ms, "north to the Center's front");
    hold_tiles(stream, context, Direction::EAST, 2);
    walk_dir(stream, context, Direction::NORTH, 2500ms, "north beside Barry");
    // Roark answers in the mine only after Barry has told us he is there
    // (2026-10-29: he ignored A in r101 and r104 and by hand, and answered at
    // once after a talk with Barry). Barry is just east of us.
    route_turn(stream, context, Direction::EAST);
    // A save made after his talk has him gone (2026-10-31, the save in front
    // of the Center was remade after it): go on, and if he never told us,
    // Roark not answering says so.
    pbf_press_button(context, BUTTON_A, 80ms, 200ms);
    if (dialogue_opens(stream, context, 3000ms)){
        finish_dialogue(stream, context, BUTTON_B, "Barry at the gym door");
    }else{
        stream.log("[01.03] Barry did not talk at the gym door: he has told us already. Going on.", COLOR_ORANGE);
    }
    // The ledge and the fence end these legs (see walk_to_wall()).
    walk_to_wall(stream, context, 0, -1, 8000ms, "south to the stone ledge");
    // A townsperson wandering across the row can stop us a tile or two short
    // (r95): after a pause, hold east again; against the fence that does
    // nothing. Whether that second walk moved cannot be read from the screen
    // with people about (s03b2: 6 to 12 standing still, so "blocked" took
    // 2 s every time), so neither walk is checked for it.
    for (size_t push = 0; push < 2; push++){
        walk_to_wall(stream, context, 1, 0, push == 0 ? 10000ms : 4000ms, "east to the fence below the Poke Mart");
        if (push == 0){
            context.wait_for(1500ms);
        }
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
    walk_to_wall(stream, context, 0, 1, 2000ms, "north to the wall on Roark's row");
    // His rock and the bottom wall end these legs (see walk_to_wall()). A
    // wild battle on the way eats into the walk's time (r100): walk again.
    for (size_t attempt = 0; attempt < 3; attempt++){
        if (walk_to_wall(stream, context, -1, 0, 6000ms, "west to the rock beside Roark").wild_battles == 0){
            break;
        }
    }
    walk_to_wall(stream, context, 0, -1, 2000ms, "south to the bottom wall");
    route_turn(stream, context, Direction::WEST);
    pbf_move_left_joystick(context, {-1, 0}, 120ms, 300ms);
    pbf_move_left_joystick(context, {-1, 0}, 120ms, 300ms);
    route_turn(stream, context, Direction::NORTH);
    context.wait_for_all_requests();
    if (VideoSnapshot screen = stream.video().snapshot()){
        const std::string folder = SCREENSHOTS_PATH() + "AutoStory/route/";
        Filesystem::create_directories(Filesystem::Path(folder));
        screen.frame->save(folder + now_to_filestring() + "-below-roark.png");
    }
    // The two taps west land within a third of a tile or so of his column
    // (s03e2 missed him by that much), so look for him with nudges either
    // side. The camera zooms in and his first line, "OK, stand back and
    // watch this.", can take some seconds (r98).
    if (!talk_with_nudges(stream, context, Direction::NORTH, "Roark", 8000ms)){
        fail(stream, "Oreburgh Mine: no talk opened below Roark (has Barry told us about the mine?).");
    }
    finish_dialogue(stream, context, BUTTON_B, "Roark");
    // His scene ends with him smashing his rock and leaving: his tile is
    // free then. The smash plays with no dialogue box, which can end the talk
    // early, and he is still there (r179a): until his tile is free, finish
    // any lines that come back and try again.
    bool gone = false;
    for (size_t attempt = 0; attempt < 5 && !gone; attempt++){
        if (dialogue_opens(stream, context, 3000ms)){
            finish_dialogue(stream, context, BUTTON_B, "Roark, after the smash");
        }
        gone = walk_dir(stream, context, Direction::NORTH, 2000ms, "onto Roark's tile").walked > 800ms;
    }
    if (!gone){
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


//  A rough check that we stand in a Pokemon Center: its floor fills the lower
//  half of the screen with warm yellow, where the gym is grey and routes are
//  green. After a battle it tells a loss (a whiteout ends at the counter)
//  from a win.
bool in_pokemon_center(VideoStream& stream){
    const VideoSnapshot frame = stream.video().snapshot();
    if (!frame){
        return false;
    }
    const ImageRGB32 small = frame.frame->scale_to(192, 108);
    size_t warm = 0, total = 0;
    for (size_t y = 54; y < 108; y++){
        for (size_t x = 0; x < 192; x++){
            const uint32_t pixel = small.pixel(x, y);
            const int r = (pixel >> 16) & 0xff, g = (pixel >> 8) & 0xff, b = pixel & 0xff;
            total++;
            if (r > 200 && g > 150 && b < 150 && r - b > 70){
                warm++;
            }
        }
    }
    const bool inside = warm * 10 > total * 3;
    stream.log("[Center check] warm floor " + std::to_string(warm * 100 / total) + " %: " +
        (inside ? "in a Pokemon Center." : "not in a Pokemon Center."));
    return inside;
}

//  For a player inside a Pokemon Center after a whiteout, at the counter:
//  out through the door. The door is straight south of the nurse in most
//  Centers; in Oreburgh's it was two tiles east of where we stood (by hand).
void walk_out_of_center(VideoStream& stream, ProControllerContext& context, const std::string& label){
    finish_dialogue(stream, context, BUTTON_B, label + ": the nurse");
    const int shifts[] = {0, 2, -4};
    for (int shift : shifts){
        if (shift != 0){
            hold_tiles(stream, context, shift > 0 ? Direction::EAST : Direction::WEST, (size_t)std::abs(shift));
        }
        if (walk_dir(stream, context, Direction::SOUTH, 8000ms, label + ": out of the Center").stop == WalkStop::TRANSITION){
            context.wait_for(2000ms);
            return;
        }
    }
    fail(stream, label + ": could not find the Pokemon Center's door.");
}

//  Oreburgh, from the street in front of the Pokemon Center (its door or its
//  front wall) into the gym, as walked by hand on 2026-10-29: west to the
//  cliff, south to the posts, a row up, then 17 tiles east is the gym door's
//  column. The hold is checked by the door itself: walk north, and if the
//  gym's wall stops us, try the columns either side.
void oreburgh_street_to_gym(VideoStream& stream, ProControllerContext& context){
    walk_to_wall(stream, context, -1, 0, 15000ms, "west to the cliff");
    walk_to_wall(stream, context, 0, -1, 3000ms, "south to the posts");
    hold_tiles(stream, context, Direction::NORTH, 1);
    hold_tiles(stream, context, Direction::EAST, 17);
    // Cumulative steps: here, a tile east, a tile west, two east, two west.
    const int steps[] = {0, 1, -2, 3, -4};
    for (int step : steps){
        if (step != 0){
            hold_tiles(stream, context, step > 0 ? Direction::EAST : Direction::WEST, (size_t)std::abs(step));
        }
        const WalkResult door = walk_dir(stream, context, Direction::NORTH, 4000ms, "north into the gym");
        // The door's fade can read as the time limit rather than a black
        // screen (by hand: "limit after 4000ms", and we were inside).
        if (door.stop == WalkStop::TRANSITION || door.stop == WalkStop::LIMIT){
            context.wait_for(1000ms);
            return;
        }
        if (door.stop == WalkStop::DIALOG){
            talk_or_fight(stream, context, "Someone outside the gym");
        }
    }
    fail(stream, "Oreburgh: no column near the gym's door went in.");
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
        const WalkResult up = walk_dir(stream, context, Direction::NORTH, 8000ms, "north up the gym");
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
            if (talk_or_fight(stream, context, "Roark")){
                if (in_pokemon_center(stream)){
                    return false;
                }
                finish_dialogue(stream, context, BUTTON_B, "Roark after the battle");
                return true;
            }
            continue;
        }
        if (up.walked <= 800ms && ++stuck >= 3){
            fail(stream, "Oreburgh Gym: stuck on the way up, and nobody answers A.");
        }
    }
    fail(stream, "Oreburgh Gym: did not reach Roark.");
}

void oreburgh_gym(VideoStream& stream, ProControllerContext& context){
    StepLog step(stream, "01.04", "From the Oreburgh Mine to the Coal Badge", {
        "North to the Pokemon Center's front",
        "West to the cliff, south to the posts, east along the street into the gym",
        "Up the gym, fighting its trainers, and beat Roark (after a loss: from the Center, again)",
        "Out of the gym",
    });
    step(1);
    // 01.03 leaves us below the mine's sign; a tile east is the column
    // straight up to the Center (by hand: blocked at its front after 6.4 s).
    hold_tiles(stream, context, Direction::EAST, 1);
    walk_to_wall(stream, context, 0, 1, 12000ms, "north to the Center's front");

    // Losing costs some money and keeps the experience, so each try comes
    // back stronger: Roark won twice by hand against Piplup Lv 10 and 14 and
    // lost the third time.
    bool won = false;
    for (size_t attempt = 0; attempt < 5 && !won; attempt++){
        step(2);
        oreburgh_street_to_gym(stream, context);
        step(3);
        won = oreburgh_gym_battles(stream, context);
        if (!won){
            stream.log("[01.04] Lost in the gym (try " + std::to_string(attempt + 1) + "); back from the Center.", COLOR_ORANGE);
            walk_out_of_center(stream, context, "Oreburgh");
        }
    }
    if (!won){
        fail(stream, "Oreburgh Gym: lost to Roark five times.");
    }

    step(4);
    // Back down the middle and out of the door.
    for (size_t leg = 0; leg < 4; leg++){
        const WalkResult down = walk_dir(stream, context, Direction::SOUTH, 10000ms, "south out of the gym");
        if (down.stop == WalkStop::TRANSITION){
            context.wait_for(2000ms);
            return;
        }
        if (down.stop == WalkStop::DIALOG){
            finish_dialogue(stream, context, BUTTON_B, "Someone in the gym");
        }
    }
    fail(stream, "Oreburgh Gym: did not leave by its door.");
}

//  West until a transition, by the user's rule for a wall in the way: walk
//  north to the top wall, which pins the row, then try the rows south of it
//  one at a time. Trainers who stop us are fought. Returns at the first
//  transition (a building, a cave, a gate).
void west_until_transition(VideoStream& stream, ProControllerContext& context, const std::string& label, size_t max_legs){
    for (size_t leg = 0; leg < max_legs; leg++){
        const WalkResult west = walk_dir(stream, context, Direction::WEST, 20000ms, label);
        if (west.stop == WalkStop::TRANSITION){
            context.wait_for(2000ms);
            return;
        }
        if (west.stop == WalkStop::DIALOG){
            talk_or_fight(stream, context, label + ": someone in the way");
            continue;
        }
        if (west.stop != WalkStop::BLOCKED){
            continue;
        }
        walk_dir(stream, context, Direction::NORTH, 4000ms, label + ": north to the top wall");
        bool moved = false;
        for (size_t row = 0; row < 10 && !moved; row++){
            hold_tiles(stream, context, Direction::SOUTH, 1);
            const WalkResult probe = walk_dir(stream, context, Direction::WEST, 1500ms, label + ": west on the next row");
            moved = probe.stop != WalkStop::BLOCKED || probe.walked > 1200ms;
            if (probe.stop == WalkStop::TRANSITION){
                context.wait_for(2000ms);
                return;
            }
            if (probe.stop == WalkStop::DIALOG){
                talk_or_fight(stream, context, label + ": someone in the way");
            }
        }
        if (!moved){
            fail(stream, label + ": no row below the top wall leads west.");
        }
    }
    fail(stream, label + ": no transition after " + std::to_string(max_legs) + " legs.");
}

void oreburgh_to_floaroma(VideoStream& stream, ProControllerContext& context){
    StepLog step(stream, "01.05", "From the Oreburgh Gym to Floaroma Town", {
        "Up to the platform by Oreburgh Gate and into it (Barry bumps into us there)",
        "Through the Gate and west along Route 203 and Jubilife to the Route 218 gate",
        "Back east to Jubilife's north road and up it (Rowan, Dawn and two Galactic grunts: a tag battle)",
        "North on Route 204 into the Ravaged Path",
        "Through the Ravaged Path, smashing its rocks",
        "Route 204 north on its grid, past the twins, into Floaroma",
        "Heal at the Floaroma Pokemon Center",
    });

    step(1);
    // From the gym door, as walked by hand on 2026-10-29.
    hold_tiles(stream, context, Direction::SOUTH, 1);
    hold_tiles(stream, context, Direction::NORTH, 1);
    walk_dir(stream, context, Direction::WEST, 20000ms, "west to the stairs' cliff");
    walk_dir(stream, context, Direction::NORTH, 10000ms, "north along the cliff");
    hold_tiles(stream, context, Direction::EAST, 2);
    walk_dir(stream, context, Direction::NORTH, 5000ms, "north up the stairs");
    walk_dir(stream, context, Direction::WEST, 6000ms, "west along the platform");
    hold_tiles(stream, context, Direction::SOUTH, 1);
    // Barry runs into us on the platform ("*thud*") the first time; his lines
    // end with no battle.
    west_until_transition(stream, context, "Into Oreburgh Gate", 8);

    step(2);
    // The Gate's cave, Route 203 (trainers stop us) and Jubilife's road all
    // run west; the first door at the end is the Route 218 gate on
    // Jubilife's west edge (by hand: twice).
    west_until_transition(stream, context, "West to the Route 218 gate", 16);

    step(3);
    // Back out east into Jubilife: its north road is columns 43 to 46 of the
    // Jubilife grid, about 40 tiles east of the gate. Rowan's scene starts
    // on the way up it; a wall in a few tiles means we are off its column.
    if (walk_dir(stream, context, Direction::EAST, 6000ms, "back out of the gate").stop != WalkStop::TRANSITION){
        fail(stream, "Jubilife: did not leave the Route 218 gate eastwards.");
    }
    context.wait_for(2000ms);
    hold_tiles(stream, context, Direction::EAST, 40);
    const int steps[] = {0, 1, -2, 3, -4, 5, -6};
    bool scene = false;
    for (int s_step : steps){
        if (s_step != 0){
            hold_tiles(stream, context, s_step > 0 ? Direction::EAST : Direction::WEST, (size_t)std::abs(s_step));
        }
        const WalkResult up = walk_dir(stream, context, Direction::NORTH, 15000ms, "north up Jubilife's north road");
        if (up.stop == WalkStop::DIALOG){
            scene = true;
            break;
        }
        walk_dir(stream, context, Direction::SOUTH, 15000ms, "back down to the street");
    }
    if (!scene){
        fail(stream, "Jubilife: no column led up the north road to Rowan.");
    }
    if (!talk_or_fight(stream, context, "Galactic grunts with Dawn", true, true)){
        fail(stream, "Jubilife: Rowan's scene ended without the tag battle.");
    }
    if (in_pokemon_center(stream)){
        fail(stream, "Jubilife: lost the tag battle with Dawn.");
    }
    finish_dialogue(stream, context, BUTTON_B, "Rowan and Dawn after the battle");

    step(4);
    // Route 204's south half: a trainer just past the sign, then the cave in
    // the cliff a column west of the path.
    for (size_t leg = 0; leg < 6; leg++){
        const WalkResult up = walk_dir(stream, context, Direction::NORTH, 15000ms, "north on Route 204");
        if (up.stop == WalkStop::TRANSITION){
            break;
        }
        if (up.stop == WalkStop::DIALOG){
            talk_or_fight(stream, context, "Route 204 trainer");
            continue;
        }
        if (up.stop == WalkStop::BLOCKED){
            hold_tiles(stream, context, Direction::WEST, 1);
        }
        if (leg == 5){
            fail(stream, "Route 204: did not reach the Ravaged Path.");
        }
    }
    context.wait_for(2000ms);

    step(5);
    // On the traced grid (Data/PokemonBDSP_AutoStory_RavagedPath): from the
    // south door up the stairs to the tile west of one of the two rocks
    // across the lower corridor, smash it, then east and south to the
    // corridor's east end; the way out is a tile west of it, south.
    {
        std::vector<Pokemon::TrainerSight> no_trainers;
        auto in_the_way = [&](const std::string& who){ talk_or_fight(stream, context, who); };
        std::vector<Pokemon::TilePos> at{RavagedPath::ENTRANCE};
        const Pokemon::MapSet rocks_in = ravaged_path_maps();
        at = walk_route(stream, context, rocks_in, at,
            [&](const Pokemon::TilePos& pos){
                const auto& before = RavagedPath::before_rocks();
                return std::find(before.begin(), before.end(), pos) != before.end();
            },
            no_trainers, in_the_way, "To a rock across the corridor");
        route_turn(stream, context, Direction::EAST);
        if (!talk_with_nudges(stream, context, Direction::EAST, "the rock east of us", 2000ms)){
            fail(stream, "Ravaged Path: no Rock Smash offer facing the rock east of us.");
        }
        // "Would you like to use Rock Smash?": A for Yes, the animation, then
        // B for anything left (by hand, 2026-10-29).
        pbf_press_button(context, BUTTON_A, 80ms, 1500ms);
        pbf_press_button(context, BUTTON_A, 80ms, 4000ms);
        pbf_press_button(context, BUTTON_B, 80ms, 1500ms);
        context.wait_for_all_requests();
        // We may be on either row; the rock east of the other stays, but no
        // leg from our row crosses it.
        std::vector<Pokemon::TilePos> smashed;
        for (const Pokemon::TilePos& pos : RavagedPath::before_rocks()){
            smashed.push_back(pos.next(Direction::EAST));
        }
        const Pokemon::MapSet rocks_out = ravaged_path_maps(smashed);
        at = walk_route(stream, context, rocks_out, at,
            [&](const Pokemon::TilePos& pos){ return pos == RavagedPath::EAST_END; },
            no_trainers, in_the_way, "To the corridor's east end");
        hold_tiles(stream, context, Direction::WEST, 1);
        const WalkResult out = walk_dir(stream, context, Direction::SOUTH, 4000ms, "south out of the Ravaged Path");
        if (out.stop != WalkStop::TRANSITION && out.stop != WalkStop::LIMIT){
            fail(stream, "Ravaged Path: the walk south from a tile west of the corridor's end did not leave the cave.");
        }
        context.wait_for(2000ms);
    }

    step(6);
    // On the traced grid (Data/PokemonBDSP_AutoStory_Route204): from the
    // cave's door west and up the stairs, round the pocket the ledge closes,
    // east and north through the tall grass, and west below the flower beds,
    // where the twins see us; then north between the beds into Floaroma.
    {
        std::vector<Pokemon::TrainerSight> twins = Route204::north_trainer_sights();
        auto fight = [&](const std::string& who){
            const bool doubles = who.find("Twins") != std::string::npos || who.find("someone") != std::string::npos;
            talk_or_fight(stream, context, who, false, doubles);
        };
        std::vector<Pokemon::TilePos> at{Route204::CAVE_EXIT};
        at = walk_route(stream, context, route204_north_maps(), at, Route204::in_floaroma_path,
            twins, fight, "To the path into Floaroma");
        // Floaroma's arrival did not read as a black screen by hand ("limit
        // after 20000ms", and we stood in the town), so either will do.
        const WalkResult in = walk_dir(stream, context, Direction::NORTH, 20000ms, "north into Floaroma");
        if (in.stop == WalkStop::BLOCKED || in.stop == WalkStop::DIALOG){
            fail(stream, "Route 204: the walk north into Floaroma was stopped.");
        }
        context.wait_for(2000ms);
    }

    step(7);
    // We arrive in front of a house; the Center is a few tiles south-east.
    walk_dir(stream, context, Direction::SOUTH, 1500ms, "south off the house's path");
    hold_tiles(stream, context, Direction::EAST, 4);
    heal_at_pokemon_center(stream, context, "Floaroma Pokemon Center");
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

void checkpoint_0104(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    AutoStoryOptions options,
    AutoStoryStats& stats
){
    checkpoint_reattempt_loop(env, context, options, stats,
        [&](size_t /*attempt*/){
            oreburgh_gym(env.console, context);
        },
        true
    );
}

void checkpoint_0105(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    AutoStoryOptions options,
    AutoStoryStats& stats
){
    checkpoint_reattempt_loop(env, context, options, stats,
        [&](size_t /*attempt*/){
            oreburgh_to_floaroma(env.console, context);
        },
        true
    );
}
}


std::string AutoStory_Segment_01::name()       const { return "01: Jubilife to Floaroma"; }
std::string AutoStory_Segment_01::start_text() const { return "Start: Standing in front of the Jubilife City Pokemon Center PC."; }
std::string AutoStory_Segment_01::end_text()   const { return "End: In the Floaroma Town Pokemon Center, with the Coal Badge."; }

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
    AutoStory_Checkpoint_0104().run_checkpoint(env, context, options, stats);
    AutoStory_Checkpoint_0105().run_checkpoint(env, context, options, stats);
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



std::string AutoStory_Checkpoint_0104::name()       const { return "01.04 - Oreburgh Gym"; }
std::string AutoStory_Checkpoint_0104::start_text() const { return AutoStory_Checkpoint_0103().end_text(); }
std::string AutoStory_Checkpoint_0104::end_text()   const { return "Outside the Oreburgh Gym's door, with the Coal Badge."; }

void AutoStory_Checkpoint_0104::run_checkpoint(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    AutoStoryOptions options,
    AutoStoryStats& stats
) const{
    checkpoint_0104(env, context, options, stats);
}

std::string AutoStory_Checkpoint_0105::name()       const { return "01.05 - Oreburgh to Floaroma"; }
std::string AutoStory_Checkpoint_0105::start_text() const { return AutoStory_Checkpoint_0104().end_text(); }
std::string AutoStory_Checkpoint_0105::end_text()   const { return "In the Floaroma Town Pokemon Center, healed."; }

void AutoStory_Checkpoint_0105::run_checkpoint(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    AutoStoryOptions options,
    AutoStoryStats& stats
) const{
    checkpoint_0105(env, context, options, stats);
}

}
}
}
