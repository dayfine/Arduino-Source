/*  BDSP AutoStory - Landmark Alignment
 *  Lines the player up with an overworld landmark seen on screen.
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <chrono>
#include <cmath>

#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "PokemonBDSP/Inference/PokemonBDSP_TransitionWatchers.h"
#include "PokemonBDSP/Programs/AutoStory/Detect/PokemonBDSP_AutoStory_OverworldDetector.h"

#include "PokemonBDSP_AutoStory_LandmarkAlign.h"

using namespace std::chrono_literals;

namespace PokemonAutomation {
    namespace NintendoSwitch {
        namespace PokemonBDSP {

            void align_to_overworld_landmarks(VideoStream& stream, ProControllerContext& context,
                OverworldMatchResult (*matcher)(const ImageRGB32&), const std::string& label, bool allow_story_start, bool horizontal_first,
                bool search_down_first,
                OverworldMatchResult (*entry_matcher)(const ImageRGB32&),
                bool* entry_detected,
                double arrival_tolerance_x, double arrival_tolerance_y,
                double moderate_entry_confidence, bool* rock_acquisition_timed_out) {
                if (rock_acquisition_timed_out) *rock_acquisition_timed_out=false;
                auto missing_rocks_since=std::chrono::steady_clock::time_point::max();
                if (entry_detected) *entry_detected=false;
                context.wait_for_all_requests();
                pbf_wait(context, 500ms);
                context.wait_for_all_requests();
                ShortDialogDetector dialogue(COLOR_YELLOW);
                BattleMenuDetector battle(BattleType::TRAINER, COLOR_GREEN);
                size_t confirmations = 0, missing = 0;
                for (size_t attempt = 0; attempt < 80; attempt++) {
                    const auto frame = stream.video().snapshot();
                    if (!frame) OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                        label + ": No video frame.", stream);
                    if (entry_matcher){
                        BlackScreenDetector transition;
                        if (entry_matcher(*frame.frame).located || transition.detect(*frame.frame)){
                            BlackScreenWatcher visible(COLOR_RED,{0.1,0.1,0.8,0.8},
                                100,10,BlackScreenWatcher::FinderType::GONE,500ms);
                            if (wait_until(stream,context,30s,{{visible}})!=0)
                                OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                                    label+": Building transition did not finish.",stream);
                            const auto inside=stream.video().snapshot();
                            if (!inside || !entry_matcher(*inside.frame).located)
                                OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                                    label+": Expected interior not confirmed after transition.",stream);
                            if(entry_detected)*entry_detected=true;
                            stream.log("["+label+"] Expected interior confirmed; ending outdoor alignment.",COLOR_GREEN);
                            return;
                        }
                    }
                    // The story can start before the exact reference point. Hand off
                    // to battle preparation instead of steering during dialogue.
                    if (dialogue.detect(*frame.frame) || battle.detect(*frame.frame)) {
                        if (allow_story_start) {
                            stream.log("[" + label + "] Story dialogue/battle started; handing off without further movement.");
                            return;
                        }
                        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                            label + ": Unexpected dialogue/battle during alignment.", stream);
                    }
                    const auto result = matcher(*frame.frame);
                    if (result.located)
                        missing_rocks_since=std::chrono::steady_clock::time_point::max();
                    VideoOverlaySet overlays(stream.overlay());
                    for (const auto& landmark : result.landmarks) {
                        overlays.add(COLOR_CYAN, landmark.search);
                        if (landmark.accepted) overlays.add(COLOR_GREEN, landmark.found);
                        stream.log("[" + label + "] " + landmark.name + " score=" +
                            std::to_string(landmark.score) + ", margin=" + std::to_string(landmark.margin) +
                            ", offset=(" + std::to_string(landmark.dx) + "," + std::to_string(landmark.dy) + ")");
                    }
                    stream.log("[" + label + "] located=" + std::to_string(result.located) +
                        ", coarse=" + std::to_string(result.coarse) + ", at_target=" + std::to_string(result.at_target) +
                        ", consensus offset=(" + std::to_string(result.dx) + "," + std::to_string(result.dy) + ")");
                    const double entry_confidence=moderate_entry_confidence>0 ? overworld_entry_confidence(result) : 0;
                    const bool moderate_arrival=moderate_entry_confidence>0 && entry_confidence>=moderate_entry_confidence;
                    if(moderate_entry_confidence>0) stream.log("["+label+"] Exterior entry confidence="+
                        std::to_string(entry_confidence)+", moderate threshold="+std::to_string(moderate_entry_confidence)+
                        ", strict target="+std::to_string(result.at_target));
                    // Explicit loose bounds are for non-interacting route handoffs.
                    // Default callers still require the matcher's precise arrival.
                    const bool rough_arrival=arrival_tolerance_x>0 && arrival_tolerance_y>0 &&
                        result.located && std::abs(result.dx)<=arrival_tolerance_x &&
                        std::abs(result.dy)<=arrival_tolerance_y;
                    if (result.at_target || rough_arrival || moderate_arrival) {
                        if (++confirmations >= 3) { stream.log("[" + label + "] Position confirmed."); return; }
                        pbf_wait(context, 250ms);
                    } else if (result.located) {
                        confirmations = 0;
                        missing = 0;
                        // Wait at an aligned pose while strict visual support
                        // recovers, rather than stepping away under a cloud.
                        if ((matcher==match_galactic_hq_jupiter ||
                            matcher==match_route208_hannah_departure || matcher==match_route208_stair_top) && result.coarse &&
                            std::abs(result.dx)<=1.0/128 && std::abs(result.dy)<=1.0/54){
                            pbf_wait(context,500ms);
                            context.wait_for_all_requests();
                            continue;
                        }
                        // A coarse match cannot confirm position, but two accepted
                        // anchors can establish a safe horizontal direction when both
                        // offsets are well outside the uncertainty in their spacing.
                        bool coarse_horizontal = false;
                        if (horizontal_first && result.coarse && result.landmarks.size() == 2) {
                            const auto& a = result.landmarks[0];
                            const auto& b = result.landmarks[1];
                            const double nearest = std::min(std::abs(a.dx), std::abs(b.dx));
                            coarse_horizontal = a.accepted && b.accepted && a.dx*b.dx > 0 &&
                                nearest > 1.0/64 && nearest > 2*std::abs(a.dx-b.dx);
                        }
                        const bool plaza_anchor_horizontal = matcher == match_jubilife_plaza_approach &&
                            result.coarse && std::abs(result.dx)>1.0/50 && std::abs(result.dy)<=1.0/54;
                        // A uniquely matched Eterna ball may correct the door
                        // column while the sign/bush enter the frame. Arrival
                        // still requires the matcher's full three-anchor check.
                        const bool eterna_anchor_horizontal = (matcher == match_eterna_center_entrance ||
                            matcher == match_eterna_underground_house || matcher == match_eterna_underground_man) &&
                            result.coarse && std::abs(result.dx)>1.0/128;
                        // Recover the gym's doorway row before moving right if
                        // the south approach overshot toward the fence/cliff.
                        const bool gym_anchor_horizontal=matcher==match_eterna_gym_entrance &&
                            result.coarse && std::abs(result.dx)>1.0/128 && std::abs(result.dy)<=.06;
                        const bool gym_row_recovery=matcher==match_eterna_gym_entrance &&
                            result.coarse && std::abs(result.dy)>.06;
                        // Reach the row north of Jupiter before entering her
                        // column. From above that row, align sideways before
                        // moving south so the NPC cannot block the correction.
                        const bool jupiter_row_recovery=matcher==match_galactic_hq_jupiter &&
                            result.dy < -1.0/54;
                        const bool jupiter_anchor_horizontal=matcher==match_galactic_hq_jupiter &&
                            !jupiter_row_recovery && result.coarse && std::abs(result.dx)>1.0/128;
                        // Stay south of the Contest Hall wall/sign before crossing to Fantina.
                        const bool contest_row_recovery=matcher==match_hearthome_contest_fantina && result.dy>1.0/54;
                        // Keep the stair approach horizontal-first until close to
                        // its column. Recorded fine poses otherwise spent repeated
                        // 16ms sideways turns while a larger vertical error waited.
                        // Fine correction changes axis priority, never arrival gates.
                        const bool stair_fine_correction=matcher==match_route208_stair_top &&
                            !result.coarse && std::abs(result.dx)<=1.0/64;
                        const bool horizontal = !contest_row_recovery && !gym_row_recovery && !jupiter_row_recovery && (jupiter_anchor_horizontal || coarse_horizontal || plaza_anchor_horizontal || eterna_anchor_horizontal || gym_anchor_horizontal || (!result.coarse &&
                            ((horizontal_first && !stair_fine_correction && std::abs(result.dx)>1.0/128) ||
                             std::abs(result.dx)>=std::abs(result.dy)*9.0/16)));
                        if (stair_fine_correction && horizontal_first && !horizontal && std::abs(result.dx)>1.0/128)
                            stream.log("["+label+"] Near stair column; correcting the larger vertical error first.");
                        if (coarse_horizontal) stream.log("[" + label +
                            "] Both landmarks agree on a large horizontal offset; correcting the street lane first.");
                        const double error = horizontal ? result.dx : result.dy*9.0/16;
                        auto duration = std::abs(error) > 1.0/24 ? 80ms : std::abs(error) > 1.0/64 ? 40ms : 16ms;
                        // A 16ms turn may not complete a step. Use a bounded
                        // full correction while reacquiring the second plaza anchor.
                        if (matcher == match_jubilife_plaza_approach && result.coarse)
                            duration = std::max(duration, 60ms);
                        if (matcher==match_galactic_hq_jupiter){
                            for (const auto& landmark:result.landmarks){
                                // The player stands around y=.74. Do not steer
                                // sideways/north when the stair opening is close
                                // to that row; a southward correction moves away.
                                if (landmark.name=="StairBoundary" && landmark.accepted &&
                                    landmark.found.y+landmark.found.height>.60 &&
                                    (horizontal || result.dy<0))
                                    OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                                        label+": Correction would approach the stair boundary.",stream);
                            }
                        }
                        pbf_move_left_joystick(context,
                            {horizontal ? (result.dx > 0 ? 1.0 : -1.0) : 0.0,
                             horizontal ? 0.0 : (result.dy < 0 ? 1.0 : -1.0)}, duration, 0ms);
                        pbf_wait(context, 500ms);
                    } else {
                        confirmations = 0;
                        if (matcher==match_hearthome_center_entrance || matcher==match_hearthome_contest_fantina){
                            // Do not sweep into a different doorway or around the Hall's
                            // side when a cloud, NPC or fade hides the required anchors.
                            if (++missing>20) OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                                label+": Scene anchors missing; stationary reacquisition exhausted.",stream);
                            pbf_wait(context,500ms);
                            context.wait_for_all_requests();
                            continue;
                        }
                        if (matcher==match_galactic_hq_jupiter || matcher==match_route208_stair_top){
                            // Keep generic missing-anchor behavior unchanged. Route208's
                            // explicit caller can instead request its recorded recovery.
                            const bool stair_rocks=matcher==match_route208_stair_top;
                            if (stair_rocks && rock_acquisition_timed_out) {
                                const auto now=std::chrono::steady_clock::now();
                                if (missing_rocks_since==std::chrono::steady_clock::time_point::max())
                                    missing_rocks_since=now;
                                // Matcher processing can take seconds per observation;
                                // measure real elapsed time rather than counting waits.
                                if (now-missing_rocks_since>=10s) {
                                    context.wait_for_all_requests();
                                    *rock_acquisition_timed_out=true;
                                    stream.log("["+label+"] Rock acquisition timed out after 10s; handing off to the requested cliff recovery.");
                                    return;
                                }
                            } else if (++missing>(stair_rocks ? 20 : 6)) {
                                OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                                    label+(stair_rocks ? ": Rock anchors missing; no blind search near stairs." :
                                        ": Chair/stair anchors missing; no blind search near stairs."),stream);
                            }
                            pbf_wait(context,500ms);
                            context.wait_for_all_requests();
                            continue;
                        }
                        // Search right first, then across the starting point to the
                        // left and back. Each complete sweep returns to its origin.
                        // Try that sweep at the initial height, above it, and below it;
                        // no missing/weak landmark ever confirms the destination.
                        if (++missing > (search_down_first ? 78 : 74)) OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                            label + ": Landmarks missing during bounded bidirectional search.", stream);
                        if (search_down_first && missing <= 4) {
                            stream.log("[" + label + "] Post-battle acquisition: moving down before lateral search.");
                            pbf_move_left_joystick(context, {0.0, -1.0}, 100ms, 0ms);
                        } else {
                            const size_t search_step = missing - 1 - (search_down_first ? 4 : 0);
                            const size_t level = search_step / 25;
                            const size_t slot = search_step % 25;
                            const bool lateral = slot < 24;
                            const double direction = slot < 6 || slot >= 18 ? 1.0 : -1.0;
                            const auto search_duration = lateral ? 100ms : level == 0 ? 600ms : 1200ms;
                            stream.log("[" + label + "] Acquisition search step=" + std::to_string(missing) +
                                ", direction=" + (lateral ? (direction>0 ? "right" : "left") : (level==0 ? "up" : "down")));
                            // On the street, a northward search from the east
                            // edge can stop at the building/NPC collision row.
                            // Step west around it before changing height, then
                            // return to the same lane for the next bounded sweep.
                            const bool street_height_search=!lateral && matcher==match_jubilife_street_approach;
                            if (street_height_search) pbf_move_left_joystick(context,{-1.0,0.0},150ms,0ms);
                            pbf_move_left_joystick(context,
                                {lateral ? direction : 0.0, lateral ? 0.0 : (level == 0 ? 1.0 : -1.0)}, search_duration, 0ms);
                            if (street_height_search) pbf_move_left_joystick(context,{1.0,0.0},150ms,0ms);
                        }
                        pbf_wait(context, 500ms);
                    }
                    context.wait_for_all_requests();
                }
                OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                    label + ": Could not confirm the reference position.", stream);
            }


        }
    }
}
