/*  BDSP AutoStory - Segment 00.04: Route 202
 *  Dawn's tour of Sandgem, Mom's parcel at home, and the catch tutorial on
 *  Route 202, saved after the catch. Then Potions, a few training battles,
 *  and north up Route 202 past its trainers to Jubilife City.
 *
 *  Start: in front of Rowan, after receiving the Pokedex.
 *  End:   in Jubilife City, after Dawn's scene.
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>

#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"

#include "../Data/PokemonBDSP_AutoStory_Route201.h"
#include "../Data/PokemonBDSP_AutoStory_Route202.h"
#include "../Data/PokemonBDSP_AutoStory_StoryMaps.h"
#include "../Utils/PokemonBDSP_AutoStory_GameSettings.h"
#include "../Utils/PokemonBDSP_AutoStory_RouteWalk.h"
#include "../Utils/PokemonBDSP_AutoStory_StoryBattle.h"
#include "../Utils/PokemonBDSP_AutoStory_Walk.h"
#include "../Utils/PokemonBDSP_AutoStory_WildEncounters.h"

#include "PokemonBDSP_AutoStory_Segment_00.h"
#include "PokemonBDSP_AutoStory_Segment_00_Shared.h"

using namespace std::chrono_literals;
using PokemonAutomation::Pokemon::MapSet;
using PokemonAutomation::Pokemon::TilePos;
using PokemonAutomation::Pokemon::TrainerSight;

namespace PokemonAutomation {
    namespace NintendoSwitch {
        namespace PokemonBDSP {

        // ---------------------------------------------------------------------------
        // Begin Segment 00 - 04
        // Will heal at Sandgem pokeCenter to reliably catch a second pokemon for Home
        // transfer.
        // Collects poketech and stands in front of the PC to end the tutorial section
        // and send the starter to pokemon HOME.
        // ---------------------------------------------------------------------------

        // Inside the player's house, just through the door: Mom watches TV a
        // column east of the door and two rows north of the mat. The door fixes
        // our column, but counted taps north lost a row (r108b stood a row below
        // her). So find her row: face east and press A, and step one tile north
        // at a time until she answers. A press from a row off talks to nobody,
        // and the TV stand stops us a row above her.
        static void talk_to_mom_at_home(VideoStream& stream, ProControllerContext& context) {
            for (size_t row = 0; row < 4; row++) {
                if (talk_to_adjacent_npc(stream, context, {Pokemon::Direction::EAST}, "Home: Mom")) {
                    stream.log("[00.04] Mom answered after " + std::to_string(row) + " step(s) north.");
                    return;
                }
                // Facing east, a short tap north only turns; facing north, it is one tile.
                route_turn(stream, context, Pokemon::Direction::NORTH);
                pbf_press_dpad(context, DPAD_UP, 80ms, 400ms);
                context.wait_for_all_requests();
            }
            OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                "Home: Mom did not answer from any of four rows north of the door.", stream);
        }

        static void tutorial_part_3_navigation(SingleSwitchProgramEnvironment& env, VideoStream& stream, ProControllerContext& context) {
            // Arm exit detection before movement and finish the fade before
            // starting the dialogue watcher, which cancels queued commands.
            recorded_move_until_transition(stream, context, 0.0, -1.0, 2010ms,
                "Rowan lab exit");
            context.wait_for_all_requests();

            wait_for_dialogue(stream, context, "Phase 14 (dawn tour)");

            pbf_mash_button(context, BUTTON_B, 25000ms);

            pbf_move_left_joystick(context, { -1.000000, -0.000000 }, 5125ms, 0ms);
            pbf_wait(context, 336ms);
            pbf_move_left_joystick(context, { 0.000000, -1.000000 }, 1700ms, 0ms);
            pbf_wait(context, 213ms);
            pbf_move_left_joystick(context, { -1.000000, -0.000000 }, 2325ms, 0ms);
            pbf_move_left_joystick(context, { -1.000000, 1.000000 }, 950ms, 0ms);
            pbf_move_left_joystick(context, { -1.000000, -0.000000 }, 2360ms, 0ms);
            pbf_wait(context, 253ms);
            pbf_move_left_joystick(context, { 0.000000, -1.000000 }, 4610ms, 0ms);
            pbf_wait(context, 954ms);
            pbf_move_left_joystick(context, { -1.000000, -0.000000 }, 220ms, 0ms);
            pbf_wait(context, 587ms);
            pbf_move_left_joystick(context, { 0.000000, -1.000000 }, 1375ms, 0ms);
            pbf_wait(context, 464ms);
            pbf_move_left_joystick(context, { 1.000000, -0.000000 }, 775ms, 0ms);
            pbf_wait(context, 1655ms);
            pbf_move_left_joystick(context, { 0.000000, 1.000000 }, 640ms, 0ms);
            pbf_wait(context, 3128ms);
            talk_to_mom_at_home(stream, context);

            wait_for_dialogue(stream, context, "Phase 15 (Mom conversation to accept parcel)");

            pbf_mash_button(context, BUTTON_B, 40000ms);

            context.wait_for_all_requests();
            pbf_wait(context, 1500ms);
            pbf_press_dpad(context, DPAD_LEFT, 150ms, 0ms);
            pbf_wait(context, 1143ms);
            pbf_move_left_joystick(context, { 0.000000, -1.000000 }, 875ms, 0ms);
            pbf_wait(context, 1169ms);
            pbf_press_button(context, BUTTON_A, 180ms, 0ms);
            pbf_wait(context, 946ms);
            pbf_press_button(context, BUTTON_A, 180ms, 0ms);
            pbf_wait(context, 9092ms);
            pbf_press_button(context, BUTTON_A, 180ms, 0ms);
            pbf_wait(context, 740ms);
            pbf_press_button(context, BUTTON_A, 180ms, 0ms);
            pbf_wait(context, 2998ms);
            pbf_move_left_joystick(context, { -1.000000, -0.000000 }, 240ms, 0ms);
            pbf_wait(context, 893ms);
            pbf_move_left_joystick(context, { 0.000000, 1.000000 }, 5300ms, 0ms);
            pbf_wait(context, 282ms);
            pbf_move_left_joystick(context, { 1.000000, -0.000000 }, 260ms, 0ms);
            context.wait_for_all_requests();
            pbf_wait(context, 1500ms);

            DateTime t{ 2026, 1, 1, 0, 0, 50 };
            set_time_fast(env, context, t);
            walk_through_grass(stream, context, 1, 0, 20, "right through grass to path", BattleType::STARTER);
            walk_through_grass(stream, context, 0, 1, 18, "up through grass 2", BattleType::STARTER);
            walk_through_grass(stream, context, 1, 0, 10, "right through grass 2", BattleType::STARTER);
            context.wait_for_all_requests();
            pbf_wait(context, 1500ms);

            pbf_move_left_joystick(context, { 0.000000, -1.000000 }, 560ms, 0ms);
            pbf_wait(context, 1500ms);
            pbf_move_left_joystick(context, { 1.000000, -0.000000 }, 5000ms, 0ms);
            pbf_wait(context, 1500ms);
            pbf_move_left_joystick(context, { -1.000000, -0.000000 }, 1000ms, 0ms);
            pbf_wait(context, 1500ms);
            pbf_move_left_joystick(context, { 0.000000, 1.000000 }, 1855ms, 0ms);
            pbf_wait(context, 1500ms);
            pbf_move_left_joystick(context, { 1.000000, -0.000000 }, 430ms, 0ms);
            pbf_wait(context, 1500ms);
            pbf_move_left_joystick(context, { 0.000000, 1.000000 }, 850ms, 0ms);
            pbf_wait(context, 1500ms);
            pbf_move_left_joystick(context, { -1.000000, -0.000000 }, 1120ms, 0ms);

            wait_for_dialogue(stream, context, "Phase 16 (catch tutorial)");

            pbf_mash_button(context, BUTTON_B, 70000ms);
            context.wait_for_all_requests();
            pbf_wait(context, 2000ms);
            catch_1_pokemon(stream, context);
            pbf_wait(context, 1500ms);
            context.wait_for_all_requests();
        }

        // Resumes from the save made after catching the second Pokemon.
        static void tutorial_part_3_after_catch_navigation(SingleSwitchProgramEnvironment& env, VideoStream& stream, ProControllerContext& context) {
            // A retry reloads the save at whatever time the clock has reached.
            // Pin it again so the landmark references see the same lighting.
            set_time_fast(env, context, DateTime{ 2026, 1, 1, 0, 0, 50 });

            // What this part of 00.04 does, logged up front and as each step starts.
            // The Potion on Route 201 is optional, and the starter outclasses the
            // Route 202 trainers, so we go straight north.
            static const char* const STEPS[] = {
                "Get 10 Potions from the Poke Mart lady on Route 201, walk back to Route 202 and save (skipped if the Bag has Potions)",
                "Train: fight 3 wild Pokemon in the catch grass, with the best move",
                "Heal to full with Potions from the Bag",
                "Walk north up Route 202 to the Jubilife exit, fighting trainers who see us; after each, use a Potion if HP is under half",
                "Enter Jubilife and get through Dawn's scene",
            };
            constexpr size_t STEP_COUNT = sizeof(STEPS) / sizeof(STEPS[0]);
            {
                std::string text = "[00.04] After the catch, the steps are:";
                for (size_t i = 0; i < STEP_COUNT; i++) {
                    text += "\n    " + std::to_string(i + 1) + ". " + STEPS[i];
                }
                stream.log(text, COLOR_BLUE);
            }
            auto step = [&](size_t number) {
                std::string text = "[00.04 step " + std::to_string(number) + "/" + std::to_string(STEP_COUNT) + "] " + STEPS[number - 1];
                text += number < STEP_COUNT ? std::string(" -- next: ") + STEPS[number] : std::string(" -- last step");
                stream.log(text, COLOR_BLUE);
                add_wrapped_log(stream.overlay(), "00.04 step " + std::to_string(number) + "/" + std::to_string(STEP_COUNT) + ": " + STEPS[number - 1], COLOR_WHITE);
                if (number < STEP_COUNT) {
                    add_wrapped_log(stream.overlay(), "Next: " + std::string(STEPS[number]), COLOR_GRAY);
                }
            };

            const MapSet& maps = story_maps();
            std::vector<TrainerSight> trainers = story_trainer_sights();
            // Fight a trainer whose dialogue has opened, with whatever starter we
            // have. Someone who only talks leaves no battle to fight.
            auto fight = [&](const std::string& name) {
                pbf_mash_button(context, BUTTON_B, 5000ms);
                context.wait_for_all_requests();
                BattleMenuWatcher battle_menu(BattleType::TRAINER, COLOR_YELLOW);
                BattleDialogWatcher battle_dialog(COLOR_YELLOW);
                if (wait_until(stream, context, 8s, {{battle_menu}, {battle_dialog}}) < 0) {
                    stream.log("[00.04] " + name + ": no battle followed the talk.");
                    pbf_mash_button(context, BUTTON_B, 2000ms);
                    context.wait_for_all_requests();
                    return;
                }
                fight_trainer(stream, context, name);
                context.wait_for_all_requests();
                // The next trainer may be a few tiles on: check HP now.
                use_potion_first_pokemon(stream, context, 0.5);
            };

            pbf_wait(context, 1500ms);
            step(1);
            // Potions let us heal where we stand, instead of walking back to the
            // Pokemon Center. A save made after getting them starts at the save
            // spot with Potions in the Bag; the post-catch save starts at the
            // catch spot without.
            const TilePos save_spot = maps.to_world(MAP_ROUTE_202, Route202::POTION_SAVE_SPOT);
            if (bag_has_item(stream, context, "potion", 1)) {
                stream.log("[00.04] The Bag has Potions; not visiting the Poke Mart lady.");
            } else {
                const TilePos catch_spot = maps.to_world(MAP_ROUTE_202, Route202::CATCH_SPOT);
                // She moves about near the west end of her row (r113a, r114a),
                // so go to the corner that pins the row, (42, 3): trees to its
                // west and north. Then walk east until she blocks the walk and
                // answers, as done by hand.
                const TilePos corner = maps.to_world(MAP_ROUTE_201, Route201::POTION_GIVER_ROW_WEST_END);
                const TilePos row_east_end = maps.to_world(MAP_ROUTE_201, Route201::POTION_GIVER_ROW_EAST_END);
                walk_route(stream, context, maps, {catch_spot},
                    [&](const TilePos& pos) { return pos == corner; }, trainers, fight, "To the Poke Mart lady");
                if (!walk_along_row_to_talk(stream, context, Pokemon::Direction::EAST,
                        (size_t)(row_east_end.x - corner.x), "Poke Mart lady")) {
                    OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                        "Poke Mart lady: nobody answered walking east along her row from (42, 3), "
                        "nor on any side of where the walk stopped.", stream);
                }
                // Her talk gives 10 Potions ("obtained" lines): A to the end.
                finish_dialogue(stream, context, BUTTON_A, "Poke Mart lady");
                if (!bag_has_item(stream, context, "potion", 1)) {
                    // She gives the Potions once: on a save that already got
                    // (and used) them, she only talks.
                    OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                        "Poke Mart lady: she talked, but no Potion is in the Bag. "
                        "Has this save already had her Potions?", stream);
                }
                // Somewhere along her row: plan from all of it.
                walk_route(stream, context, maps, story_potion_giver_area(),
                    [&](const TilePos& pos) { return pos == save_spot; }, trainers, fight, "Back to Route 202");
                stream.log("[00.04] Saving with the Potions, at the save spot on Route 202.");
                save_game(env, context);
            }
            walk_route(stream, context, maps, {save_spot},
                [&](const TilePos& pos) {
                    const std::vector<TilePos> grass = story_after_training_area();
                    return std::find(grass.begin(), grass.end(), pos) != grass.end();
                }, trainers, fight, "To the catch grass");

            step(2);
            // The starter starts around level 5 with only a Normal move; the
            // Route 202 trainers need a level or two more.
            for (size_t battle = 0; battle < 3; battle++) {
                route_status(stream, "Training battle " + std::to_string(battle + 1) + "/3");
                battle_1_pokemon(stream, context);
            }

            step(3);
            use_potion_first_pokemon(stream, context);

            step(4);
            walk_route(stream, context, maps, story_after_training_area(),
                story_at_jubilife_exit, trainers, fight, "To Jubilife");

            step(5);
            // Jubilife's south edge is a wall with stairs in it, west of where the
            // route may leave us (r78 stood at its east end). Entering starts a
            // scene: Dawn walks over while we stand still. So walk north; if the
            // wall stops us and no dialogue follows, the stairs are further west.
            bool met_dawn = false;
            for (size_t attempt = 0; attempt < 6 && !met_dawn; attempt++) {
                const WalkResult walk = walk_until(stream, context, 0, 1, 4000ms, "to Dawn in Jubilife");
                if (walk.stop == WalkStop::TRANSITION) {
                    OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                        "Jubilife: left the area instead of meeting Dawn.", stream);
                }
                ShortDialogWatcher dialog(COLOR_CYAN);
                if (walk.stop == WalkStop::DIALOG || wait_until(stream, context, 6s, {{dialog}}) == 0) {
                    met_dawn = true;
                    break;
                }
                stream.log("[00.04] Jubilife: the wall stopped us; looking for the stairs one tile west.");
                route_turn(stream, context, Pokemon::Direction::WEST);
                pbf_move_left_joystick(context, {-1, 0}, route_open_hold(Pokemon::Direction::WEST, 1), 200ms);
                context.wait_for_all_requests();
            }
            if (!met_dawn) {
                OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                    "Jubilife: no stairs found and no Dawn after six tiles west.", stream);
            }
            wait_for_dialogue(stream, context, "Phase 21 (dawn Jubilife dialog)");

            pbf_mash_button(context, BUTTON_B, 5000ms);
        }
        // ---------------------------------------------------------------------------
        // End of Segment 00 - 04
        // Standing in Jubilife after speaking to dawn.
        // ---------------------------------------------------------------------------
        static void checkpoint_0004(
            SingleSwitchProgramEnvironment& env,
            ProControllerContext& context,
            AutoStoryOptions options,
            AutoStoryStats& stats
        ) {
            checkpoint_reattempt_loop(env, context, options, stats,
                [&](size_t /*attempt*/) {
                    // Segment 00-003: Poketech acquisition
                    tutorial_part_3_navigation(env, env.console, context);
                },
                true
            );

            // Save and advance progress at the catch boundary. Retries of this
            // section must start from the caught-Pokemon save, not the old route.
            checkpoint_reattempt_loop(env, context, options, stats,
                [&](size_t /*attempt*/) {
                    tutorial_part_3_after_catch_navigation(env, env.console, context);
                },
                true
            );
        }
        // DIAGNOSTIC ONLY: Remove before pushing to production.
        void diagnostic_checkpoint_0004_after_catch(
            SingleSwitchProgramEnvironment& env, ProControllerContext& context,
            AutoStoryOptions options, AutoStoryStats& stats
        ) {
            checkpoint_reattempt_loop(env, context, options, stats,
                [&](size_t /*attempt*/) {
                    tutorial_part_3_after_catch_navigation(env, env.console, context);
                },
                true
            );
        }

        // ========================
        // Checkpoint 0004 Wrapper
        // ========================

        std::string AutoStory_Checkpoint_0004::name()       const { return "00.04 - After Pokedex"; }
        std::string AutoStory_Checkpoint_0004::start_text() const { return AutoStory_Checkpoint_0003().end_text(); }
        std::string AutoStory_Checkpoint_0004::end_text()   const { return "Standing in in jubilife after speaking to dawn. Next: give town map to barry"; }

        void AutoStory_Checkpoint_0004::run_checkpoint(
            SingleSwitchProgramEnvironment& env,
            ProControllerContext& context,
            AutoStoryOptions options,
            AutoStoryStats& stats
        ) const {
            checkpoint_0004(env, context, options, stats);
        }

        }
    }
}
