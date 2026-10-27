/*  Blueberry Catch Photo
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/Tools/ErrorDumper.h"
#include "CommonFramework/Exceptions/ProgramFinishedException.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_Superscalar.h"
#include "NintendoSwitch/Programs/NintendoSwitch_GameEntry.h"
#include "NintendoSwitch/Programs/DateSpam/NintendoSwitch_HomeToDateTime.h"
#include "NintendoSwitch/Programs/DateSpam/NintendoSwitch_RollDateForward1.h"
#include "VideoGames/PokemonSV/Programs/PokemonSV_GameEntry.h"
#include "VideoGames/PokemonSV/Programs/PokemonSV_SaveGame.h"
#include "VideoGames/PokemonSV/PokemonSV_Settings.h"
#include "VideoGames/PokemonSV/Programs/PokemonSV_GameEntry.h"
#include "VideoGames/PokemonSV/Inference/Dialogs/PokemonSV_DialogDetector.h"
#include "VideoGames/PokemonSV/Inference/Battles/PokemonSV_NormalBattleMenus.h"
#include "VideoGames/PokemonSV/Inference/Battles/PokemonSV_EncounterWatcher.h"
#include "VideoGames/PokemonSV/Inference/PokemonSV_MainMenuDetector.h"
#include "VideoGames/PokemonSV/Inference/Dialogs/PokemonSV_DialogDetector.h"
#include "VideoGames/PokemonSV/Inference/Overworld/PokemonSV_OverworldDetector.h"
#include "VideoGames/PokemonSV/Inference/Overworld/PokemonSV_TeraGlowDetector.h"
#include "VideoGames/PokemonSV/Inference/Overworld/PokemonSV_LetsGoHpReader.h"
#include "VideoGames/PokemonSV/Inference/Overworld/PokemonSV_PrideRockDetector.h"
#include "VideoGames/PokemonSV/Programs/PokemonSV_MenuNavigation.h"
#include "VideoGames/PokemonSV/Programs/PokemonSV_WorldNavigation.h"
#include "VideoGames/PokemonSV/Programs/Battles/PokemonSV_BasicCatcher.h"
#include "VideoGames/PokemonSV/Programs/Battles/PokemonSV_Battles.h"
#include "VideoGames/PokemonSV/Programs/PokemonSV_Terarium.h"
#include "PokemonSV_BBQLab.h"
#include "PokemonSV_BlueberryQuests.h"

#include<vector>
#include <functional>
#include <optional>
#include "PokemonSV_BlueberryCatchPhoto.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{

//The arrow on "Choose next Pokemon / Run away" after the lead faints. The old box,
//{0.610, 0.523, 0.044, 0.079}, ended before the arrow's right edge (0.672) and never
//saw it: R16 stopped twice on "Invalid state ret2_run" at this prompt.
static const ImageFloatBox FAINTED_PROMPT_ARROW_BOX(0.600, 0.500, 0.080, 0.120);

CameraAngle quest_photo_navi(
    const ProgramInfo& info,
    ConsoleHandle& console, ProControllerContext& context,
    const BBQOption& BBQ_OPTIONS,
    BBQuests current_quest
){
    CameraAngle angle = CameraAngle::none;

    //Navigate to target
    switch (current_quest){
        case BBQuests::photo_fly:
            console.log("Photo: In-flight");

            //Floating Pokemon count as in flight. Alolan Geodude by Central Plaza, the
            //catch-rock route; the Polar Duosion was missing 12 of 12 looks (R17).
            open_map_from_overworld(info, console, context);
            fly_to_overworld_from_map(info, console, context);
            pbf_press_button(context, BUTTON_L, 160ms, 400ms);
            pbf_move_left_joystick(context, {-0.453, +1}, 80ms, 160ms);
            pbf_press_button(context, BUTTON_L | BUTTON_PLUS, 160ms, 840ms);
            jump_glide_fly(console, context, BBQ_OPTIONS.INVERTED_FLIGHT, 800ms, 4400ms, 2400ms);
            pbf_press_button(context, BUTTON_PLUS, 160ms, 840ms);
            pbf_wait(context, 2400ms);
            context.wait_for_all_requests();

            break;
        case BBQuests::photo_psychic:
            console.log("Photo: Psychic");

            //Slowpoke on the beach north of Coastal Plaza: the glide lands on the sand, and
            //a 250 ms turn left puts one near the middle (in frame 3 of 3, 2026-10-25).
            //The Polar Duosion was missing 12 of 12 looks in R17.
            central_to_slowpoke_beach(info, console, context, BBQ_OPTIONS);
            pbf_move_right_joystick(context, {-1, 0}, 250ms, 300ms);

            break;
        case BBQuests::photo_swim: case BBQuests::photo_water: case BBQuests::photo_polar:
            console.log("Photo: Swimming/Water/Polar");

            //Polar Outdoor Classroom 1 - fixed Horsea
            central_to_polar_class1(info, console, context);

            pbf_wait(context, 1600ms);
            context.wait_for_all_requests();

            pbf_press_button(context, BUTTON_L, 80ms, 400ms);
            pbf_move_left_joystick(context, {+1, +0.609}, 1440ms, 160ms);

            angle = CameraAngle::down;

            break;
        case BBQuests::photo_coastal: case BBQuests::photo_grass: case BBQuests::photo_dragon:
            console.log("Photo: Coastal/Grass/Dragon");

            //Coastal Plaza - Exeggutor-A
            central_to_coastal_plaza(info, console, context);

            pbf_move_left_joystick(context, {-1, +0.102}, 3200ms, 160ms);

            //Jump down
            pbf_press_button(context, BUTTON_L | BUTTON_PLUS, 160ms, 840ms);
            ssf_press_button(context, BUTTON_B, 0ms, 800ms);
            ssf_press_button(context, BUTTON_B, 0ms, 160ms, 80ms);
            ssf_press_button(context, BUTTON_B, 0ms, 160ms);

            pbf_wait(context, 800ms);
            context.wait_for_all_requests();

            pbf_move_left_joystick(context, {0, +1}, 1200ms, 160ms);
            pbf_press_button(context, BUTTON_B, 160ms, 160ms);
            pbf_wait(context, 1600ms);
            pbf_press_button(context, BUTTON_L | BUTTON_PLUS, 160ms, 840ms);
            context.wait_for_all_requests();

            break;
        case BBQuests::photo_canyon: case BBQuests::photo_ghost: case BBQuests::photo_ground:
            console.log("Photo: Canyon/Ghost/Ground");

            //Canyon Plaza - Golett
            central_to_canyon_plaza(info, console, context);

            pbf_move_left_joystick(context, {+0.646, 0}, 80ms, 160ms);
            pbf_press_button(context, BUTTON_L | BUTTON_PLUS, 160ms, 840ms);

            jump_glide_fly(console, context, BBQ_OPTIONS.INVERTED_FLIGHT, 4800ms, 2000ms, 3200ms);

            pbf_press_button(context, BUTTON_L | BUTTON_PLUS, 160ms, 840ms);

            break;
        case BBQuests::photo_savanna: case BBQuests::photo_normal: case BBQuests::photo_fire:
            console.log("Photo: Normal/Fire");

            //Savanna Plaza - Pride Rock. take_photo() turns to Pyroar.
            central_to_pride_rock(info, console, context, BBQ_OPTIONS);
            break;
        case BBQuests::photo_bug: case BBQuests::photo_rock:
            console.log("Photo: Bug/Rock");

            //Kleavor
            central_to_canyon_plaza(info, console, context);

            pbf_move_left_joystick(context, {+0.606, +0.5}, 160ms, 840ms);
            pbf_press_button(context, BUTTON_L | BUTTON_PLUS, 160ms, 840ms);

            jump_glide_fly(console, context, BBQ_OPTIONS.INVERTED_FLIGHT, 8000ms, 12000ms, 2400ms);

            pbf_press_button(context, BUTTON_L | BUTTON_PLUS, 160ms, 840ms);

            break;
        case BBQuests::photo_fairy:
            console.log("Photo: Fairy");

            //Snubbull - Central plaza
            open_map_from_overworld(info, console, context);
            fly_to_overworld_from_map(info, console, context);

            pbf_move_left_joystick(context, {-1, +0.375}, 80ms, 160ms);
            pbf_press_button(context, BUTTON_L | BUTTON_PLUS, 160ms, 840ms);

            jump_glide_fly(console, context, BBQ_OPTIONS.INVERTED_FLIGHT, 16000ms, 12000ms, 1600ms);

            pbf_press_button(context, BUTTON_PLUS, 160ms, 840ms);

            pbf_move_left_joystick(context, {+1, +1}, 80ms, 160ms);
            pbf_press_button(context, BUTTON_L, 160ms, 400ms);

            angle = CameraAngle::down;
            break;
        case BBQuests::photo_ice:
            console.log("Photo: Ice");

            //Snover - Start at central plaza, fly north-ish
            open_map_from_overworld(info, console, context);
            fly_to_overworld_from_map(info, console, context);

            pbf_move_left_joystick(context, {-1, +1}, 80ms, 160ms);
            pbf_press_button(context, BUTTON_L | BUTTON_PLUS, 160ms, 840ms);

            jump_glide_fly(console, context, BBQ_OPTIONS.INVERTED_FLIGHT, 8800ms, 13600ms, 1600ms);

            pbf_press_button(context, BUTTON_PLUS, 160ms, 840ms);
            pbf_move_left_joystick(context, {-1, 0}, 160ms, 400ms);
            pbf_press_button(context, BUTTON_L, 160ms, 400ms);

            break;
        case BBQuests::photo_electric:
            console.log("Photo: Electric");

            //Electabuzz
            central_to_canyon_rest(info, console, context);

            pbf_move_left_joystick(context, {+1, -1}, 80ms, 160ms);
            pbf_press_button(context, BUTTON_L, 160ms, 400ms);
            pbf_move_left_joystick(context, {0, +1}, 3200ms, 160ms);

            break;
        case BBQuests::photo_fighting:
            console.log("Photo: Fighting");

            //Hitmontop (TERA ICE) - Canyon Plaza or Classroom
            central_to_canyon_plaza(info, console, context);
            pbf_move_left_joystick(context, {-1, 0}, 3200ms, 160ms);
            pbf_press_button(context, BUTTON_L, 80ms, 400ms);
            pbf_move_left_joystick(context, {-1, +0.219}, 160ms, 400ms);
            pbf_press_button(context, BUTTON_L | BUTTON_PLUS, 160ms, 840ms);

            jump_glide_fly(console, context, BBQ_OPTIONS.INVERTED_FLIGHT, 1600ms, 4000ms, 6400ms);

            pbf_press_button(context, BUTTON_PLUS, 160ms, 800ms);
            context.wait_for_all_requests();

            break;
        case BBQuests::photo_poison: case BBQuests::photo_dark:
            console.log("Photo: Poison/Dark");

            //Muk-A - area a bit laggy but consistently so
            central_to_coastal_plaza(info, console, context);
            pbf_move_left_joystick(context, {-1, 0}, 160ms, 400ms);

            pbf_press_button(context, BUTTON_L | BUTTON_PLUS, 160ms, 840ms);

            if (console.state().console_type() == ConsoleType::Switch1){
                jump_glide_fly(console, context, BBQ_OPTIONS.INVERTED_FLIGHT, 8000ms, 11200ms, 2400ms);
            }else{ //Switch 2
                jump_glide_fly(console, context, BBQ_OPTIONS.INVERTED_FLIGHT, 8000ms, 10400ms, 2400ms);
            }
            pbf_press_button(context, BUTTON_PLUS, 160ms, 840ms);
            pbf_move_left_joystick(context, {0, +1}, 1200ms, 400ms);
            pbf_move_left_joystick(context, {+0.409, +1}, 160ms, 400ms);
            pbf_wait(context, 1600ms); //Give it time to spawn/load.
            context.wait_for_all_requests();

            angle = CameraAngle::down;

            break;
        case BBQuests::photo_steel:
            console.log("Photo: Steel");

            //Dugtrio-A - area a bit laggy but should work most of the time
            central_to_coastal_plaza(info, console, context);
            pbf_move_left_joystick(context, {-1, 0}, 160ms, 400ms);

            pbf_press_button(context, BUTTON_L | BUTTON_PLUS, 160ms, 840ms);

            if (console.state().console_type() == ConsoleType::Switch1){
                jump_glide_fly(console, context, BBQ_OPTIONS.INVERTED_FLIGHT, 1600ms, 4600ms, 1600ms);
            }else{ //Switch 2
                jump_glide_fly(console, context, BBQ_OPTIONS.INVERTED_FLIGHT, 1600ms, 400ms, 1600ms);
            }

            pbf_press_button(context, BUTTON_PLUS, 160ms, 840ms);
            pbf_move_left_joystick(context, {-1, 0}, 160ms, 400ms);
            pbf_press_button(context, BUTTON_L, 160ms, 400ms);

            break;
        default:
            OperationFailedExceptionWithScreenshot::fire(
                ErrorReportMode::SEND_ERROR_REPORT,
                "Invalid photo quest.",
                console
            );
            break;
    }

    return angle;
}

//Open photo mode, tilt the camera, take the photo and return to the overworld.
//Turn the camera until Pyroar's mane is in the middle. Returns whether it was seen.
static bool center_red_mane(ConsoleHandle& console, ProControllerContext& context, bool photo_mode){
    size_t turns = 0;
    bool seen = false;
    for (size_t step = 0; step < 8; step++){
        context.wait_for_all_requests();
        VideoSnapshot screen = console.video().snapshot();
        if (!screen.frame){
            break;
        }
        const std::optional<double> x = find_red_mane_x(*screen.frame, photo_mode);
        if (!x){
            console.log("Pyroar mane: none (look " + std::to_string(step + 1) + ").");
            if (turns >= 3){
                break;
            }
            //About a quarter turn.
            pbf_move_right_joystick(context, {+1, 0}, 450ms, 300ms);
            turns++;
            continue;
        }
        seen = true;
        const double dx = *x - 0.5;
        console.log("Pyroar mane at x=" + std::to_string(*x) + ".");
        if (dx < 0.05 && dx > -0.05){
            break;
        }
        //About 350 ms pans the camera one frame width.
        pbf_move_right_joystick(context, {dx > 0 ? +1.0 : -1.0, 0}, Milliseconds((int64_t)(std::abs(dx) * 350) + 40), 300ms);
    }
    return seen;
}

//Pyroar wanders on Pride Rock: in the photo box, past either edge, above it, or out of
//view behind the slope (console 2026-10-26, 6 photos). Photo mode moves the camera to the
//player, so a Pyroar centered in the overworld sits at the right edge of the photo
//(x=0.57 -> 0.92). Face it in the overworld first, with a short step so the player turns
//to the camera, then center it again in photo mode.
static void face_red_mane(ConsoleHandle& console, ProControllerContext& context){
    if (center_red_mane(console, context, false)){
        pbf_move_left_joystick(context, {0, +1}, 100ms, 400ms);
    }
}

bool take_photo(ConsoleHandle& console, ProControllerContext& context, CameraAngle move_camera, bool aim_at_red_mane = false){
    //Take photo.
    console.log("Taking photo.");
    bbq_lab_event(console, context, "route_done");
    if (aim_at_red_mane){
        face_red_mane(console, context);
    }
    PromptDialogWatcher photo_prompt(COLOR_RED);
    OverworldWatcher overworld(console.logger(), COLOR_BLUE);

    pbf_press_dpad(context, DPAD_DOWN, 400ms, 160ms);
    pbf_wait(context, 800ms);
    context.wait_for_all_requests();

    if (move_camera == CameraAngle::up){
        pbf_move_right_joystick(context, {0, +1}, 400ms, 160ms);
    }else if (move_camera == CameraAngle::down){
        pbf_move_right_joystick(context, {0, -1}, 400ms, 160ms);
    }
    if (aim_at_red_mane){
        center_red_mane(console, context, true);
        bbq_lab_event(console, context, "aimed");
    }
    context.wait_for_all_requests();
    bbq_lab_event(console, context, "photo");

    pbf_press_button(context, BUTTON_A, 160ms, 400ms);

    int ret = wait_until(
        console, context,
        std::chrono::seconds(10),
        { photo_prompt }
    );
    const bool taken = ret == 0;
    if (!taken){
        console.log("Photo not taken.");
    }

    //Mash B until overworld
    int exit = run_until<ProControllerContext>(
        console, context,
        [&](ProControllerContext& context){
            pbf_mash_button(context, BUTTON_B, 16000ms);
        },
        {{ overworld }}
    );
    if (exit == 0){
        console.log("Overworld detected.");
    }
    return taken;
}

//Take a photo of a Flying type: run into one at the canyon grassland, flee, and photograph
//it while it is still in front. Flying Pokemon are hard to frame in the overworld, but
//after fleeing the target stays next to the player.
void quest_photo_after_fleeing(
    const ProgramInfo& info,
    ConsoleHandle& console, ProControllerContext& context,
    const BBQOption& BBQ_OPTIONS
){
    console.log("Photo: Flying");
    EncounterWatcher encounter_watcher(console, COLOR_RED);
    bool battle_menu_detected = false;
    int ret = run_until<ProControllerContext>(
        console, context,
        [&](ProControllerContext& context){
            //Same spot as catch_flying: Scyther, Altaria, Swablu and Fletchling spawn there.
            central_to_canyon_plaza(info, console, context);

            pbf_move_left_joystick(context, {+0.606, +0.5}, 160ms, 840ms);
            pbf_press_button(context, BUTTON_L | BUTTON_PLUS, 160ms, 840ms);

            if (console.state().console_type() == ConsoleType::Switch1){
                jump_glide_fly(console, context, BBQ_OPTIONS.INVERTED_FLIGHT, 8000ms, 13200ms, 2400ms);
            }else{ //All switch 2s
                jump_glide_fly(console, context, BBQ_OPTIONS.INVERTED_FLIGHT, 8000ms, 12800ms, 2400ms);
            }

            battle_menu_detected = send_out_and_look_around(console, context, std::chrono::seconds(6), "a flying Pokemon");
        },
        {
            static_cast<VisualInferenceCallback&>(encounter_watcher),
            static_cast<AudioInferenceCallback&>(encounter_watcher),
        }
    );
    encounter_watcher.throw_if_no_sound();

    if (ret < 0 && !battle_menu_detected){
        console.log("No battle started. Returning to plaza; the quest stays on the board.");
        return_to_plaza(info, console, context, BBQ_OPTIONS.LANGUAGE);
        return;
    }
    if ((bool)encounter_watcher.shiny_screenshot()){
        console.log("Shiny detected!");
        pbf_press_button(context, BUTTON_CAPTURE, 2000ms, 5000ms);
        throw ProgramFinishedException();
    }

    console.log("Fleeing, then taking the photo.");
    //Keep choosing Run while the battle menu comes back, so a failed escape can't leave us
    //in the battle. After 5 tries, return_to_plaza() below handles what is left.
    for (int i = 0; i < 5; i++){
        NormalBattleMenuWatcher battle_menu(COLOR_YELLOW);
        context.wait_for_all_requests();
        if (wait_until(console, context, std::chrono::seconds(5), {battle_menu}) != 0){
            break;
        }
        battle_menu.move_to_slot(console, context, 3);
        pbf_press_button(context, BUTTON_A, 80ms, 400ms);
        pbf_mash_button(context, BUTTON_B, 2000ms);
    }
    press_Bs_to_back_to_overworld(info, console, context);

    take_photo(console, context, CameraAngle::none);
    leave_quest_area(info, console, context, BBQ_OPTIONS, BBQuests::photo_flying);
}

//The Geodude for photo-fly lands right next to the player, at the edge of the photo or
//out of it: photo mode moves the camera to the player (console 2026-10-26; R22 3 of 6
//tries, then 0 of 2 with extra photos turned 30 degrees either way). Lock on with ZL,
//which turns the camera to it, and step back while locked. Walking back turns the
//player away, so lock on again and tap forward to face it before the photo.
static void back_off_facing_target(ProControllerContext& context){
    ssf_press_button(context, BUTTON_ZL, 1000ms, 3000ms);
    ssf_press_left_joystick(context, {0, -1}, 0ms, 900ms);
    pbf_wait(context, 2500ms);
    ssf_press_button(context, BUTTON_ZL, 1000ms, 2000ms);
    ssf_press_left_joystick(context, {0, +1}, 0ms, 100ms);
    pbf_wait(context, 1500ms);
    context.wait_for_all_requests();
}

void quest_photo(
    const ProgramInfo& info,
    ConsoleHandle& console, ProControllerContext& context,
    const BBQOption& BBQ_OPTIONS,
    BBQuests current_quest
){
    if (current_quest == BBQuests::photo_flying){
        quest_photo_after_fleeing(info, console, context, BBQ_OPTIONS);
        return;
    }

    bool took_photo = false;
    CameraAngle move_camera = CameraAngle::none;

    while(!took_photo){
        EncounterWatcher encounter_watcher(console, COLOR_RED);
        int ret = run_until<ProControllerContext>(
            console, context,
            [&](ProControllerContext& context){

                move_camera = quest_photo_navi(info, console, context, BBQ_OPTIONS, current_quest);

                if (current_quest == BBQuests::photo_fly){
                    back_off_facing_target(context);
                }
                take_photo(
                    console, context, move_camera,
                    current_quest == BBQuests::photo_savanna || current_quest == BBQuests::photo_normal || current_quest == BBQuests::photo_fire
                );
                took_photo = true;
                context.wait_for_all_requests();
            },
            {
                static_cast<VisualInferenceCallback&>(encounter_watcher),
                static_cast<AudioInferenceCallback&>(encounter_watcher),
            }
        );
        encounter_watcher.throw_if_no_sound();

        if (ret >= 0){
            console.log("Battle menu detected.");

            bool is_shiny = (bool)encounter_watcher.shiny_screenshot();
            if (is_shiny){
                console.log("Shiny detected!");
                pbf_press_button(context, BUTTON_CAPTURE, 2000ms, 5000ms);
                throw ProgramFinishedException();
            }else{
                console.log("Detected battle. Running from battle and returning to plaza.");
                try{
                    //Smoke Ball or Flying type required due to Arena Trap/Magnet Pull
                    NormalBattleMenuWatcher battle_menu(COLOR_YELLOW);
                    battle_menu.move_to_slot(console, context, 3);
                    pbf_press_button(context, BUTTON_A, 80ms, 400ms);
                    press_Bs_to_back_to_overworld(info, console, context);
                    return_to_plaza(info, console, context, BBQ_OPTIONS.LANGUAGE);
                }catch (...){
                    console.log("Unable to flee.");
                    OperationFailedExceptionWithScreenshot::fire(
                        ErrorReportMode::SEND_ERROR_REPORT,
                        "Unable to flee!",
                        console
                    );
                }
            }
        }
    }
    context.wait_for_all_requests();
    leave_quest_area(info, console, context, BBQ_OPTIONS, current_quest);
    if (quest_spot_needs_reload(current_quest)){
        skip_day_at_quest_spot(info, console, context);
    }
}

//Whether the lead is out in Let's Go mode: its HP bar shows at the bottom left.
static bool lead_out_in_lets_go(ConsoleHandle& console, ProControllerContext& context){
    LetsGoHpWatcher hp_watcher(COLOR_RED);
    wait_until(console, context, std::chrono::milliseconds(1500), { hp_watcher });
    return hp_watcher.last_known_value() > 0;
}

//Hold ZL to lock on to the nearest Pokemon (the camera turns to it and its name and level
//show above it), then send out the lead at it with ZR. With nothing to lock on to, ZR
//sends the lead out in Let's Go mode instead and no battle starts (console 2026-10-26: a
//Geodude a few steps to the left). Then call the lead back with R, turn about a quarter
//and try again. Returns whether a battle started.
static bool send_out_locked_on(ConsoleHandle& console, ProControllerContext& context, size_t tries){
    bbq_lab_event(console, context, "route_done");
    for (size_t attempt = 0; attempt < tries; attempt++){
        if (attempt > 0){
            pbf_move_right_joystick(context, {+1, 0}, 450ms, 300ms);
        }
        ssf_press_button(context, BUTTON_ZL, 1200ms, 3500ms);
        ssf_press_button(context, BUTTON_ZR, 0ms, 400ms);
        pbf_wait(context, 3000ms);
        context.wait_for_all_requests();
        NormalBattleMenuWatcher battle_menu(COLOR_YELLOW);
        if (wait_until(console, context, std::chrono::seconds(8), { battle_menu }) == 0){
            return true;
        }
        console.log("Lock-on " + std::to_string(attempt + 1) + "/" + std::to_string(tries) + ": no battle.", COLOR_ORANGE);
        if (lead_out_in_lets_go(console, context)){
            console.log("The lead is out in Let's Go mode: calling it back.");
            pbf_press_button(context, BUTTON_R, 160ms, 1000ms);
        }
    }
    bbq_lab_event(console, context, "no_battle");
    return false;
}

//Send out the lead at the target straight ahead, stepping closer between tries. Unlike
//send_out_and_look_around(), it never turns away: after following a glow the target is
//in front, and the look-around turned the camera off it (console 2026-10-26).
static bool send_out_ahead(ConsoleHandle& console, ProControllerContext& context, size_t tries){
    for (size_t i = 0; i < tries; i++){
        if (i > 0){
            console.log("No battle yet. Stepping closer (" + std::to_string(i) + "/" + std::to_string(tries - 1) + ").");
            pbf_move_left_joystick(context, {0, +1}, 500ms, 200ms);
        }
        ssf_press_button(context, BUTTON_ZR, 0ms, 1600ms);
        ssf_press_button(context, BUTTON_ZL, 800ms, 400ms);
        ssf_press_button(context, BUTTON_ZL, 1200ms, 400ms);
        pbf_wait(context, 2400ms);
        context.wait_for_all_requests();

        NormalBattleMenuWatcher battle_menu(COLOR_YELLOW);
        if (wait_until(console, context, std::chrono::seconds(6), { battle_menu }) == 0){
            return true;
        }
    }
    return false;
}

//Fly from Central to Rabsca, walk to its glow and send out the lead.
static void central_to_rabsca_and_send_out(
    const ProgramInfo& info,
    ConsoleHandle& console, ProControllerContext& context,
    const BBQOption& BBQ_OPTIONS
){
    central_to_savanna_rest_area_2(info, console, context);

    //Turned further right and glided longer than the first version (5.5 s), to land
    //closer to Rabsca, past the Deerling and Trapinch near the rest area.
    pbf_move_left_joystick(context, {+1, -0.35}, 80ms, 160ms);
    pbf_press_button(context, BUTTON_L | BUTTON_PLUS, 160ms, 840ms);
    jump_glide_fly(console, context, BBQ_OPTIONS.INVERTED_FLIGHT, 3000ms, 6500ms, 2400ms);

    pbf_press_button(context, BUTTON_PLUS, 160ms, 840ms);
    pbf_move_left_joystick(context, {+0.6, +0.8}, 80ms, 160ms);
    pbf_press_button(context, BUTTON_L, 160ms, 400ms);

    if (follow_tera_glow(console, context)){
        //The first send-out from where the glow-follow stops missed 4 of 4 times, and
        //one 500 ms step closer hit each time (console 2026-10-26).
        pbf_move_left_joystick(context, {0, +1}, 500ms, 200ms);
        send_out_ahead(console, context, 3);
    }else{
        send_out_and_look_around(console, context, std::chrono::seconds(6), "Rabsca");
    }
}

void quest_catch_navi(
    const ProgramInfo& info,
    ConsoleHandle& console, ProControllerContext& context,
    const BBQOption& BBQ_OPTIONS,
    BBQuests current_quest
){
    switch (current_quest){
        case BBQuests::catch_any:
        case BBQuests::catch_normal: case BBQuests::catch_fire:
            //Any wild Pokemon counts for catch_any. The Chargestone court was 1/3 (console
            //2026-10-26): the spawns stood too far for the lead. Pride Rock's Pyroar,
            //reloaded from Central Plaza, was 3/3.
            console.log(current_quest == BBQuests::catch_any ? "Catch: Any (Pride Rock)" : "Catch: Normal/Fire");

            //Savanna Plaza - Pride Rock
            central_to_pride_rock(info, console, context, BBQ_OPTIONS);
            break;

        case BBQuests::catch_psychic:
            console.log("Catch: Psychic");

            //Slowpoke on the beach north of Coastal Plaza (see photo-psychic). Face it,
            //then the ball below locks on.
            central_to_slowpoke_beach(info, console, context, BBQ_OPTIONS);
            pbf_press_button(context, BUTTON_PLUS, 160ms, 840ms);
            pbf_move_left_joystick(context, {-0.4, +0.92}, 160ms, 400ms);
            pbf_press_button(context, BUTTON_L, 160ms, 400ms);

            break;

        case BBQuests::catch_grass: case BBQuests::catch_dragon:
            console.log("Catch: Grass/Dragon");

            //Coastal Plaza - Exeggutor-A
            central_to_coastal_plaza(info, console, context);

            pbf_move_left_joystick(context, {-1, +0.102}, 3200ms, 160ms);

            //Jump down
            pbf_press_button(context, BUTTON_L | BUTTON_PLUS, 160ms, 840ms);
            ssf_press_button(context, BUTTON_B, 0ms, 800ms);
            ssf_press_button(context, BUTTON_B, 0ms, 160ms, 80ms);
            ssf_press_button(context, BUTTON_B, 0ms, 160ms);

            pbf_wait(context, 800ms);
            context.wait_for_all_requests();

            pbf_move_left_joystick(context, {0, +1}, 2800ms, 160ms);
            pbf_press_button(context, BUTTON_B, 160ms, 160ms);
            pbf_wait(context, 1600ms);
            pbf_press_button(context, BUTTON_L | BUTTON_PLUS, 160ms, 840ms);

            break;
        case BBQuests::catch_ghost: case BBQuests::catch_ground:
            console.log("Catch: Ghost/Ground");

            //Canyon Plaza - Golett
            central_to_canyon_plaza(info, console, context);

            pbf_move_left_joystick(context, {+0.646, 0}, 80ms, 160ms);
            pbf_press_button(context, BUTTON_L | BUTTON_PLUS, 160ms, 840ms);

            jump_glide_fly(console, context, BBQ_OPTIONS.INVERTED_FLIGHT, 4800ms, 2400ms, 3200ms);

            pbf_press_button(context, BUTTON_L | BUTTON_PLUS, 160ms, 840ms);

            pbf_move_left_joystick(context, {-1, +1}, 80ms, 160ms);
            pbf_press_button(context, BUTTON_L, 160ms, 400ms);
            pbf_move_left_joystick(context, {0, +1}, 400ms, 160ms);

            break;
        case BBQuests::catch_fairy:
            console.log("Catch: Fairy");

            //Snubbull - Central plaza
            //Tested while snowing, seems fine?
            open_map_from_overworld(info, console, context);
            fly_to_overworld_from_map(info, console, context);

            pbf_move_left_joystick(context, {-1, +0.375}, 80ms, 160ms);
            pbf_press_button(context, BUTTON_L | BUTTON_PLUS, 160ms, 840ms);

            jump_glide_fly(console, context, BBQ_OPTIONS.INVERTED_FLIGHT, 16000ms, 12000ms, 1600ms);

            pbf_press_button(context, BUTTON_PLUS, 160ms, 840ms);

            pbf_move_left_joystick(context, {+1, +1}, 80ms, 160ms);
            pbf_press_button(context, BUTTON_L, 160ms, 400ms);

            break;
        case BBQuests::catch_fighting:
            console.log("Catch: Fighting");

            //Hitmontop (TERA ICE) - Canyon Plaza or Classroom
            central_to_canyon_plaza(info, console, context);
            pbf_move_left_joystick(context, {-1, 0}, 3200ms, 160ms);
            pbf_press_button(context, BUTTON_L, 80ms, 400ms);
            pbf_move_left_joystick(context, {-1, +0.219}, 160ms, 400ms);
            pbf_press_button(context, BUTTON_L | BUTTON_PLUS, 160ms, 840ms);

            jump_glide_fly(console, context, BBQ_OPTIONS.INVERTED_FLIGHT, 1600ms, 4000ms, 6400ms);

            break;

        case BBQuests::catch_bug:
            console.log("Catch: Bug");

            //The static Tera Rabsca (Lv 65) northeast of Savanna Rest Area 2. Its battle
            //weakens it before the throws: caught with 2 balls in ~90 s (console 2026-10-26),
            //where the Kleavor route took ~16 balls and 10 min (R21). Walking into it does
            //not start the battle; sending out the lead does.
            central_to_rabsca_and_send_out(info, console, context, BBQ_OPTIONS);
            break;
        case BBQuests::catch_flying:
            console.log("Catch: Flying");

            if (BBQ_OPTIONS.COASTAL_FLYING_CATCH){
                central_to_coastal_plaza(info, console, context);
                coastal_plaza_to_grassland(console, context, BBQ_OPTIONS.INVERTED_FLIGHT);
                send_out_and_look_around(console, context, std::chrono::seconds(6), "a flying Pokemon");
                break;
            }

            //Canyon grassland at Black Augurite, where the Tera-self quest glides: Scyther,
            //Altaria, Swablu and Fletchling spawn around the fixed Kleavor. The Savanna
            //route waited for one Vullaby/Mandibuzz spawn and was marked not possible.
            central_to_canyon_plaza(info, console, context);

            pbf_move_left_joystick(context, {+0.606, +0.5}, 160ms, 840ms);
            pbf_press_button(context, BUTTON_L | BUTTON_PLUS, 160ms, 840ms);

            if (console.state().console_type() == ConsoleType::Switch1){
                jump_glide_fly(console, context, BBQ_OPTIONS.INVERTED_FLIGHT, 8000ms, 13200ms, 2400ms);
            }else{ //All switch 2s
                jump_glide_fly(console, context, BBQ_OPTIONS.INVERTED_FLIGHT, 8000ms, 12800ms, 2400ms);
            }

            send_out_and_look_around(console, context, std::chrono::seconds(6), "a flying Pokemon");
            break;
        case BBQuests::catch_rock: case BBQuests::catch_electric:
            console.log("Catch: Rock/Electric");

            //Geodude-A
            open_map_from_overworld(info, console, context);
            fly_to_overworld_from_map(info, console, context);
            pbf_press_button(context, BUTTON_L, 160ms, 400ms);
            pbf_move_left_joystick(context, {-0.453, +1}, 80ms, 160ms);
            pbf_press_button(context, BUTTON_L | BUTTON_PLUS, 160ms, 840ms);

            jump_glide_fly(console, context, BBQ_OPTIONS.INVERTED_FLIGHT, 800ms, 4400ms, 2400ms);

            pbf_press_button(context, BUTTON_PLUS, 160ms, 840ms);
            pbf_wait(context, 2400ms);
            context.wait_for_all_requests();

            //Geodude stands near the landing but does not walk in: R22 waited 0 of 2
            //times into a battle, once with a Geodude a few steps to the right, and the
            //look-around, which sends the lead straight ahead, missed it 30 degrees off.
            //Locking on first turns to it: 3 of 3 battles (console 2026-10-26).
            send_out_locked_on(console, context, 4);
            break;
        case BBQuests::catch_steel:
            console.log("Catch: Steel");

            //Dugtrio-A - area a bit laggy but should work most of the time
            central_to_coastal_plaza(info, console, context);
            pbf_move_left_joystick(context, {-1, 0}, 160ms, 400ms);

            pbf_press_button(context, BUTTON_L | BUTTON_PLUS, 160ms, 840ms);

            if (console.state().console_type() == ConsoleType::Switch1){
                jump_glide_fly(console, context, BBQ_OPTIONS.INVERTED_FLIGHT, 1600ms, 4600ms, 1600ms);
            }else{ //Switch 2
                jump_glide_fly(console, context, BBQ_OPTIONS.INVERTED_FLIGHT, 1600ms, 4000ms, 1600ms);
            }

            pbf_press_button(context, BUTTON_PLUS, 160ms, 840ms);
            pbf_move_left_joystick(context, {-1, 0}, 160ms, 400ms);
            pbf_press_button(context, BUTTON_L, 160ms, 400ms);
            pbf_move_left_joystick(context, {0, +1}, 400ms, 400ms);

            break;
        case BBQuests::catch_poison: case BBQuests::catch_dark:
            console.log("Catch: Poison/Dark");

            //Alolan Grimer at the hill west of Coastal Plaza: 3 of 10 looks from its middle
            //(2026-10-25). The old Alolan Muk glide never caught one (0/5 by R16).
            central_to_coastal_plaza(info, console, context);
            coastal_plaza_to_grassland(console, context, BBQ_OPTIONS.INVERTED_FLIGHT);
            send_out_and_look_around(console, context, std::chrono::seconds(6), "a wild Pokemon");
            break;
        case BBQuests::catch_water: case BBQuests::catch_ice:
            console.log("Catch: Water/Ice");

            //Lapras - Tera Bug
            central_to_polar_rest(info, console, context);
            pbf_press_button(context, BUTTON_L, 80ms, 400ms);
            pbf_move_left_joystick(context, {0, +1}, 1840ms, 160ms);
            pbf_move_left_joystick(context, {-1, 0}, 2400ms, 160ms);
            pbf_press_button(context, BUTTON_L, 160ms, 400ms);
            pbf_move_left_joystick(context, {-0.844, +1}, 160ms, 400ms);
            pbf_press_button(context, BUTTON_L | BUTTON_PLUS, 160ms, 840ms);

            jump_glide_fly(console, context, BBQ_OPTIONS.INVERTED_FLIGHT, 5600ms, 13600ms, 2400ms);

            break;
        default:
            OperationFailedExceptionWithScreenshot::fire(
                ErrorReportMode::SEND_ERROR_REPORT,
                "Invalid catch quest.",
                console
            );
            break;
    }

    //Lock on and throw ball
    ssf_press_button(context, BUTTON_ZR, 0ms, 1600ms);
    ssf_press_button(context, BUTTON_ZL, 800ms, 400ms);
    ssf_press_button(context, BUTTON_ZL, 1200ms, 400ms);

    pbf_wait(context, 2400ms);
    context.wait_for_all_requests();

}

void quest_catch_throw_ball(
    const ProgramInfo& info,
    ConsoleHandle& console, ProControllerContext& context,
    Language language,
    const std::string& selected_ball
){
    BattleBallReader reader(console, language);
    std::string ball_reader = "";
    WallClock start = current_time();

    console.log("Opening ball menu...");
    while (ball_reader == ""){
        if (current_time() - start > std::chrono::minutes(2)){
            console.log("Timed out trying to read ball after 2 minutes.", COLOR_RED);
            OperationFailedExceptionWithScreenshot::fire(
                ErrorReportMode::SEND_ERROR_REPORT,
                "Timed out trying to read ball after 2 minutes.",
                console
            );
        }

        //Mash B to exit anything else
        pbf_mash_button(context, BUTTON_B, 1000ms);
        context.wait_for_all_requests();

        //Press X to open Ball menu
        pbf_press_button(context, BUTTON_X, 160ms, 800ms);
        context.wait_for_all_requests();

        VideoSnapshot screen = console.video().snapshot();
        ball_reader = reader.read_ball(screen);
    }

    console.log("Selecting ball.");
    int quantity = move_to_ball(reader, console, context, selected_ball);
    if (quantity == 0){
        console.log("Unable to find ball.");
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "Unable to find ball.",
            console
        );
    }
    if (quantity < 0){
        console.log("Unable to read ball quantity.", COLOR_RED);
    }

    //Throw ball
    console.log("Throwing ball.");
    pbf_mash_button(context, BUTTON_A, 1200ms);
    context.wait_for_all_requests();

    pbf_mash_button(context, BUTTON_B, 7200ms);
    context.wait_for_all_requests();
}

void quest_catch_handle_battle(
    const ProgramInfo& info,
    ConsoleHandle& console, ProControllerContext& context,
    const BBQOption& BBQ_OPTIONS,
    BBQuests current_quest
){
    console.log("Catching Pokemon.");
    AdvanceDialogWatcher advance_dialog(COLOR_MAGENTA);
    PromptDialogWatcher add_to_party(COLOR_PURPLE);
    OverworldWatcher overworld(console.logger(), COLOR_RED);

    uint8_t switch_party_slot = 1;
    bool quickball_thrown = false;
    bool tera_target = false;
    bool use_quickball = BBQ_OPTIONS.QUICKBALL;

    int ret2 = run_until<ProControllerContext>(
        console, context,
        [&](ProControllerContext& context){
            while (true){
                //Check that battle menu appears - this is in case of swapping pokemon
                NormalBattleMenuWatcher menu_before_throw(COLOR_YELLOW);
                int bMenu = wait_until(
                    console, context,
                    std::chrono::seconds(15),
                    { menu_before_throw }
                );
                if (bMenu < 0){
                    console.log("Unable to find menu_before_throw.");
                    OperationFailedExceptionWithScreenshot::fire(
                        ErrorReportMode::SEND_ERROR_REPORT,
                        "Unable to find menu_before_throw.",
                        console
                    );
                }
                
                //Check for catch menu - if none it is a tera pokemon
                console.log("Checking for ball menu.");
                BattleBallReader exists(console, BBQ_OPTIONS.LANGUAGE);
                std::string ball_exists = "";

                pbf_press_button(context, BUTTON_X, 160ms, 800ms);
                context.wait_for_all_requests();

                VideoSnapshot screen_ball = console.video().snapshot();
                ball_exists = exists.read_ball(screen_ball);

                if (ball_exists == ""){
                    console.log("Could not find ball reader. Tera battle. Using first attack.");
                    pbf_mash_button(context, BUTTON_B, 1000ms);
                    context.wait_for_all_requests();

                    pbf_mash_button(context, BUTTON_A, 2400ms);
                    context.wait_for_all_requests();

                    tera_target = true;
                }else{
                    //Quick ball occurs before anything else in battle.
                    //Do not throw if target is a tera pokemon.
                    if (use_quickball && !quickball_thrown && tera_target == false){
                        console.log("Quick Ball option checked. Throwing Quick Ball.");
                        quest_catch_throw_ball(info, console, context, BBQ_OPTIONS.LANGUAGE, "quick-ball");
                        quickball_thrown = true;
                    }else{
                        console.log("Throwing selected ball.");
                        quest_catch_throw_ball(info, console, context, BBQ_OPTIONS.LANGUAGE, BBQ_OPTIONS.BALL_SELECT.slug());
                    }

                    //Check for battle menu, if found use fourth attack this turn
                    NormalBattleMenuWatcher battle_menu(COLOR_YELLOW);
                    int ret = wait_until(
                        console, context,
                        std::chrono::seconds(4),
                        { battle_menu }
                    );
                    if (ret == 0){
                        console.log("Battle menu detected early. Using fourth attack.");
                        MoveSelectWatcher move_watcher(COLOR_BLUE);
                        MoveSelectDetector move_select(COLOR_BLUE);

                        int ret_move_select = run_until<ProControllerContext>(
                            console, context,
                            [&](ProControllerContext& context){
                                pbf_press_button(context, BUTTON_A, 80ms, 400ms);
                                pbf_wait(context, 800ms);
                                context.wait_for_all_requests();
                            },
                            { move_watcher }
                        );
                        if (ret_move_select != 0){
                            console.log("Could not find move select.");
                        }else{
                            console.log("Move select found!");
                        }

                        context.wait_for_all_requests();
                        move_select.move_to_slot(console, context, 3);
                        pbf_mash_button(context, BUTTON_A, 1200ms);
                        pbf_wait(context, 800ms);
                        context.wait_for_all_requests();

                        //Check for battle menu
                        //If found after a second, assume out of PP and stop as this is a setup issue
                        //None of the target pokemon for this program have disable, taunt, etc.
                        NormalBattleMenuWatcher battle_menu2(COLOR_YELLOW);
                        int ret3 = wait_until(
                            console, context,
                            std::chrono::seconds(4),
                            { battle_menu2 }
                        );
                        if (ret3 == 0){
                            console.log("Battle menu detected early. Out of PP/No move in slot, please check your setup.");
                            OperationFailedExceptionWithScreenshot::fire(
                                ErrorReportMode::SEND_ERROR_REPORT,
                                "Battle menu detected early. Out of PP, please check your setup.",
                                console
                            );
                        }
                    }else{
                        //Wild pokemon's turn/wait for catch animation
                        pbf_mash_button(context, BUTTON_B, 7200ms);
                        context.wait_for_all_requests();
                    }
                }

                NormalBattleMenuWatcher battle_menu(COLOR_YELLOW);
                GradientArrowWatcher fainted(COLOR_BLUE, GradientArrowType::RIGHT, FAINTED_PROMPT_ARROW_BOX);
                SwapMenuWatcher swap(COLOR_YELLOW);
                int ret2_run = wait_until(
                    console, context,
                    std::chrono::seconds(60),
                    { battle_menu, fainted }
                );
                switch (ret2_run){
                case 0:
                    console.log("Battle menu detected, continuing.");
                    break;
                case 1:
                    console.log("Detected fainted Pokemon. Switching to next living Pokemon...");
                    pbf_press_button(context, BUTTON_A, 160ms, 400ms);
                    pbf_wait(context, 800ms);
                    context.wait_for_all_requests();
                    if (swap.move_to_slot(console, context, switch_party_slot)){
                        pbf_mash_button(context, BUTTON_A, 3000ms);
                        context.wait_for_all_requests();
                        switch_party_slot++;
                    }
                    break;
                default:
                    console.log("Invalid state ret2_run. Out of moves?");
                    OperationFailedExceptionWithScreenshot::fire(
                        ErrorReportMode::SEND_ERROR_REPORT,
                        "Invalid state ret2_run. Out of moves?",
                        console
                    );
                }

            }
        },
        { advance_dialog, add_to_party, overworld }
        );

    switch (ret2){
    case 0:
        console.log("Advance Dialog detected.");
        press_Bs_to_back_to_overworld(info, console, context);
        break;
    case 1:
        console.log("Prompt dialog detected.");
        press_Bs_to_back_to_overworld(info, console, context);
        break;
    case 2:
        console.log("Overworld detected.");
        break;
    default:
        console.log("Invalid state in run_battle().");
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "Invalid state in run_battle().",
            console
        );
    }
}

//Turn and walk toward the glow of a static Tera Pokemon until it is close, so where the
//glide lands matters less. Returns false if no glow was seen.
bool follow_tera_glow(ConsoleHandle& console, ProControllerContext& context){
    size_t turns = 0;
    size_t misses = 0;
    bool seen = false;
    for (size_t step = 0; step < 20; step++){
        context.wait_for_all_requests();
        VideoSnapshot screen = console.video().snapshot();
        const std::optional<TeraGlow> glow = screen.frame ? find_tera_glow(*screen.frame) : std::nullopt;
        if (!glow){
            console.log("Tera glow: none (look " + std::to_string(step + 1) + ").");
            if (seen){
                //A far glow drops out between frames (console 2026-10-26: lost at
                //x=0.29 with Rabsca still on screen). Keep walking the same way.
                if (++misses > 3){
                    break;
                }
                pbf_move_left_joystick(context, {0, +1}, 700ms, 200ms);
                continue;
            }
            if (turns >= 4){
                break;
            }
            //About a quarter turn.
            pbf_move_right_joystick(context, {+1, 0}, 450ms, 300ms);
            turns++;
            continue;
        }
        seen = true;
        misses = 0;
        const double dx = glow->x - 0.5;
        console.log("Tera glow at x=" + std::to_string(glow->x) + ", orb " + std::to_string(glow->orb_pixels) + ", halo " + std::to_string(glow->halo_pixels) + ".");
        if (dx > 0.06 || dx < -0.06){
            //About 350 ms pans the camera one frame width.
            pbf_move_right_joystick(context, {dx > 0 ? +1.0 : -1.0, 0}, Milliseconds((int64_t)(std::abs(dx) * 350) + 40), 300ms);
            continue;
        }
        //Close enough for the lead: the orb itself (43-67 samples up close; at 42 the
        //first send-out missed, console 2026-10-26). The
        //halo is no measure in daylight, where the golden grass counts as halo too
        //(halo 1019 with Rabsca still far, console 2026-10-26).
        if (glow->orb_pixels >= 55){
            break;
        }
        pbf_move_left_joystick(context, {0, +1}, 700ms, 200ms);
    }
    bbq_lab_event(console, context, seen ? "glow_followed" : "no_glow");
    return seen;
}

bool catch_quest_catches_on_return(BBQuests current_quest, bool battled_this_attempt){
    return current_quest == BBQuests::catch_any && !battled_this_attempt;
}

//From the Coastal Plaza fly point to the middle of the hill west of the plaza, inside
//its cliff ring, where Pikipek, Trumbeak and Toucannon spawn. Calibrated 2026-10-25 from
//the capture card: 4 of 4 flights landed on the hill, and the first look from its middle
//found a Toucannon. Riding off the cliff to the south instead runs into the hill's wall.
void central_to_slowpoke_beach(
    const ProgramInfo& info,
    ConsoleHandle& console, ProControllerContext& context,
    const BBQOption& BBQ_OPTIONS
){
    central_to_coastal_plaza(info, console, context);
    pbf_move_left_joystick(context, {+0.4, +0.92}, 160ms, 400ms);
    pbf_press_button(context, BUTTON_L, 160ms, 400ms);
    pbf_press_button(context, BUTTON_L | BUTTON_PLUS, 160ms, 840ms);
    jump_glide_fly(console, context, BBQ_OPTIONS.INVERTED_FLIGHT, 3000ms, 5000ms, 2400ms);
    //Let the landing finish: a PLUS pressed during it is lost, and the ride stays out
    //(R19: sneak-up threw from the ride 2 of 2 times and never started a battle).
    pbf_wait(context, 1600ms);
}

void coastal_plaza_to_grassland(ConsoleHandle& console, ProControllerContext& context, bool inverted_flight){
    console.log("Coastal Plaza to the hill west of the plaza.");

    //The landing view faces the plaza. The hill is 35 degrees to its left.
    pbf_move_left_joystick(context, {-0.57, +0.82}, 160ms, 400ms);
    pbf_press_button(context, BUTTON_L | BUTTON_PLUS, 160ms, 840ms);
    jump_glide_fly(console, context, inverted_flight, 3000ms, 3000ms, 3200ms);
    context.wait_for_all_requests();
    bbq_lab_event(console, context, "landed");

    //Ride from the landing spot, at the hill's east edge, to its middle (the user's spot,
    //2026-10-25: 115 px west of the fly point on the default map zoom; this lands 110).
    pbf_press_button(context, BUTTON_L, 160ms, 400ms);
    pbf_move_left_joystick(context, {+0.15, +1}, 2000ms, 500ms);
    context.wait_for_all_requests();
}

void fly_back_to_coastal_plaza(const ProgramInfo& info, ConsoleHandle& console, ProControllerContext& context, Language language){
    try{
        open_map_from_overworld(info, console, context);
        if (fly_to_visible_closest_flypoint_cur_zoom_level(info, console, context, FlyPoint::FAST_TRAVEL)){
            return;
        }
        fly_to_closest_pokecenter_on_map(info, console, context, 2);
        return;
    }catch (OperationFailedException&){
        console.log("No Pokecenter icon found near the player. Flying to Coastal Plaza directly.", COLOR_ORANGE);
    }
    press_Bs_to_back_to_overworld(info, console, context);
    stay_away_from_plaza(language);
    central_to_coastal_plaza(info, console, context);
}

//Look for a Flying type again from the closest fly point, without going back to
//Central Plaza: after the first look the player is still near Coastal Plaza.
static void retry_coastal_flying(
    const ProgramInfo& info,
    ConsoleHandle& console, ProControllerContext& context,
    bool inverted_flight, Language language
){
    console.log("Flying back to the closest fly point to look again.");
    fly_back_to_coastal_plaza(info, console, context, language);
    coastal_plaza_to_grassland(console, context, inverted_flight);
    send_out_and_look_around(console, context, std::chrono::seconds(6), "a wild Pokemon");
}

//Read the opponent's species and say whether to flee. Only quests with a species
//table are read; an unreadable name is caught anyway.
static bool is_wrong_catch_target(
    const ProgramInfo& info,
    ConsoleHandle& console, ProControllerContext& context,
    const BBQOption& BBQ_OPTIONS,
    BBQuests current_quest
){
    if (!catch_quest_checks_species(current_quest)){
        return false;
    }
    std::set<std::string> slugs = read_singles_opponent(info, console, context, BBQ_OPTIONS.LANGUAGE);
    std::string names;
    for (const std::string& slug : slugs){
        names += (names.empty() ? "" : ", ") + slug;
    }
    std::optional<bool> counts = catch_quest_counts(current_quest, slugs);
    console.log("Opponent: " + (names.empty() ? std::string("(unread)") : names)
        + (counts == std::optional<bool>(false) ? ". Wrong type for this quest." : "."));
    return counts == std::optional<bool>(false);
}

enum class CatchAttempt{ NO_BATTLE, WRONG_TYPE, BATTLED };

//One look for a catch target: navigate, and fight and catch whatever comes, unless
//its species is read and does not count for the quest.
static CatchAttempt catch_attempt(
    const ProgramInfo& info,
    ConsoleHandle& console, ProControllerContext& context,
    const BBQOption& BBQ_OPTIONS,
    BBQuests current_quest,
    const std::function<void(ProControllerContext&)>& navigate
){
    EncounterWatcher encounter_watcher(console, COLOR_RED);
    bool battle_menu_seen = false;

    //Navigate to target and start battle
    int ret = run_until<ProControllerContext>(
        console, context,
        [&](ProControllerContext& context){
            
            navigate(context);
            context.wait_for_all_requests();
            bbq_lab_event(console, context, "route_done");

            NormalBattleMenuWatcher battle_menu(COLOR_YELLOW);
            int ret2 = wait_until(
                console, context,
                std::chrono::seconds(25),
                { battle_menu }
            );
            if (ret2 == 0){
                //encounter_watcher looks for the same battle menu. If this watcher
                //sees it first, run_until() returns -1 even though a battle started.
                battle_menu_seen = true;
            }else{
                console.log("Did not enter battle. Did target spawn?");
            }
        },
        {
            static_cast<VisualInferenceCallback&>(encounter_watcher),
            static_cast<AudioInferenceCallback&>(encounter_watcher),
        }
    );
    encounter_watcher.throw_if_no_sound();

    if (ret < 0 && !battle_menu_seen){
        bbq_lab_event(console, context, "no_battle");
        return CatchAttempt::NO_BATTLE;
    }
    console.log("Battle menu detected.");
    bbq_lab_event(console, context, "battle");

    if (encounter_watcher.shiny_screenshot()){
        console.log("Shiny detected!");
        pbf_press_button(context, BUTTON_CAPTURE, 2000ms, 5000ms);
        throw ProgramFinishedException();
    }
    if (is_wrong_catch_target(info, console, context, BBQ_OPTIONS, current_quest)){
        bbq_lab_event(console, context, "wrong_type");
        //A Trapinch at the Rabsca spot blocked running 10 times (console 2026-10-26).
        if (escape_wild_battle(info, console, context) == EscapeResult::FAILED){
            OperationFailedExceptionWithScreenshot::fire(
                ErrorReportMode::SEND_ERROR_REPORT,
                "catch_attempt(): could not get out of a wrong-type battle.",
                console
            );
        }
        return CatchAttempt::WRONG_TYPE;
    }
    quest_catch_handle_battle(info, console, context, BBQ_OPTIONS, current_quest);
    return CatchAttempt::BATTLED;
}

size_t catch_quest_looks(BBQuests current_quest, bool coastal_flying_catch){
    const bool coastal =
        (coastal_flying_catch && current_quest == BBQuests::catch_flying)
        || current_quest == BBQuests::catch_poison || current_quest == BBQuests::catch_dark;
    if (coastal){
        return 3;
    }
    //Rabsca stays put while a wrong-type Pokemon that got there first is fled: send the
    //lead out again in place (console 2026-10-26: a Deerling ended 1 of 3 attempts).
    return current_quest == BBQuests::catch_bug ? 2 : 1;
}

void quest_catch(
    const ProgramInfo& info,
    ConsoleHandle& console, ProControllerContext& context,
    const BBQOption& BBQ_OPTIONS,
    BBQuests current_quest
){
    const size_t looks = catch_quest_looks(current_quest, BBQ_OPTIONS.COASTAL_FLYING_CATCH);

    CatchAttempt result = CatchAttempt::NO_BATTLE;
    for (size_t look = 0; look < looks && result != CatchAttempt::BATTLED; look++){
        result = catch_attempt(
            info, console, context, BBQ_OPTIONS, current_quest,
            [&](ProControllerContext& context){
                if (look == 0){
                    quest_catch_navi(info, console, context, BBQ_OPTIONS, current_quest);
                }else if (current_quest == BBQuests::catch_bug){
                    //After fleeing a wrong-type Pokemon, Rabsca is still near.
                    if (follow_tera_glow(console, context)){
                        send_out_ahead(console, context, 3);
                    }else{
                        send_out_and_look_around(console, context, std::chrono::seconds(6), "Rabsca");
                    }
                }else{
                    retry_coastal_flying(info, console, context, BBQ_OPTIONS.INVERTED_FLIGHT, BBQ_OPTIONS.LANGUAGE);
                }
            }
        );
    }
    bool battled = result != CatchAttempt::NO_BATTLE;

    while (true){
        if (!catch_quest_catches_on_return(current_quest, battled)){
            leave_quest_area(info, console, context, BBQ_OPTIONS, current_quest);
            break;
        }
        if (!return_to_plaza_unless_battle(info, console, context, BBQ_OPTIONS.LANGUAGE)){
            break;
        }
        console.log("Battle on the way back to Central Plaza. Any wild Pokemon counts: catching it.");
        quest_catch_handle_battle(info, console, context, BBQ_OPTIONS, current_quest);
        battled = true;
    }

    //Day skip and attempt to respawn fixed encounters
    pbf_press_button(context, BUTTON_HOME, 160ms, GameSettings::instance().GAME_TO_HOME_DELAY1);
    home_to_date_time(console, context, true);
    roll_date_forward_1(console, context, true);
    pbf_press_button(context, BUTTON_HOME, 160ms, ConsoleSettings::instance().SETTINGS_TO_HOME_DELAY0);
    resume_game_from_home(console, context);

    //Nothing to heal without a battle (return_to_plaza() flees with a Smoke Ball).
    //The player is on the Central Plaza fly point, where return_to_plaza() landed, or
    //away from it for a direct flight, so the central_to_* routes need no re-fly either.
    if (!battled){
        console.log("No battle this attempt. Skipping the Pokecenter heal.");
        context.wait_for_all_requests();
        return;
    }

    heal_after_battle_if_due(info, console, context);
    context.wait_for_all_requests();
}

void wild_battle_tera(
    const ProgramInfo& info,
    VideoStream& console, ProControllerContext& context,
    bool& tera_self
){
    AdvanceDialogWatcher lost(COLOR_YELLOW);
    OverworldWatcher overworld(console.logger(), COLOR_RED);
    WallClock start = current_time();
    uint8_t switch_party_slot = 1;
    bool first_turn = true;

    int ret2 = run_until<ProControllerContext>(
        console, context,
        [&](ProControllerContext& context){
            while(true){
                if (current_time() - start > std::chrono::minutes(5)){
                    console.log("Timed out during battle after 5 minutes.", COLOR_RED);
                    OperationFailedExceptionWithScreenshot::fire(
                        ErrorReportMode::SEND_ERROR_REPORT,
                        "Timed out during battle after 5 minutes.",
                        console
                    );
                }

                if (first_turn && tera_self){
                    console.log("Turn 1: Tera.");
                    //Open move menu
                    pbf_press_button(context, BUTTON_A, 80ms, 400ms);
                    pbf_wait(context, 800ms);
                    context.wait_for_all_requests();

                    pbf_press_button(context, BUTTON_R, 160ms, 400ms);
                    pbf_press_button(context, BUTTON_A, 80ms, 400ms);
                    bbq_lab_event(console, context, "tera_turn");

                    first_turn = false;
                }

                NormalBattleMenuWatcher battle_menu(COLOR_MAGENTA);
                GradientArrowWatcher fainted(COLOR_BLUE, GradientArrowType::RIGHT, FAINTED_PROMPT_ARROW_BOX);
                SwapMenuWatcher swap(COLOR_YELLOW);

                context.wait_for_all_requests();

                int ret3 = wait_until(
                    console, context,
                    std::chrono::seconds(90),
                    { battle_menu, fainted }
                );
                switch (ret3){
                case 0:
                    console.log("Detected battle menu. Pressing A to attack...");
                    //After failed runs the cursor stays on Run, and A only tries to run
                    //again (a Trapinch, console 2026-10-26: 5 minutes of A on Run).
                    battle_menu.move_to_slot(console, context, 0);
                    pbf_mash_button(context, BUTTON_A, 3000ms);
                    context.wait_for_all_requests();
                    break;
                case 1:
                    console.log("Detected fainted Pokemon. Switching to next living Pokemon...");
                    pbf_press_button(context, BUTTON_A, 160ms, 400ms);
                    pbf_wait(context, 800ms);
                    context.wait_for_all_requests();
                    if (swap.move_to_slot(console, context, switch_party_slot)){
                        pbf_mash_button(context, BUTTON_A, 3000ms);
                        context.wait_for_all_requests();
                        switch_party_slot++;
                    }
                    break;
                default:
                    OperationFailedExceptionWithScreenshot::fire(
                        ErrorReportMode::SEND_ERROR_REPORT,
                        "Timed out during battle. Stuck, crashed, or took more than 90 seconds for a turn.",
                        console
                    );
                }
            }
        },
        { lost, overworld }
    );
    if (ret2 == 0){
        console.log("Lost battle. Mashing B.");
    }
}

}
}
}
