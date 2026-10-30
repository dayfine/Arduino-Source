/*  BDSP AutoStory - Segment 00.05: Jubilife City
 *  Barry's Town Map at the Trainers' School, the three clowns' coupons and
 *  the Poketch from the president, then into the Pokemon Center.
 *
 *  Start: in Jubilife City, after Dawn's scene.
 *  End:   in front of the Pokemon Center PC, with the Poketch.
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cmath>

#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"

#include "../Data/PokemonBDSP_AutoStory_JubilifeCity.h"
#include "../Data/PokemonBDSP_AutoStory_StoryMaps.h"
#include "../Utils/PokemonBDSP_AutoStory_RouteWalk.h"
#include "../Utils/PokemonBDSP_AutoStory_StoryBattle.h"
#include "../Utils/PokemonBDSP_AutoStory_Walk.h"
#include "../Utils/PokemonBDSP_OverworldMatchers.h"

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
        // Begin Segment 00 - 05
        // Standing at the entrance to jubilife after speaking to dawn, will navigate to the poketech challenge.
        // Collects poketech and stands in front of the PC to end the tutorial section
        // and send the starter to pokemon HOME.
        // ---------------------------------------------------------------------------

            // Stop the approach as soon as buildings can guide it, instead of
            // overshooting south and then blindly walking back north.
            static void approach_poketch_landmarks(VideoStream& stream, ProControllerContext& context) {
                context.wait_for_all_requests();
                Milliseconds walked = 0ms;
                while (walked < 1900ms) {
                    auto frame = stream.video().snapshot();
                    if (!frame) OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                        "Poketch approach: No video frame.", stream);
                    const auto result = match_poketch_landmarks(*frame.frame);
                    VideoOverlaySet overlays(stream.overlay());
                    for (const auto& landmark : result.landmarks) {
                        overlays.add(COLOR_CYAN, landmark.search);
                        if (landmark.accepted) overlays.add(COLOR_GREEN, landmark.found);
                        stream.log("[Poketch approach] " + landmark.name + " score=" + std::to_string(landmark.score) +
                            " margin=" + std::to_string(landmark.margin));
                    }
                    if (result.located) {
                        stream.log("[Poketch approach] Landmarks acquired after " + std::to_string(walked.count()) + "ms; ending downward approach.");
                        return;
                    }
                    const Milliseconds step = std::min(200ms, 1900ms - walked);
                    pbf_move_left_joystick(context, {0, -1}, step, 0ms);
                    walked += step;
                    pbf_wait(context, 700ms);
                    context.wait_for_all_requests();
                }
                stream.log("[Poketch approach] Reached 1900ms downward limit; handing off to locator.");
            }

            static void align_and_talk_to_poketch_man(VideoStream& stream, ProControllerContext& context) {
                context.wait_for_all_requests();
                pbf_wait(context, 700ms);
                context.wait_for_all_requests();
                size_t confirmed = 0, coarse_confirmed = 0;
                double previous_dx = 0, previous_dy = 0;
                size_t acquisition_misses = 0, vertical_probe = 0;
                // Two small excursions around the approach position. This is
                // visibility recovery, not permission to interact without a match.
                const int vertical_probes[] = {-80, 160, -240, 320, -160, -120, 240, -360, 480, -240};
                for (size_t attempt = 0; attempt < 80; attempt++) {
                    auto frame = stream.video().snapshot();
                    if (!frame) OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                        "Poketch interaction: No video frame.", stream);
                    const OverworldMatchResult result = match_poketch_landmarks(*frame.frame);
                    stream.log(result.coarse ? "[Poketch landmarks] Coarse acquisition." : "[Poketch landmarks] Fine alignment.");
                    VideoOverlaySet overlays(stream.overlay());
                    for (const auto& landmark : result.landmarks) {
                        overlays.add(COLOR_CYAN, landmark.search);
                        if (landmark.accepted) overlays.add(COLOR_GREEN, landmark.found);
                        stream.log("[Poketch landmarks] " + landmark.name + " score=" + std::to_string(landmark.score) +
                            " margin=" + std::to_string(landmark.margin) + " accepted=" + std::to_string(landmark.accepted) +
                            " screen-relative offset=(" + std::to_string(landmark.dx) + "," + std::to_string(landmark.dy) + ")");
                    }
                    if (result.located) acquisition_misses = 0;
                    if (result.coarse && result.located) {
                        if (coarse_confirmed && std::abs(result.dx - previous_dx) <= 0.00625 && std::abs(result.dy - previous_dy) <= 1.0/90)
                            coarse_confirmed++;
                        else coarse_confirmed = 1;
                        previous_dx = result.dx; previous_dy = result.dy;
                    } else coarse_confirmed = 0;
                    if (!result.located) {
                        confirmed = 0;
                        if (++acquisition_misses >= 4 && vertical_probe < sizeof(vertical_probes) / sizeof(vertical_probes[0])) {
                            int probe = vertical_probes[vertical_probe++];
                            acquisition_misses = 0;
                            stream.log("[Poketch landmarks] Bounded vertical visibility probe " + std::to_string(vertical_probe) +
                                "/10: " + (probe < 0 ? "down " : "up ") + std::to_string(std::abs(probe)) + "ms.");
                            pbf_move_left_joystick(context, {0, probe < 0 ? -1.0 : 1.0}, Milliseconds(std::abs(probe)), 0ms);
                            pbf_wait(context, 700ms);
                        } else {
                            stream.log("[Poketch landmarks] No trustworthy acquisition; waiting for another frame.");
                            pbf_wait(context, 250ms);
                        }
                    } else if (result.coarse && coarse_confirmed < 3) {
                        confirmed = 0;
                        stream.log("[Poketch landmarks] Waiting for consistent coarse offset before movement.");
                        pbf_wait(context, 250ms);
                    } else if (result.at_target) {
                        if (++confirmed >= 3) {
                            stream.log("[Poketch landmarks] Above-NPC reference confirmed; turning down.");
                            pbf_press_dpad(context, DPAD_DOWN, 40ms, 400ms);
                            context.wait_for_all_requests();
                            pbf_press_button(context, BUTTON_A, 80ms, 500ms);
                            context.wait_for_all_requests();
                            wait_for_dialogue(stream, context, "Phase 27 (poketech challenge completed dialog)", 10s);
                            return;
                        }
                        pbf_wait(context, 300ms);
                    } else {
                        confirmed = 0;
                        const double dx = result.dx, dy = result.dy;
                        stream.log("[Poketch landmarks] Reference screen-relative offset=(" + std::to_string(result.dx) + "," + std::to_string(result.dy) + ").");
                        const bool horizontal = std::abs(dx) > 7.0/960 &&
                            (std::abs(dy) <= 7.0/540 || std::abs(dx) >= std::abs(dy)*9.0/16);
                        double error = horizontal ? dx : dy*9.0/16;
                        auto duration = std::abs(error) > 0.03125 ? 80ms : std::abs(error) > 0.015625 ? 40ms : 16ms;
                        if (std::abs(dx) <= 7.0/960 && std::abs(dy) <= 7.0/540) {
                            stream.log("[Poketch landmarks] Coarse position near target; waiting for fine confirmation.");
                            pbf_wait(context, 300ms);
                            context.wait_for_all_requests();
                            continue;
                        }
                        stream.log("[Poketch landmarks] Correcting " + std::string(horizontal ? "horizontal" : "vertical") +
                            " axis for " + std::to_string(duration.count()) + "ms.");
                        pbf_move_left_joystick(context,
                            {horizontal ? (dx > 0 ? 1.0 : -1.0) : 0.0,
                             horizontal ? 0.0 : (dy < 0 ? 1.0 : -1.0)}, duration, 0ms);
                        pbf_wait(context, 700ms);
                    }
                    context.wait_for_all_requests();
                }
                OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                    "Poketch interaction: Building landmarks did not confirm the reference position; A not pressed.", stream);
            }

            void align_and_enter_jubilife_center(VideoStream& stream, ProControllerContext& context) {
                context.wait_for_all_requests();
                pbf_wait(context, 700ms);
                context.wait_for_all_requests();
                size_t confirmed = 0;
                for (size_t attempt = 0; attempt < 60; attempt++) {
                    const auto frame = stream.video().snapshot();
                    if (!frame) OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                        "Jubilife Center alignment: No video frame.", stream);
                    const auto result = match_jubilife_center_entrance(*frame.frame);
                    VideoOverlaySet overlays(stream.overlay());
                    for (const auto& landmark : result.landmarks) {
                        overlays.add(COLOR_CYAN, landmark.search);
                        if (landmark.accepted) overlays.add(COLOR_GREEN, landmark.found);
                        stream.log("[Jubilife Center alignment] score=" + std::to_string(landmark.score) +
                            " margin=" + std::to_string(landmark.margin) + " screen-relative offset=(" +
                            std::to_string(landmark.dx) + "," + std::to_string(landmark.dy) + ")");
                    }
                    if (!result.located) {
                        confirmed = 0;
                        pbf_wait(context, 250ms);
                    } else if (result.at_target) {
                        if (++confirmed >= 3) {
                            stream.log("[Jubilife Center alignment] Door position confirmed; entering.");
                            recorded_move_until_transition(stream, context, 0, 1, 2000ms, "Jubilife Pokemon Center entry");
                            pbf_wait(context, 700ms);
                            context.wait_for_all_requests();
                            return;
                        }
                        pbf_wait(context, 300ms);
                    } else {
                        confirmed = 0;
                        bool horizontal = std::abs(result.dx) >= std::abs(result.dy)*9.0/16;
                        double error = horizontal ? result.dx : result.dy*9.0/16;
                        const auto duration = std::abs(error) > 1.0/24 ? 80ms : std::abs(error) > 0.015625 ? 40ms : 16ms;
                        pbf_move_left_joystick(context,
                            {horizontal ? (result.dx > 0 ? 1.0 : -1.0) : 0.0,
                             horizontal ? 0.0 : (result.dy < 0 ? 1.0 : -1.0)}, duration, 0ms);
                        pbf_wait(context, 700ms);
                    }
                    context.wait_for_all_requests();
                }
                OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                    "Jubilife Center alignment: Door reference not confirmed; entry not attempted.", stream);
            }

        static void tutorial_part_4_navigation(SingleSwitchProgramEnvironment& env, VideoStream& stream, ProControllerContext& context) {
            // What 00.05 does, stepped through on the console before writing it.
            // Every walk is planned on Jubilife's tile grid; every talk stands
            // beside the person, faces them and presses A once.
            static const char* const STEPS[] = {
                "Trainers' School: talk to Barry, who gives the Town Map",
                "Talk to the Poketch Company president on the main road",
                "Get the three coupons from the clowns by the Poke Mart, the TV station and the Poketch Company",
                "Show the coupons to the president for the Poketch",
                "Walk into the Pokemon Center and stand at the PC",
            };
            constexpr size_t STEP_COUNT = sizeof(STEPS) / sizeof(STEPS[0]);
            {
                std::string text = "[00.05] In Jubilife, the steps are:";
                for (size_t i = 0; i < STEP_COUNT; i++) {
                    text += "\n    " + std::to_string(i + 1) + ". " + STEPS[i];
                }
                stream.log(text, COLOR_BLUE);
            }
            auto step = [&](size_t number) {
                std::string text = "[00.05 step " + std::to_string(number) + "/" + std::to_string(STEP_COUNT) + "] " + STEPS[number - 1];
                text += number < STEP_COUNT ? std::string(" -- next: ") + STEPS[number] : std::string(" -- last step");
                stream.log(text, COLOR_BLUE);
                add_wrapped_log(stream.overlay(), "00.05 step " + std::to_string(number) + "/" + std::to_string(STEP_COUNT) + ": " + STEPS[number - 1], COLOR_WHITE);
                if (number < STEP_COUNT) {
                    add_wrapped_log(stream.overlay(), "Next: " + std::string(STEPS[number]), COLOR_GRAY);
                }
            };

            const MapSet& maps = jubilife_maps();
            auto world = [&](const TilePos& pos) { return maps.to_world(MAP_JUBILIFE_CITY, pos); };
            // Jubilife has no trainers; someone in the way only talks.
            std::vector<TrainerSight> trainers;
            auto in_the_way = [&](const std::string&) { finish_dialogue(stream, context, 0, "Someone in the way"); };
            std::vector<TilePos> at;
            for (const TilePos& pos : JubilifeCity::after_dawn()) {
                at.push_back(world(pos));
            }

            step(1);
            const TilePos school = world(JubilifeCity::TRAINERS_SCHOOL_DOOR.next(Pokemon::Direction::SOUTH));
            // Doors sit in open plaza, which no wall pins down: end anywhere
            // within a tile of the door's column, and let the door search
            // step sideways.
            auto by_door = [](const TilePos& front) {
                return [front](const TilePos& pos) { return pos.y == front.y && pos.x >= front.x - 1 && pos.x <= front.x + 1; };
            };
            at = walk_route(stream, context, maps, at, by_door(school),
                trainers, in_the_way, "To the Trainers' School");
            enter_door_north(stream, context, "Trainers' School door");
            // Barry stands at the blackboard, a tile west of the door's column:
            // step west, walk north until he stops us, talk.
            route_turn(stream, context, Pokemon::Direction::WEST);
            pbf_move_left_joystick(context, {-1, 0}, route_open_hold(Pokemon::Direction::WEST, 1), 200ms);
            walk_until(stream, context, 0, 1, 3000ms, "to Barry");
            pbf_press_button(context, BUTTON_A, 80ms, 200ms);
            context.wait_for_all_requests();
            {
                ShortDialogWatcher dialog(COLOR_CYAN);
                if (wait_until(stream, context, 2000ms, {{dialog}}) != 0) {
                    OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                        "Trainers' School: Barry did not answer.", stream);
                }
            }
            // His scene ends with "You obtained a Town Map!", which A closes.
            finish_dialogue(stream, context, 16, "Barry");
            // Out: south to the front wall, a tile east (the door's column), out.
            walk_until(stream, context, 0, -1, 4000ms, "to the school's front wall");
            route_turn(stream, context, Pokemon::Direction::EAST);
            pbf_move_left_joystick(context, {1, 0}, route_open_hold(Pokemon::Direction::EAST, 1), 200ms);
            if (walk_until(stream, context, 0, -1, 2500ms, "out of the Trainers' School").stop != WalkStop::TRANSITION) {
                OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                    "Trainers' School: did not leave by the door.", stream);
            }
            context.wait_for(1500ms);
            at = {school.next(Pokemon::Direction::WEST), school, school.next(Pokemon::Direction::EAST)};

            step(2);
            // The president talks on with A after each last line; B moves his
            // lines on and never starts a new talk.
            const TilePos president = world(JubilifeCity::POKETCH_PRESIDENT);
            talk_to_npc(stream, context, maps, at, president,
                {Pokemon::Direction::EAST, Pokemon::Direction::SOUTH}, trainers, in_the_way, "Poketch president");
            finish_dialogue(stream, context, 0, "Poketch president");

            step(3);
            // Each clown asks a question whose default answer, Yes, is right,
            // and hands over a coupon with an "obtained" message A closes.
            talk_to_npc(stream, context, maps, at, world(JubilifeCity::CLOWN_MART),
                {Pokemon::Direction::SOUTH, Pokemon::Direction::WEST, Pokemon::Direction::EAST},
                trainers, in_the_way, "Clown by the Poke Mart");
            finish_dialogue(stream, context, 8, "Clown by the Poke Mart");
            talk_to_npc(stream, context, maps, at, world(JubilifeCity::CLOWN_TV_STATION),
                {Pokemon::Direction::EAST, Pokemon::Direction::SOUTH, Pokemon::Direction::WEST},
                trainers, in_the_way, "Clown by the TV station");
            finish_dialogue(stream, context, 8, "Clown by the TV station");
            talk_to_npc(stream, context, maps, at, world(JubilifeCity::CLOWN_POKETCH_COMPANY),
                {Pokemon::Direction::EAST, Pokemon::Direction::SOUTH, Pokemon::Direction::WEST},
                trainers, in_the_way, "Clown by the Poketch Company");
            finish_dialogue(stream, context, 8, "Clown by the Poketch Company");

            step(4);
            talk_to_npc(stream, context, maps, at, president,
                {Pokemon::Direction::EAST, Pokemon::Direction::SOUTH}, trainers, in_the_way, "Poketch president");
            finish_dialogue(stream, context, 0, "Poketch president");

            step(5);
            const TilePos center = world(JubilifeCity::POKEMON_CENTER_DOOR.next(Pokemon::Direction::SOUTH));
            at = walk_route(stream, context, maps, at, by_door(center),
                trainers, in_the_way, "To the Pokemon Center");
            enter_door_north(stream, context, "Pokemon Center door");
            // Inside: north to the counter, three tiles east, north to the PC
            // at the back wall right of the counter.
            walk_until(stream, context, 0, 1, 2500ms, "to the counter");
            route_turn(stream, context, Pokemon::Direction::EAST);
            pbf_move_left_joystick(context, {1, 0}, route_open_hold(Pokemon::Direction::EAST, 3), 200ms);
            walk_until(stream, context, 0, 1, 1500ms, "to the PC");
            context.wait_for_all_requests();
        }
        // ---------------------------------------------------------------------------
        // Segment 00-05 ends
        // Standing in front of the Jubilife City Pokemon Center PC.
        // Player will need to deposit pokemon in the pc and transfer to Pokemon Home
        // and level to 100 before starting the next segment.
        // ---------------------------------------------------------------------------

        static void checkpoint_0005(
            SingleSwitchProgramEnvironment& env,
            ProControllerContext& context,
            AutoStoryOptions options,
            AutoStoryStats& stats
        ) {
            checkpoint_reattempt_loop(env, context, options, stats,
                [&](size_t /*attempt*/) {
                    // Segment 00-004: Poketech acquisition
                    tutorial_part_4_navigation(env, env.console, context);
                },
                true
            );
        }

        // ========================
        // Checkpoint 0005 Wrapper
        // ========================

        std::string AutoStory_Checkpoint_0005::name()       const { return "00.05 - Jubilife After Dawn"; }
        std::string AutoStory_Checkpoint_0005::start_text() const { return AutoStory_Checkpoint_0004().end_text(); }
        std::string AutoStory_Checkpoint_0005::end_text()   const { return "Standing in front of PC. Recieved the Poketech. Ready to deposit starter"; }

        void AutoStory_Checkpoint_0005::run_checkpoint(
            SingleSwitchProgramEnvironment& env,
            ProControllerContext& context,
            AutoStoryOptions options,
            AutoStoryStats& stats
        ) const {
            checkpoint_0005(env, context, options, stats);
        }

        }
    }
}
