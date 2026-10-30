/*  BDSP AutoStory - Segment 01.02: Route 203 to Oreburgh City
 *  East along Route 203 past its trainers, the Hiker's Rock Smash, through
 *  the Oreburgh Gate into Oreburgh City and the man's tour there.
 *
 *  Start: on Route 203 by its sign, after beating Barry.
 *  End:   in front of the Oreburgh Pokemon Center, after the tour and Barry.
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

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

//  One tile south, for a player facing another way.
void step_south(VideoStream& stream, ProControllerContext& context){
    route_turn(stream, context, Direction::SOUTH);
    pbf_move_left_joystick(context, {0, -1}, route_open_hold(Direction::SOUTH, 1), 200ms);
    context.wait_for_all_requests();
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


}
}
}
