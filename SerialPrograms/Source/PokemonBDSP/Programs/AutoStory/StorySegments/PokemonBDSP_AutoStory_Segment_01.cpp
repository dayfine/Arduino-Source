/*  BDSP AutoStory - Segment 01
 *  From the Jubilife Pokemon Center to Oreburgh City.
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "PokemonBDSP_AutoStory_Segment_01.h"
#include "../Data/PokemonBDSP_AutoStory_JubilifeCity.h"
#include "../Data/PokemonBDSP_AutoStory_StoryMaps.h"
#include "../Detect/PokemonBDSP_AutoStory_OverworldDetector.h"
#include "../Utils/PokemonBDSP_AutoStory_RouteWalk.h"
#include "../Utils/PokemonBDSP_AutoStory_Segment_00_tools.h"
#include "../Utils/PokemonBDSP_AutoStory_Walk.h"

#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/VisualDetectors/BlackScreenDetector.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "PokemonBDSP/Inference/PokemonBDSP_DialogDetector.h"
#include "PokemonBDSP/Inference/Battles/PokemonBDSP_BattleMenuDetector.h"

using namespace std::chrono_literals;

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using Pokemon::GridDirection;
using Pokemon::TilePos;


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
//  overworld. Returns whether there was a battle.
bool talk_or_fight(VideoStream& stream, ProControllerContext& context, const std::string& label){
    ShortDialogDetector dialog;
    OverworldDetector overworld;
    BattleMenuDetector battle_menu(BattleType::TRAINER);
    BlackScreenDetector black;
    size_t quiet = 0;
    for (size_t check = 0; check < 60; check++){
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
            if (++quiet >= 2){
                route_status(stream, label + " only talked.");
                return false;
            }
            pbf_wait(context, 300ms);
            continue;
        }
        quiet = 0;
        if (talking){
            pbf_press_button(context, BUTTON_B, 80ms, 1100ms);
        }else{
            pbf_wait(context, 300ms);
        }
    }
    fail(stream, label + ": neither a battle nor the overworld after the dialogue.");
}

void jubilife_to_oreburgh(SingleSwitchProgramEnvironment& env, VideoStream& stream, ProControllerContext& context){
    (void)env;
    // What 01.01 does, stepped through on the console before writing it.
    static const char* const STEPS[] = {
        "Heal at the nurse, then walk out of the Pokemon Center",
        "Walk to Jubilife's east exit and battle Barry at the Route 203 entrance",
        "Walk east along Route 203, fighting the trainers who see us",
        "Enter the Oreburgh Gate from the corner by the cliff",
        "Listen to the Hiker, who gives HM Rock Smash",
        "Walk east through the Gate past Curtis and Diana, out to Oreburgh City",
        "Listen to the man who meets us at the city's entrance",
    };
    constexpr size_t STEP_COUNT = sizeof(STEPS) / sizeof(STEPS[0]);
    {
        std::string text = "[01.01] From Jubilife to Oreburgh, the steps are:";
        for (size_t i = 0; i < STEP_COUNT; i++){
            text += "\n    " + std::to_string(i + 1) + ". " + STEPS[i];
        }
        stream.log(text, COLOR_BLUE);
    }
    auto step = [&](size_t number){
        std::string text = "[01.01 step " + std::to_string(number) + "/" + std::to_string(STEP_COUNT) + "] " + STEPS[number - 1];
        text += number < STEP_COUNT ? std::string(" -- next: ") + STEPS[number] : std::string(" -- last step");
        stream.log(text, COLOR_BLUE);
        stream.overlay().add_log("01.01 step " + std::to_string(number) + "/" + std::to_string(STEP_COUNT) + ": " + STEPS[number - 1], COLOR_WHITE);
        if (number < STEP_COUNT){
            stream.overlay().add_log("Next: " + std::string(STEPS[number]), COLOR_GRAY);
        }
    };

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
    // Her first line, then Yes to healing; B for the rest.
    finish_dialogue(stream, context, 2, "Nurse");
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
    const Pokemon::MapSet& maps = jubilife_maps();
    auto world = [&](const TilePos& pos){ return maps.to_world(MAP_JUBILIFE_CITY, pos); };
    std::vector<Pokemon::TrainerSight> no_trainers;
    auto in_the_way = [&](const std::string&){ finish_dialogue(stream, context, 0, "Someone in the way"); };
    const TilePos street = world(JubilifeCity::EAST_EXIT_APPROACH);
    walk_route(stream, context, maps, {world(JubilifeCity::CENTER_EXIT_STOP)},
        [&](const TilePos& pos){ return pos.y == street.y && pos.x >= street.x; },
        no_trainers, in_the_way, "To the street to Route 203");
    // East along the street. The opening to the route is a few rows high,
    // just south of this row; Barry stops us a little way in. A wall means
    // we are a row off it: a row further south.
    bool barry = false;
    for (size_t row = 0; row < 4 && !barry; row++){
        if (row > 0){
            step_south(stream, context);
        }
        WalkResult walk{WalkStop::LIMIT, 0ms, 0};
        for (size_t stretch = 0; stretch < 3 && walk.stop == WalkStop::LIMIT; stretch++){
            walk = walk_until(stream, context, 1, 0, 9000ms, "east to Route 203");
        }
        barry = walk.stop == WalkStop::DIALOG;
    }
    if (!barry){
        fail(stream, "Route 203: Barry did not stop us at the entrance.");
    }
    if (!talk_or_fight(stream, context, "Barry")){
        fail(stream, "Route 203: Barry talked but did not battle.");
    }

    step(3);
    // East to the cliff at the route's end. A woman standing on the path
    // only talks; a step south goes around her. Youngsters Dallas and
    // Sebastian see us on the way.
    size_t fights = 0;
    size_t walls = 0;
    for (size_t leg = 0; leg < 20; leg++){
        const WalkResult walk = walk_until(stream, context, 1, 0, 10000ms, "east along Route 203");
        if (walk.stop == WalkStop::TRANSITION){
            fail(stream, "Route 203: an unexpected transition walking east.");
        }
        if (walk.stop == WalkStop::LIMIT){
            continue;
        }
        if (walk.stop == WalkStop::DIALOG || dialogue_opens(stream, context, 1500ms)){
            fights += talk_or_fight(stream, context, "Route 203 trainer " + std::to_string(fights + 1)) ? 1 : 0;
            walls = 0;
            continue;
        }
        // A wall: the cliff once both trainers are behind us; before that,
        // someone on the path, or the cliff if a trainer missed us.
        if (fights >= 2 || ++walls >= 3){
            break;
        }
        step_south(stream, context);
    }
    route_status(stream, "Route 203: " + std::to_string(fights) + " trainer battle(s) on the way.");

    step(4);
    // North to the corner by the route's sign; the Gate's opening is in the
    // cliff a couple of tiles south of it.
    walk_until(stream, context, 0, 1, 6000ms, "north to the corner by the cliff");
    bool inside = false;
    for (size_t row = 0; row < 5 && !inside; row++){
        inside = walk_until(stream, context, 1, 0, 2500ms, "into the Oreburgh Gate").stop == WalkStop::TRANSITION;
        if (!inside){
            step_south(stream, context);
        }
    }
    if (!inside){
        fail(stream, "Route 203: did not find the Oreburgh Gate's opening in the cliff.");
    }
    context.wait_for(1500ms);

    step(5);
    // The Hiker steps up to us as we walk east.
    const WalkResult to_hiker = walk_until(stream, context, 1, 0, 4000ms, "to the Hiker");
    if (to_hiker.stop != WalkStop::DIALOG && !dialogue_opens(stream, context, 2000ms)){
        fail(stream, "Oreburgh Gate: the Hiker did not stop us.");
    }
    finish_dialogue(stream, context, 0, "Hiker");

    step(6);
    // East: Camper Curtis and Picnicker Diana see us; past them the exit's
    // white fade, and the man in Oreburgh stops us right outside.
    fights = 0;
    bool city = false;
    for (size_t leg = 0; leg < 12 && !city; leg++){
        const WalkResult walk = walk_until(stream, context, 1, 0, 10000ms, "east through the Oreburgh Gate");
        if (walk.stop == WalkStop::LIMIT || walk.stop == WalkStop::TRANSITION){
            continue;
        }
        if (walk.stop != WalkStop::DIALOG && !dialogue_opens(stream, context, 2000ms)){
            fail(stream, "Oreburgh Gate: a wall stopped us walking east.");
        }
        if (fights >= 2){
            city = true;
            break;
        }
        fights += talk_or_fight(stream, context, "Oreburgh Gate trainer " + std::to_string(fights + 1)) ? 1 : 0;
    }
    if (!city){
        fail(stream, "Oreburgh Gate: did not reach Oreburgh City.");
    }

    step(7);
    finish_dialogue(stream, context, 0, "Man at Oreburgh's entrance");
    context.wait_for_all_requests();
}

void checkpoint_0101(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    AutoStoryOptions options,
    AutoStoryStats& stats
){
    checkpoint_reattempt_loop(env, context, options, stats,
        [&](size_t /*attempt*/){
            jubilife_to_oreburgh(env, env.console, context);
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
    env.console.log("End Segment " + name(), COLOR_GREEN);
}

std::string AutoStory_Checkpoint_0101::name()       const { return "01.01 - Jubilife to Oreburgh"; }
std::string AutoStory_Checkpoint_0101::start_text() const { return "Standing in front of the Jubilife City Pokemon Center PC."; }
std::string AutoStory_Checkpoint_0101::end_text()   const { return "In Oreburgh City, after the man at the entrance."; }

void AutoStory_Checkpoint_0101::run_checkpoint(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    AutoStoryOptions options,
    AutoStoryStats& stats
) const{
    checkpoint_0101(env, context, options, stats);
}


}
}
}
