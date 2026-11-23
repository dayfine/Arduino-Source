/*  BDSP AutoStory - Events: Oreburgh Gym (01.04)
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  01.04's walk into the Oreburgh Gym and up it to Roark, again from the
 *  Pokemon Center after each loss, as it ran on the console (2026-10-29 to
 *  10-31). A loss keeps the experience, which a reload of the row's save
 *  would not.
 *
 */

#include <format>
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "PokemonBDSP/Inference/Battles/PokemonBDSP_BattleMenuDetector.h"
#include "PokemonBDSP/Inference/PokemonBDSP_DialogDetector.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStoryMaps.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStoryIndoors.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStoryRouteWalk.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStoryWalk.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStoryWalkResult.h"
#include "PokemonBDSP_AutoStoryBattle.h"
#include "PokemonBDSP_AutoStoryBattleMoves.h"
#include "PokemonBDSP_AutoStoryCenter.h"
#include "PokemonBDSP_AutoStoryLitDoorway.h"
#include "PokemonBDSP_AutoStoryMenuActions.h"
#include "PokemonBDSP_AutoStoryTalks.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStoryWalks.h"
#include "PokemonBDSP_AutoStoryWildBattle.h"
#include "PokemonBDSP_AutoStoryEvents.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace std::chrono_literals;


namespace{

//  01.04 starts below Oreburgh's Center door (47, 21): 01.03's heal and
//  the walk out after a loss both leave us there. The Gym's door is on the
//  same street, so the D-pad taps west are the columns between the two
//  doors on the map (21). A hand count of 20, from before the maps, ended
//  a tile east of the Gym's door (lab-B, 2026-11-08 14:51).
size_t gym_taps_west_of_center(){
    const Pokemon::TilePos center = map_point("OreburghCity", "pokemon_center_door");
    const Pokemon::TilePos gym = map_point("OreburghCity", "gym_door");
    return static_cast<size_t>(center.x - gym.x);
}

void oreburgh_street_to_gym(VideoStream& stream, ProControllerContext& context){
    // From the Center's doorstep, straight west along its row to below the
    // gym's door, then north through it, all on the D-pad (whole tiles; see
    // gym_taps_west_of_center()). 01.04 always starts on the doorstep: its
    // own save, or the walk out of the Center after a loss. A brightness
    // check for "already in the gym" (gym 75 against street 104 by day, r74)
    // read the street at night (88, game clock 00:25) as the gym and talked
    // to the nurse as Roark (lab-B, 2026-11-08 14:25).
    context.wait_for_all_requests();
    dpad_tiles(stream, context, DPAD_LEFT, gym_taps_west_of_center(), "west to the gym's door");
    if (dpad_through_door_north(stream, context, "Oreburgh Gym door")){
        return;
    }
    //  The gym's wall stopped us: try the column west, then the one east.
    dpad_tiles(stream, context, DPAD_LEFT, 1, "a column west of the gym's door");
    if (dpad_through_door_north(stream, context, "Oreburgh Gym door")){
        return;
    }
    dpad_tiles(stream, context, DPAD_RIGHT, 2, "a column east of the gym's door");
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
            //  His badge speech is over when talk_or_fight() returns: the
            //  battle ends with its speeches (confirm_battle_overworld()).
            if (talk_or_fight(stream, context, "Roark", TalkExpect::BATTLE)){
                return !in_pokemon_center(stream);
            }
            continue;
        }
        if (up.walked <= 800ms && ++stuck >= 3){
            fail_step(stream, "Oreburgh Gym: stuck on the way up, and nobody answers A.");
        }
    }
    fail_step(stream, "Oreburgh Gym: did not reach Roark.");
}

constexpr Milliseconds MINE_PACE_STEP_HOLD = 400ms;
constexpr Milliseconds MINE_PACE_STEP_WAIT = 400ms;
constexpr Milliseconds MINE_ENCOUNTER_TIMEOUT = 120s;
constexpr Milliseconds MINE_BATTLE_MENU_WAIT = 20s;
constexpr Milliseconds MINE_COLUMN_WALK_LIMIT = 25000ms;
constexpr Milliseconds MINE_CAVERN_STEP_HOLD = 1500ms;
constexpr size_t MINE_TRAINING_BATTLES = 3;
constexpr size_t MINE_MAX_PREP_TRIPS = 6;

