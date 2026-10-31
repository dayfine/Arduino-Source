/*  BDSP AutoStory - Segment 01
 *  From the Jubilife Pokemon Center to Oreburgh City.
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "PokemonBDSP_AutoStory_Segment_01.h"
#include "../Data/PokemonBDSP_AutoStory_OreburghMine.h"
#include "../Data/PokemonBDSP_AutoStory_RavagedPath.h"
#include "../Data/PokemonBDSP_AutoStory_Route203.h"
#include "../Data/PokemonBDSP_AutoStory_Route204.h"
#include "../Data/PokemonBDSP_AutoStory_StoryMaps.h"
#include "../Detect/PokemonBDSP_AutoStory_OverworldDetector.h"
#include "../Utils/PokemonBDSP_AutoStory_PokemonCenter.h"
#include "../Utils/PokemonBDSP_AutoStory_Location.h"
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
uint64_t mean_brightness(const ImageViewRGB32& screen);
bool center_floor_on_screen(const ImageViewRGB32& screen, size_t* percent = nullptr);
bool in_dim_cave(VideoStream& stream, uint64_t below = 85);
bool talk_or_fight(VideoStream& stream, ProControllerContext& context, const std::string& label, bool battle_expected = false, bool doubles = false){
    ShortDialogDetector dialog;
    OverworldDetector overworld;
    BattleMenuDetector battle_menu(BattleType::TRAINER);
    //  Roark's battle menu read as STANDARD, not TRAINER, and nothing fought
    //  it (r210d): either counts here, as only a trainer's battle can start.
    BattleMenuDetector battle_menu_standard(BattleType::STANDARD);
    BlackScreenDetector black;
    auto quiet_since = std::chrono::steady_clock::now();
    for (size_t check = 0; check < 120; check++){
        context.wait_for_all_requests();
        const auto frame = stream.video().snapshot();
        if (!frame){
            fail(stream, label + ": No video frame.");
        }
        if (battle_menu.detect(*frame.frame) || battle_menu_standard.detect(*frame.frame) || black.detect(*frame.frame)){
            route_status(stream, "Fighting " + label + ".");
            fight_story_battle(stream, context, label, doubles);
            // Lines after the battle (Roark's badge, r211d) keep the menu
            // from opening: finish them before the Potion.
            if (dialogue_opens(stream, context, 3000ms)){
                finish_dialogue(stream, context, BUTTON_B, label + " after the battle");
            }
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

WalkResult walk_dir(
    VideoStream& stream, ProControllerContext& context, Direction dir, Milliseconds limit, const std::string& label,
    const std::function<bool(const ImageViewRGB32& screen)>& arrived = nullptr
){
    const double x = dir == Direction::EAST ? 1 : dir == Direction::WEST ? -1 : 0;
    const double y = dir == Direction::NORTH ? 1 : dir == Direction::SOUTH ? -1 : 0;
    return walk_until(stream, context, x, y, limit, label, arrived);
}

//  Out of a building whose door is straight south of us, a D-pad tile at a
//  time, until the location banner shows: it names the map we step into
//  (e.g. "Oreburgh City") for a few seconds, and never shows inside. Ends in
//  the doorway, facing south. Out of the Oreburgh Gym from in front of Roark
//  (by hand, 2026-10-31): 18 taps inside, 2 taps of black fade, the banner on
//  the 21st. The door's fade alone was missed between steps (r219g walked on
//  south to the rocks below the gym).
bool dpad_out_of_building(VideoStream& stream, ProControllerContext& context, size_t max_taps, const std::string& label){
    pbf_press_dpad(context, DPAD_DOWN, 160ms, 700ms);    // turn
    for (size_t tap = 0; tap < max_taps; tap++){
        pbf_press_dpad(context, DPAD_DOWN, 160ms, 900ms);
        context.wait_for_all_requests();
        const VideoSnapshot frame = stream.video().snapshot();
        if (frame && location_banner_showing(*frame.frame)){
            stream.log("[Walk] " + label + ": the location banner shows after " + std::to_string(tap + 1) + " taps; outside.");
            context.wait_for(1000ms);
            return true;
        }
        if (dialogue_opens(stream, context, 100ms)){
            finish_dialogue(stream, context, BUTTON_B, label + ": someone in the way");
        }
    }
    return false;
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
        // A tile at a time, looking after each, so we stop on the doorstep:
        // walk_until() looks only between 400 ms holds, and r205d held on
        // through the fade and stepped a tile further south.
        bool out = false;
        for (size_t tile = 0; tile < 10 && !out; tile++){
            // D-pad taps: whole tiles (the first only turns us from the nurse).
            pbf_press_dpad(context, DPAD_DOWN, 160ms, 0ms);
            context.wait_for(1500ms);
            if (dialogue_opens(stream, context, 100ms)){
                finish_dialogue(stream, context, BUTTON_B, label + ": someone in the Center");
            }
            const VideoSnapshot frame = stream.video().snapshot();
            size_t percent = 100;
            if (frame){
                center_floor_on_screen(*frame.frame, &percent);
            }
            out = percent < 30;
        }
        if (out){
            // "No floor" also reads true on the door's black fade, and the game
            // takes no input until it ends: the first D-pad taps after it were
            // lost and the gym's door ended a tile away (r215d, r216d; a shot
            // per press by hand showed the black frame). Wait for daylight.
            const auto start = current_time();
            while (current_time() - start < 6000ms){
                context.wait_for(100ms);
                const VideoSnapshot frame = stream.video().snapshot();
                if (frame && mean_brightness(*frame.frame) > 40){
                    break;
                }
            }
            context.wait_for(1000ms);
            return;
        }
    }
    fail(stream, label + ": could not find the Pokemon Center's door.");
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
    walk_to_wall(stream, context, 1, 0, 5000ms, "east to the fence below the Center");
    walk_to_wall(stream, context, 0, 1, 3000ms, "north to the Center's front");
    // The door wants us near its middle: a turn and a tap west (half a tile
    // too far, by hand) or a turn alone leave north blocked by the facade
    // beside it. The facade blocks north for two tiles either side of the
    // door, so look for it in nudges of about a third of a tile.
    route_turn(stream, context, Direction::WEST);
    context.wait_for_all_requests();
    auto inside_now = [&](){
        const VideoSnapshot frame = stream.video().snapshot();
        size_t percent = 0;
        return frame && (center_floor_on_screen(*frame.frame, &percent), percent >= 45);
    };
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
        // The door's fade often reads as the time limit, not a black screen
        // (r183a went in and on to the counter): look at the floor.
        // Stop as soon as the floor shows: walking on to the counter left the
        // door's column, and the walk back out drifted into the yard (r188b).
        // Oreburgh's sand reads up to 34 % warm outside (r189a), the floor
        // 56 to 64 % inside.
        const WalkResult in = walk_until(stream, context, 0, 1, 2500ms, "north into the Center's door",
            [](const ImageViewRGB32& screen){ size_t percent = 0; center_floor_on_screen(screen, &percent); return percent >= 45; });
        if (in.stop == WalkStop::ARRIVED || ((in.stop == WalkStop::TRANSITION || in.stop == WalkStop::LIMIT) && inside_now())){
            inside = true;
            break;
        }
        if (in.stop == WalkStop::DIALOG){
            talk_or_fight(stream, context, "Someone at the Center's door");
        }else{
            pbf_press_button(context, BUTTON_A, 80ms, 200ms);
            if (!dialogue_opens(stream, context, 2000ms)){
                continue;
            }
            finish_dialogue(stream, context, BUTTON_B, "Barry at the Center's door");
            context.wait_for(2000ms);    // he runs off
        }
        // Talked in the door's column: the door is free now.
        walk_until(stream, context, 0, 1, 2500ms, "north into the Center's door",
            [](const ImageViewRGB32& screen){ size_t percent = 0; center_floor_on_screen(screen, &percent); return percent >= 45; });
        inside = inside_now();
    }
    if (!inside || !inside_now()){
        fail(stream, "Oreburgh: no column east of the save went into the Pokemon Center.");
    }
}

//  Oreburgh: from just inside the Pokemon Center's door, up to the nurse
//  and heal; ends at the counter facing her. The door is straight south of
//  her (by hand, 2026-10-31: north from the door stopped at the counter).
void heal_at_oreburgh_counter(VideoStream& stream, ProControllerContext& context){
    if (walk_to_talk(stream, context, 0, 1, 6000ms, "to the nurse").stop != WalkStop::DIALOG){
        fail(stream, "Oreburgh Pokemon Center: the nurse did not answer.");
    }
    // A throughout: Yes to "rest your Pokemon?" (B would answer No).
    finish_dialogue(stream, context, BUTTON_A, "Nurse");
    // Healing shows no text box for a few seconds, which reads as the end;
    // her "Thank you for waiting" lines follow it.
    if (dialogue_opens(stream, context, 6000ms)){
        finish_dialogue(stream, context, BUTTON_B, "Nurse, after healing");
    }
}

//  Oreburgh: from just inside the Pokemon Center's door, out onto the tile
//  below it, facing south, on the door's column. Stops as soon as the floor
//  is gone: walking on runs down the column to the mine.
void leave_center_onto_door_row(VideoStream& stream, ProControllerContext& context){
    walk_until(stream, context, 0, -1, 4000ms, "out of the Center's door",
        [](const ImageViewRGB32& screen){ size_t percent = 0; center_floor_on_screen(screen, &percent); return percent < 30; });
    context.wait_for(1500ms);
}

void oreburgh_to_roark(VideoStream& stream, ProControllerContext& context){
    // What 01.03 does, stepped through on the console before writing it.
    StepLog step(stream, "01.03", "From the Oreburgh Pokemon Center to Roark in the mine", {
        "Into the Pokemon Center by its door (talking to Barry if he stands in it) and back out onto the door's column",
        "Straight south from the door into the mine",
        "Through the mine's entrance hall to the rocky corridor",
        "Walk the planned route down the corridor and round the coal block, and talk to Roark",
        "Walk back to the corridor's top and out of the mine",
        "Into the Pokemon Center and heal at the counter",
    });

    step(1);
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

    step(2);
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
        fail(stream, "Oreburgh: the walk south from the Center's door did not reach the mine.");
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
    // His rock and the bottom wall end these legs (see walk_to_wall()). The
    // two taps west from there land within a third of a tile or so of his
    // column (s03e2 missed him by that much), so look for him with nudges
    // either side. Any tap here can start a wild battle, which leaves us
    // off his column (r180a, even on his row west of his rock): then line up
    // again from the east end of the bottom row, three taps west and north to the wall on his row (Data/
    // OreburghMine: columns 23 to 25 have that wall above).
    bool talked = false;
    for (size_t attempt = 0; attempt < 3 && !talked; attempt++){
        if (attempt > 0){
            // The battle may leave us on his row or the bottom one.
            walk_to_wall(stream, context, 0, -1, 2000ms, "south to the bottom row");
            walk_to_wall(stream, context, 1, 0, 6000ms, "east along the bottom row");
            route_turn(stream, context, Direction::WEST);
            for (size_t tap = 0; tap < 3; tap++){
                pbf_move_left_joystick(context, {-1, 0}, 120ms, 300ms);
            }
            walk_to_wall(stream, context, 0, 1, 2000ms, "north to the wall on Roark's row");
        }
        // A wild battle on the way eats into the walk's time (r100): walk again.
        for (size_t walk = 0; walk < 3; walk++){
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
        if (run_from_late_encounter(stream, context, "below Roark")){
            continue;
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
        talked = talk_with_nudges(stream, context, Direction::NORTH, "Roark", 8000ms,
            [&](const ImageViewRGB32& before, const ImageViewRGB32& after){
                const ImageFloatBox above_box{0.0, 0.0, 1.0, 0.6};
                const double change = walk_screen_change(
                    extract_box_reference(before, above_box), extract_box_reference(after, above_box)
                );
                stream.log("[01.03] View change when the talk opened: " + std::to_string(change) + " (Roark zooms in).");
                return change > 35;     //  by hand: 50 for his zoom
            });
    }
    if (!talked){
        fail(stream, "Oreburgh Mine: no talk opened below Roark (has Barry told us about the mine?).");
    }
    // His scene (by hand, 2026-10-31): the camera zooms in, about eight
    // lines that each wait for a press ("OK, stand back and watch this.",
    // Rock Smash, "...That'd be me!"), then he smashes his rock and leaves.
    // finish_dialogue() ended early on it, and stepping onto his tile could
    // not tell him from the time the stop takes to read (r184, r186 "passed"
    // with him still there, and Barry kept the gym shut). Press B through it
    // on a clock, then ask his tile: once he is gone, A there opens nothing.
    // B and A in the overworld facing a wall do nothing.
    bool gone = false;
    for (size_t round = 0; round < 3 && !gone; round++){
        for (size_t press = 0; press < 14; press++){
            pbf_press_button(context, BUTTON_B, 80ms, 2420ms);
        }
        pbf_press_button(context, BUTTON_A, 80ms, 200ms);
        gone = !dialogue_opens(stream, context, 2500ms);
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

    step(6);
    // End at the nurse, healed: the counter is a fixed start for 01.04,
    // where the mine's mouth is not (2026-10-31, at the user's request). Out
    // of the mine we stand below its sign; a tile east is the Center door's
    // column, and north on it goes in, or stops on the front rows.
    hold_tiles(stream, context, Direction::EAST, 1);
    walk_until(stream, context, 0, 1, 12000ms, "north into the Center's door",
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


//  A rough check that we stand in a Pokemon Center: its floor fills the lower
//  half of the screen with warm yellow, where the gym is grey and routes are
//  green. After a battle it tells a loss (a whiteout ends at the counter)
//  from a win.
bool center_floor_on_screen(const ImageViewRGB32& screen, size_t* percent){
    const ImageRGB32 small = screen.scale_to(192, 108);
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
    if (percent){
        *percent = warm * 100 / total;
    }
    return warm * 10 > total * 3;
}
bool in_pokemon_center(VideoStream& stream){
    const VideoSnapshot frame = stream.video().snapshot();
    if (!frame){
        return false;
    }
    size_t percent = 0;
    const bool inside = center_floor_on_screen(*frame.frame, &percent);
    stream.log("[Center check] warm floor " + std::to_string(percent) + " %: " +
        (inside ? "in a Pokemon Center." : "not in a Pokemon Center."));
    return inside;
}

//  The Oreburgh Mine's hall and corridors are dim: mean brightness 45 to 64
//  on the console, against 99 to 116 for the city and its yard (2026-10-31).
bool in_dim_cave(VideoStream& stream, uint64_t below){
    const VideoSnapshot frame = stream.video().snapshot();
    if (!frame){
        return false;
    }
    const ImageRGB32 small = frame.frame->scale_to(192, 108);
    uint64_t sum = 0;
    for (size_t y = 0; y < 108; y++){
        for (size_t x = 0; x < 192; x++){
            const uint32_t pixel = small.pixel(x, y);
            sum += ((pixel >> 16) & 0xff) + ((pixel >> 8) & 0xff) + (pixel & 0xff);
        }
    }
    const uint64_t mean = sum / (3 * 192 * 108);
    stream.log("[Cave check] mean brightness " + std::to_string(mean) + ".");
    return mean < below;
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
uint64_t mean_brightness(const ImageViewRGB32& screen);

//  `taps` D-pad taps toward `dpad`, settling between them. On the D-pad the
//  player moves a whole tile per tap (measured above), so counted legs do
//  not drift the way stick holds do. The first tap toward a new way only
//  turns the player, so turn first.
void dpad_tiles(VideoStream& stream, ProControllerContext& context, DpadPosition dpad, size_t taps, const std::string& label){
    stream.log("[Walk] " + label + ": " + std::to_string(taps) + " D-pad taps.");
    pbf_press_dpad(context, dpad, 160ms, 700ms);    // turn
    // Count only taps that moved the view: one after the turn was lost and
    // the leg ended a tile short (r215d). A tile's scroll changes the view by
    // about 100; standing still, under 15. A tap that does not move is someone
    // in the way: people walk about these streets (r220g: blocked on tap 9
    // for 12 s while a passer-by stood in our row). Wait for them to move on,
    // up to 30 s, before giving up.
    size_t moved = 0;
    const auto deadline = current_time() + std::chrono::milliseconds(taps * 1000 + 30000);
    while (moved < taps && current_time() < deadline){
        context.wait_for_all_requests();
        const VideoSnapshot before = stream.video().snapshot();
        pbf_press_dpad(context, dpad, 160ms, 700ms);
        context.wait_for_all_requests();
        const VideoSnapshot after = stream.video().snapshot();
        const double change = before && after ? walk_screen_change(*before.frame, *after.frame) : 100;
        if (change > 30){
            moved++;
            continue;
        }
        stream.log("[Walk] " + label + ": tap " + std::to_string(moved + 1) + " did not move (view change " + std::to_string(change) + "); someone in the way? Waiting.");
        if (dialogue_opens(stream, context, 300ms)){
            talk_or_fight(stream, context, label + ": someone in the way");
        }
        context.wait_for(1500ms);
    }
    if (moved < taps){
        fail(stream, label + ": only " + std::to_string(moved) + " of " + std::to_string(taps) + " D-pad taps moved us.");
    }
}

//  Through a door straight north of us: tap up to face it, hold up into it,
//  and wait for its dark fade (by hand: brightness 7, about 1 s after).
bool dpad_through_door_north(VideoStream& stream, ProControllerContext& context, const std::string& label){
    pbf_press_dpad(context, DPAD_UP, 160ms, 300ms);
    pbf_press_dpad(context, DPAD_UP, 300ms, 0ms);
    context.wait_for_all_requests();
    const auto start = current_time();
    while (current_time() - start < 5000ms){
        context.wait_for(100ms);
        const VideoSnapshot frame = stream.video().snapshot();
        if (frame && mean_brightness(*frame.frame) < 25){
            stream.log("[Route] " + label + ": the screen went dark; through the door.");
            context.wait_for(2000ms);
            return true;
        }
    }
    stream.log("[Route] " + label + ": no dark screen after the D-pad step north.");
    return false;
}

//  Mean of the three channels over a small copy of the frame, 0 to 255.
uint64_t mean_brightness(const ImageViewRGB32& screen){
    const ImageRGB32 small = screen.scale_to(192, 108);
    uint64_t sum = 0;
    for (size_t y = 0; y < 108; y++){
        for (size_t x = 0; x < 192; x++){
            const uint32_t pixel = small.pixel(x, y);
            sum += ((pixel >> 16) & 0xff) + ((pixel >> 8) & 0xff) + (pixel & 0xff);
        }
    }
    return sum / (3 * 192 * 108);
}

//  North through a door from just below it: one tile's step, then watch
//  for the door's black fade. Entering the Oreburgh Gym (by hand,
//  2026-10-31): the door opens and the camera zooms in, and about 2 s after
//  the step the screen is black (brightness 7) for 1 to 1.5 s. The step's
//  own walk stops watching when the stick is let go, so it missed that
//  black; a scene comparison could not tell a tile's scroll (106 to 119)
//  from going in (121, r205d, r207d). A miss steps back onto the row.
bool through_door_north(VideoStream& stream, ProControllerContext& context, const std::string& label){
    auto went_dark = [&](Milliseconds timeout){
        const auto start = current_time();
        while (current_time() - start < timeout){
            context.wait_for(100ms);
            const VideoSnapshot frame = stream.video().snapshot();
            if (frame && mean_brightness(*frame.frame) < 25){
                return true;
            }
        }
        return false;
    };
    context.wait_for_all_requests();
    const VideoSnapshot before = stream.video().snapshot();
    const WalkResult up = walk_for(stream, context, 0, 1, 240ms, label);
    // Look for the fade ourselves: the library's black screen check missed it
    // under the gym's door (r209d), while its frames read 7 against about
    // 100 outside.
    if (up.stop == WalkStop::TRANSITION || went_dark(5000ms)){
        stream.log("[Route] " + label + ": the screen went dark; through the door.");
        context.wait_for(2000ms);
        return true;
    }
    const VideoSnapshot after = stream.video().snapshot();
    const double moved = before && after ? walk_screen_change(*before.frame, *after.frame) : 0;
    // The door opened and the camera zoomed in, but one step did not take us
    // through (r213d: view change 160, no fade): step on north.
    if (moved > WALK_SCENE_CHANGE){
        stream.log("[Route] " + label + ": the door opened (view change " + std::to_string(moved) + "); on north.");
        walk_for(stream, context, 0, 1, 240ms, label + ": on through");
        if (went_dark(5000ms)){
            stream.log("[Route] " + label + ": the screen went dark; through the door.");
            context.wait_for(2000ms);
            return true;
        }
    }
    // Back down only if the step moved us: against the building's front it
    // does not, and stepping south anyway took us a row down each try
    // (r208d drifted south along the gym's front).
    stream.log("[Route] " + label + ": no dark screen; not the door (view change " + std::to_string(moved) + ").");
    if (moved > WALK_BLOCKED_CHANGE * 2){
        walk_for(stream, context, 0, -1, 240ms, label + ": back down");
    }
    return false;
}

void oreburgh_street_to_gym(VideoStream& stream, ProControllerContext& context){
    // From the Center's doorstep, straight west along its row to below the
    // gym's door, then north through it, all on the D-pad (whole tiles; see
    // GYM_DPAD_TAPS_WEST_OF_CENTER).
    dpad_tiles(stream, context, DPAD_LEFT, GYM_DPAD_TAPS_WEST_OF_CENTER, "west to the gym's door");
    if (dpad_through_door_north(stream, context, "Oreburgh Gym door")){
        return;
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
        const WalkResult up = walk_until_no_wild(stream, context, 0, 1, 8000ms, "north up the gym");
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
            if (talk_or_fight(stream, context, "Roark", true)){
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
        "Out of the Pokemon Center from the counter",
        "West along the row into the gym",
        "Up the gym, fighting its trainers, and beat Roark (after a loss: from the Center, again)",
    });
    step(1);
    // 01.03 leaves us at the Center's counter, facing the nurse; the door
    // is straight south.
    walk_out_of_center(stream, context, "Oreburgh");

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

}

//  West until a transition, by the user's rule for a wall in the way: walk
//  north to the top wall, which pins the row, then try the rows south of it
//  one at a time. Trainers who stop us are fought. Returns at the first
//  transition (a building, a cave, a gate), or when `arrived` matches.
void west_until_transition(
    VideoStream& stream, ProControllerContext& context, const std::string& label, size_t max_legs,
    const std::function<bool(const ImageViewRGB32& screen)>& arrived = nullptr
){
    for (size_t leg = 0; leg < max_legs; leg++){
        const WalkResult west = walk_dir(stream, context, Direction::WEST, 20000ms, label, arrived);
        if (west.stop == WalkStop::ARRIVED){
            return;
        }
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
            const WalkResult probe = walk_dir(stream, context, Direction::WEST, 1500ms, label + ": west on the next row", arrived);
            if (probe.stop == WalkStop::ARRIVED){
                return;
            }
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

//  Oreburgh: from in front of Roark at the top of the gym, back down and out
//  of its door a tile at a time, east along the row to the Pokemon Center
//  and heal; ends at the counter.
void oreburgh_gym_to_center(VideoStream& stream, ProControllerContext& context){
    if (!dpad_out_of_building(stream, context, 40, "out of the gym")){
        fail(stream, "Oreburgh Gym: did not leave by its door.");
    }

    // Heal at the Center, so 01.05 starts at its counter: the gym's doorstep
    // row runs east to the Center's door, the way 01.04 came.
    // The way 01.04 came, reversed, on the D-pad. Untested on the console:
    // the gym's doorstep is assumed to be the tile below its door.
    dpad_tiles(stream, context, DPAD_RIGHT, GYM_DPAD_TAPS_WEST_OF_CENTER, "east to the Center's door");
    if (!dpad_through_door_north(stream, context, "Oreburgh Pokemon Center door")){
        fail(stream, "Oreburgh: the Pokemon Center's door did not take us from the gym's row.");
    }
    heal_at_oreburgh_counter(stream, context);
}

void oreburgh_to_route203(VideoStream& stream, ProControllerContext& context){
    StepLog step(stream, "01.05", "From the Oreburgh Gym out of the Oreburgh Gate onto Route 203", {
        "Out of the gym onto its doorstep",
        "Up to the platform by Oreburgh Gate and into it (Barry bumps into us there)",
        "West through the Gate until Route 203's banner shows",
    });
    step(1);
    // 01.04 ends in front of Roark, just after his battle: its save at
    // 01.05's start is right after the win (2026-10-31, at the user's
    // request), so a failure later never has to beat him again.
    // No trip back to the Oreburgh Center (2026-10-31, the user): talk_or_fight()
    // gives a Potion after Roark when HP is low, and Jubilife's Center is on
    // the way. Out of the gym onto its doorstep, which is on the Center door's
    // row: west along it reaches the stairs' cliff.
    if (!dpad_out_of_building(stream, context, 40, "out of the gym")){
        fail(stream, "Oreburgh Gym: did not leave by its door.");
    }

    step(2);
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

    step(3);
    // The cave's fade out of the Gate does not always read as a transition
    // (r89), so the walk west ends on Route 203's location banner, which
    // shows for a few seconds as we step out. 01.06 starts there, from a save.
    west_until_transition(stream, context, "West out of Oreburgh Gate", 8, location_banner_showing);
}

void route203_to_jubilife(VideoStream& stream, ProControllerContext& context){
    StepLog step(stream, "01.06", "From the Oreburgh Gate's west exit into Jubilife City", {
        "Route 203 on its grid to row 17, below the blue-haired trainer",
        "West along row 17 into Jubilife, counting tiles",
    });

    step(1);
    // On the grid (Data/PokemonBDSP_AutoStory_Route203, checked by hand):
    // south from the Gate, west under the beaten girl to the stairs, north
    // to the ledge, west to the blue-haired trainer, and a row down onto row
    // 17, which runs straight to Jubilife. Trainers the map does not list
    // are fought where they stop us.
    std::vector<Pokemon::TrainerSight> trainers;
    auto fight = [&](const std::string& who){
        talk_or_fight(stream, context, "Route 203: " + who);
    };
    const std::vector<Pokemon::TilePos> at = walk_route(stream, context, route203_maps(), Route203::gate_exit(),
        on_route203_west_line, trainers, fight, "Route 203 to row 17");
    if (at.size() != 1){
        fail(stream, "Route 203: the walk to row 17 did not end on a known tile.");
    }

    step(2);
    // Row 17 has no wall before Jubilife, so count the tiles: from column
    // `x` it is x + 1 taps to Jubilife's first tile (by hand from (31, 17):
    // 32 taps, Jubilife's banner on the last one). 01.07 starts there.
    dpad_tiles(stream, context, DPAD_LEFT, (size_t)at[0].x + 1, "west along row 17 into Jubilife");
    context.wait_for_all_requests();
    if (!location_banner_showing(*stream.video().snapshot().frame)){
        fail(stream, "Route 203: no Jubilife banner after counting the tiles west.");
    }
}

void jubilife_to_floaroma(VideoStream& stream, ProControllerContext& context){
    StepLog step(stream, "01.07", "From Jubilife City's east edge to Floaroma Town", {
        "West along Jubilife's long road to the foot of the north road",
        "Up Jubilife's north road (Rowan, Dawn and two Galactic grunts: a tag battle)",
        "North on Route 204 into the Ravaged Path",
        "Through the Ravaged Path, smashing its rocks",
        "Route 204 north on its grid, past the twins, into Floaroma",
        "Heal at the Floaroma Pokemon Center",
    });

    step(1);
    // Route 203's row 17 carries on as Jubilife's long road. From the first
    // tile, the street north to Route 204 opens 7 tiles west, by the corner
    // of the building with the blue door (by hand, 2026-10-31: 7 west and
    // about 10 north to Rowan's scene; 18 west went past it to the road
    // south to Route 202, r230b).
    dpad_tiles(stream, context, DPAD_LEFT, 7, "west along Jubilife's long road");

    step(2);
    // Rowan's scene starts on the way up that street, about 10 tiles north.
    // A wall sooner means we are off its column: step back down exactly as
    // far as we went (walking south until blocked went down the road to
    // Route 202, r230b) and try the next column.
    const int steps[] = {0, 1, -2, 3, -4};
    bool scene = false;
    for (int s_step : steps){
        if (s_step != 0){
            dpad_tiles(stream, context, s_step > 0 ? DPAD_RIGHT : DPAD_LEFT, (size_t)std::abs(s_step), "to the next column");
        }
        // The green-haired man walks about this corner and may stand in our
        // column for a moment: wait for him twice before trying another.
        size_t went = 0;
        for (size_t wait = 0; wait < 3; wait++){
            if (wait > 0){
                pbf_wait(context, 2500ms);
            }
            const WalkResult up = walk_until_dpad(stream, context, 0, 1, 6000ms, "north up the street to Route 204");
            scene = up.stop == WalkStop::DIALOG;
            const Milliseconds moving = up.walked - ROUTE_START_DELAY - (up.stop == WalkStop::BLOCKED ? 800ms : 0ms);
            went += moving.count() <= 0 ? 0 : (size_t)(moving / ROUTE_MS_PER_TILE_UP_DOWN);
            if (scene || up.stop != WalkStop::BLOCKED){
                break;
            }
        }
        if (scene){
            break;
        }
        if (went > 0){
            dpad_tiles(stream, context, DPAD_DOWN, went, "back down to the long road");
        }
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

    step(3);
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

    step(4);
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

    step(5);
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

    step(6);
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
            oreburgh_to_route203(env.console, context);
        },
        true
    );
}

void checkpoint_0106(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    AutoStoryOptions options,
    AutoStoryStats& stats
){
    checkpoint_reattempt_loop(env, context, options, stats,
        [&](size_t /*attempt*/){
            route203_to_jubilife(env.console, context);
        },
        true
    );
}

