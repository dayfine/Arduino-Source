/*  BBQ Failure Snapshots
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <sstream>
#include "Common/Cpp/PrettyPrint.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "PokemonSV_BBQSnapshot.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{


std::string bbq_snapshot_directory(){
    return DEBUG_PATH() + "BBQ/";
}

std::string save_bbq_snapshot(
    Logger& logger, const ImageViewRGB32& frame,
    const std::string& directory, BBQuests quest, std::string_view reason
){
    const std::string& slug = BBQuests_enum_to_string(quest);
    std::ostringstream path;
    path << directory << now_to_filestring() << '-'
         << (slug.empty() ? "unknown" : slug) << '-' << reason << ".png";
    const std::string filename = path.str();

    const bool saved = frame.save(filename);
    std::ostringstream out;
    out << "BBQ event=snapshot saved=" << saved << " path=" << filename;
    logger.log(out.str());
    return saved ? filename : std::string();
}


void record_bbq_battle_entry_timeout(ConsoleHandle& console, BBQuests quest){
    BBQLogger(console.logger()).battle_entry_timeout(quest);
    save_bbq_snapshot(console.logger(), console.video().snapshot(), bbq_snapshot_directory(), quest, "battle-entry-timeout");
}

void record_bbq_attempt_result(
    ConsoleHandle& console, BBQAttemptLog& attempt, BBQuests quest, BBQAttemptOutcome outcome
){
    if (outcome == BBQAttemptOutcome::StillOnBoard){
        save_bbq_snapshot(console.logger(), console.video().snapshot(), bbq_snapshot_directory(), quest, "verification-failed");
    }
    attempt.result(outcome == BBQAttemptOutcome::Completed);
}

} // namespace PokemonSV
} // namespace NintendoSwitch
} // namespace PokemonAutomation