//  Paces north/south along column 12 in OreburghMineB1F (between the two
//  conveyor belts, well clear of both warps) until a wild battle opens.
//  Does not watch BlackScreenWatcher: the cave ceiling above the north
//  stairs fills the top of the frame with black void and false-triggers it.
//  `go_south` persists across encounters to avoid drifting into either warp.
void pace_for_mine_encounter(VideoStream& stream, ProControllerContext& context, bool& go_south){
    const auto deadline = current_time() + MINE_ENCOUNTER_TIMEOUT;
    while (current_time() < deadline){
        const double y = go_south ? -1.0 : 1.0;
        go_south = !go_south;
        BattleMenuWatcher menu(BattleType::STANDARD, COLOR_YELLOW);
        BattleDialogWatcher dialog(COLOR_YELLOW);
        const int hit = run_until<ProControllerContext>(stream, context,
            [y](ProControllerContext& ctx){
                pbf_move_left_joystick(ctx, {0, y}, MINE_PACE_STEP_HOLD, MINE_PACE_STEP_WAIT);
            }, {{menu}, {dialog}});
        if (hit == 0){
            return;
        }
        if (hit == 1){
            BattleMenuWatcher confirm(BattleType::STANDARD);
            if (wait_until(stream, context, MINE_BATTLE_MENU_WAIT, {{confirm}}) < 0){
                fail_step(stream, "Oreburgh Mine training: battle menu did not appear.");
            }
            return;
        }
    }
    fail_step(stream, "Oreburgh Mine training: no wild encounter within 2 minutes.");
}

//  Walks north out of OreburghMineB1F back up column 46/47 to Oreburgh
//  Pokemon Center, heals, and returns to the Center doorstep (47, 21).
//  Avoids find_lit_doorway() / enter_lit_door(): in afternoon sun, Oreburgh
//  Center's sunlit left roof corner matches lit_doorway_pixel() while the
//  shaded door glass does not, walking Dawn west off (47, 21) into the wall
//  (lab-B 2026-11-22 19:35).
void exit_mine_to_oreburgh_center(VideoStream& stream, ProControllerContext& context){
    const JoystickPosition north_stick{0, 1};
    size_t east_shifts = 0;
    for (size_t legs = 0; legs < 12; legs++){
        const WalkResult north = walk_until(stream, context, run_from_wild_battle, north_stick, MINE_COLUMN_WALK_LIMIT, "north to Oreburgh Center",
            [](const ImageViewRGB32& screen){ return pokemon_center_inside(screen); });
        if (north.stop == WalkStop::TRANSITION || north.stop == WalkStop::ARRIVED){
            if (north.stop == WalkStop::TRANSITION){
                confirm_battle_overworld(stream, context, "north transition to Oreburgh Center");
            }else{
                pbf_wait(context, 2000ms);
            }
            if (in_pokemon_center(stream)){
                heal_inside_pokemon_center(stream, context, "Oreburgh");
                return;
            }
            continue;
        }
        if (north.stop == WalkStop::BLOCKED){
            if (in_pokemon_center(stream)){
                heal_inside_pokemon_center(stream, context, "Oreburgh");
                return;
            }
            if (east_shifts == 0){
                dpad_tiles(stream, context, DPAD_RIGHT, 1, "column 47 toward Oreburgh Center door");
                east_shifts++;
            }else if (north.moved_chunks <= 1 && east_shifts < 4){
                //  Grazing the Hiker on the street can slide Dawn west onto the
                //  door's left pillar (lab-B 2026-11-22 20:41, 20:48): clear the
                //  pillar and sweep east ~0.6 tile across the doorway.
                pbf_move_left_joystick(context, {0, -1}, 120ms, 100ms);
                pbf_move_left_joystick(context, {1, 0}, 240ms, 200ms);
                east_shifts++;
            }else if (north.moved_chunks <= 1){
                pbf_move_left_joystick(context, {0, -1}, 120ms, 100ms);
                pbf_move_left_joystick(context, {-1, 0}, 300ms, 200ms);
            }else{
                context.wait_for(2000ms);
            }
        }
    }
    fail_step(stream, "Oreburgh Mine training: could not reach Oreburgh Center.");
}

