/*  BBQ Failure Snapshots
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonSV_BBQSnapshot_H
#define PokemonAutomation_PokemonSV_BBQSnapshot_H

#include <string>
#include <string_view>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "PokemonSV_BBQLogger.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{


// Where BBQ failure screenshots are saved: DEBUG_PATH()/BBQ/.
std::string bbq_snapshot_directory();

// Save a screenshot of a failed quest attempt into `directory` (created if
// missing) as <timestamp>-<quest>-<reason>.png, and log the outcome as a
// "BBQ event=snapshot" line. Unlike dump_image(), this stays on the local
// machine and sends nothing to telemetry.
// Returns the path written, or an empty string if the image was not saved.
std::string save_bbq_snapshot(
    Logger& logger, const ImageViewRGB32& frame,
    const std::string& directory, BBQuests quest, std::string_view reason
);


// Board verification decides whether the attempt completed; capturing evidence does not.
enum class BBQAttemptOutcome{
    Completed,
    StillOnBoard,
};

// Log a battle-entry timeout and save its current frame locally; no controller input.
void record_bbq_battle_entry_timeout(ConsoleHandle& console, BBQuests quest);

// Save the verification-failure frame when needed, then log the attempt result.
// The caller retains all retry/counting decisions; this performs no controller input.
void record_bbq_attempt_result(
    ConsoleHandle& console, BBQAttemptLog& attempt, BBQuests quest, BBQAttemptOutcome outcome
);

} // namespace PokemonSV
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