void checkpoint_0107(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    AutoStoryOptions options,
    AutoStoryStats& stats
){
    checkpoint_reattempt_loop(env, context, options, stats,
        [&](size_t /*attempt*/){
            jubilife_to_floaroma(env.console, context);
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
    AutoStory_Checkpoint_0106().run_checkpoint(env, context, options, stats);
    AutoStory_Checkpoint_0107().run_checkpoint(env, context, options, stats);
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
std::string AutoStory_Checkpoint_0103::end_text()   const { return "At the Oreburgh Pokemon Center counter, healed, after talking to Roark."; }

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
std::string AutoStory_Checkpoint_0104::end_text()   const { return "In the Oreburgh Gym, in front of Roark, just after beating him (Coal Badge)."; }

void AutoStory_Checkpoint_0104::run_checkpoint(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    AutoStoryOptions options,
    AutoStoryStats& stats
) const{
    checkpoint_0104(env, context, options, stats);
}

std::string AutoStory_Checkpoint_0105::name()       const { return "01.05 - Oreburgh to Route 203"; }
std::string AutoStory_Checkpoint_0105::start_text() const { return AutoStory_Checkpoint_0104().end_text(); }
std::string AutoStory_Checkpoint_0105::end_text()   const { return "On Route 203, just out of the Oreburgh Gate's west exit."; }

void AutoStory_Checkpoint_0105::run_checkpoint(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    AutoStoryOptions options,
    AutoStoryStats& stats
) const{
    checkpoint_0105(env, context, options, stats);
}

std::string AutoStory_Checkpoint_0106::name()       const { return "01.06 - Route 203 to Jubilife"; }
std::string AutoStory_Checkpoint_0106::start_text() const { return AutoStory_Checkpoint_0105().end_text(); }
std::string AutoStory_Checkpoint_0106::end_text()   const { return "In Jubilife City, on the first tile of its long road in from Route 203."; }

void AutoStory_Checkpoint_0106::run_checkpoint(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    AutoStoryOptions options,
    AutoStoryStats& stats
) const{
    checkpoint_0106(env, context, options, stats);
}

std::string AutoStory_Checkpoint_0107::name()       const { return "01.06 - Route 203 to Floaroma"; }
std::string AutoStory_Checkpoint_0107::start_text() const { return AutoStory_Checkpoint_0106().end_text(); }
std::string AutoStory_Checkpoint_0107::end_text()   const { return "In the Floaroma Town Pokemon Center, healed."; }

void AutoStory_Checkpoint_0107::run_checkpoint(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    AutoStoryOptions options,
    AutoStoryStats& stats
) const{
    checkpoint_0107(env, context, options, stats);
}

}
}
}