//  From Oreburgh Center doorstep (47, 21), walks straight south down column 46
//  into OreburghMineB1F, fights up to `battles` wild battles (allowing evolution
//  and learning Mach Punch at Lv 14), and returns healed to (47, 21). Stops
//  early once Mach Punch is observed on the move menu.
bool train_in_oreburgh_mine(VideoStream& stream, ProControllerContext& context, size_t battles){
    stream.log(std::format("[Oreburgh Mine] Training (up to {} wild battles).", battles), COLOR_BLUE);
    dpad_tiles(stream, context, DPAD_LEFT, 1, "column 46 to Oreburgh Mine");
    const WalkResult enter = walk_until(stream, context, run_from_wild_battle, {0, -1}, MINE_COLUMN_WALK_LIMIT, "south into Oreburgh Mine");
    if (enter.stop != WalkStop::TRANSITION){
        fail_step(stream, "Oreburgh Mine training: did not enter Oreburgh Mine.");
    }
    confirm_battle_overworld(stream, context, "into Oreburgh Mine");
    walk_until(stream, context, run_from_wild_battle, {0, -1}, MINE_CAVERN_STEP_HOLD, "into Oreburgh Mine cavern");
    bool go_south = false;
    for (size_t done = 0; done < battles; done++){
        pace_for_mine_encounter(stream, context, go_south);
        const bool lead_ok = fight_wild_battle(stream, context, "Oreburgh Mine training");
        if (in_pokemon_center(stream)){
            stream.log("[Oreburgh Mine] Fainted during training; back at the Center with XP kept.", COLOR_ORANGE);
            walk_out_of_center(stream, context, "Oreburgh");
            return remembered_move_includes("mach-punch");
        }
        if (!lead_ok){
            stream.log("[Oreburgh Mine] Lead fainted and fled; heading back to heal.", COLOR_ORANGE);
            break;
        }
        if (remembered_move_includes("mach-punch")){
            stream.log("[Oreburgh Mine] Monferno has Mach Punch; heading back to heal.", COLOR_GREEN);
            break;
        }
    }
    exit_mine_to_oreburgh_center(stream, context);
    return remembered_move_includes("mach-punch");
}

//  01.03b: conditional pre-gym training for Chimchar so it enters 01.04 as
//  Monferno with Mach Punch and saves at Oreburgh Center doorstep (47, 21).
std::optional<Pokemon::StoryFailure> train_fire_starter_in_mine(StoryEnv& console, BdspRunState&, const BdspStep&){
    if (console.starter != StarterChoice::CHIMCHAR){
        console.env.console.log("[01.03b] Starter is not Chimchar; skipping Oreburgh Mine training.", COLOR_BLUE);
        return std::nullopt;
    }
    clear_remembered_moves();
    for (size_t trip = 0; trip < MINE_MAX_PREP_TRIPS; trip++){
        const bool ready = train_in_oreburgh_mine(console.env.console, console.context, MINE_TRAINING_BATTLES);
        save_game(console.env, console.context, "Oreburgh City");
        if (ready){
            return std::nullopt;
        }
    }
    return std::nullopt;
}

//  01.04: west along the row into the gym and up it to Roark, again from the Center after a loss.
void gym_until_roark_beaten(VideoStream& stream, ProControllerContext& context){
    // Losing costs some money and keeps the experience, so each try comes
    // back stronger: Roark won twice by hand against Piplup Lv 10 and 14 and
    // lost the third time. After a second loss (gym trainers already cleared,
    // e.g. Chimchar stuck at Lv 12 against Geodude), train in Oreburgh Mine
    // until Monferno evolves and learns Mach Punch.
    bool won = false;
    for (size_t attempt = 0; attempt < 6 && !won; attempt++){
        oreburgh_street_to_gym(stream, context);
        won = oreburgh_gym_battles(stream, context);
        if (!won){
            stream.log(std::format("[01.04] Lost in the gym (try {}); back from the Center.", attempt + 1), COLOR_ORANGE);
            walk_out_of_center(stream, context, "Oreburgh");
            if (attempt >= 1){
                train_in_oreburgh_mine(stream, context, MINE_TRAINING_BATTLES);
            }
        }
    }
    if (!won){
        fail_step(stream, "Oreburgh Gym: lost to Roark six times.");
    }
}

} // namespace


void add_oreburgh_events(StoryEvents& events){
    events.emplace("train_fire_starter_in_mine", train_fire_starter_in_mine);
    events.emplace("gym_until_roark_beaten", console_event(gym_until_roark_beaten));
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
