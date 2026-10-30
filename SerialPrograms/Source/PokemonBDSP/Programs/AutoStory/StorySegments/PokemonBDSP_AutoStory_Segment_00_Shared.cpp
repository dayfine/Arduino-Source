/*  BDSP AutoStory - Segment 00 Shared
 *  Helpers used by more than one Segment 00 checkpoint.
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/VisualDetectors/BlackScreenDetector.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"

#include "PokemonBDSP_AutoStory_Segment_00_Shared.h"

using namespace std::chrono_literals;

namespace PokemonAutomation {
    namespace NintendoSwitch {
        namespace PokemonBDSP {

            // Watch before movement starts, release on the fade, and hold the
            // following recorded wait until the destination is visible again.
            void recorded_move_until_transition(
                VideoStream& stream, ProControllerContext& context,
                double x, double y, Milliseconds duration, const std::string& label
            ) {
                context.wait_for_all_requests();
                stream.log("[Tutorial transition] " + label + ": approaching fade.");
                BlackScreenWatcher black;
                int ret = run_until<ProControllerContext>(stream, context,
                    [x, y, duration](ProControllerContext& ctx) {
                        pbf_move_left_joystick(ctx, {x, y}, duration, 0ms);
                        // Keep observing a delayed fade after the recorded hold.
                        pbf_wait(ctx, 5000ms);
                    }, {{black}});
                if (ret != 0) {
                    OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                        label + ": Black transition not detected.", stream);
                }
                BlackScreenWatcher visible(COLOR_RED, {0.1, 0.1, 0.8, 0.8},
                    100, 10, BlackScreenWatcher::FinderType::GONE, 500ms);
                if (wait_until(stream, context, 15s, {{visible}}) != 0) {
                    OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                        label + ": Transition did not finish.", stream);
                }
                stream.log("[Tutorial transition] " + label + ": destination visible.");
            }

        }
    }
}
