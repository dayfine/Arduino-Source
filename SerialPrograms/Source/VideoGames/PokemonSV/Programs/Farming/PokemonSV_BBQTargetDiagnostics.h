/*  BBQ Target Diagnostics
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Records how each attempt to reach a catch, Sneak Up or photo target went,
 *  to find out why so many attempts never find their target. It only observes:
 *  it sends no controller commands and changes no decision.
 *
 */

#ifndef PokemonAutomation_PokemonSV_BBQTargetDiagnostics_H
#define PokemonAutomation_PokemonSV_BBQTargetDiagnostics_H

#include <chrono>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/Time.h"
#include "CommonFramework/Tools/VideoStream.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "VideoGames/PokemonSV/Options/PokemonSV_BBQOption.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{


enum class TargetAttemptKind{
    battle,     //  Catch and Sneak Up: lock on to the target to start a battle.
    photo,      //  Photo: frame the target and take a picture.
};

enum class TargetAttemptOutcome{
    battle_entered,
    no_battle,
    //  The battle menu showed up, but the encounter watcher did not fire, so
    //  the route treats the attempt as if no battle started.
    battle_menu_without_encounter,
    photo_prompt_seen,
    //  Not proof the photo failed: some photo quests still complete.
    photo_prompt_timeout,
    battle_during_photo,
};

//  What the route's detectors reported for one attempt.
struct TargetAttemptSignals{
    TargetAttemptKind kind = TargetAttemptKind::battle;
    bool encounter_detected = false;
    bool battle_menu_seen = false;      //  Battle routes only.
    bool photo_prompt_seen = false;     //  Photo routes only.
};

TargetAttemptOutcome classify_target_attempt(const TargetAttemptSignals& signals);
const char* target_attempt_outcome_to_string(TargetAttemptOutcome outcome);

//  Whether the stream history before this outcome is worth keeping: it is,
//  whenever the target may not have been reached.
bool target_attempt_needs_history(TargetAttemptOutcome outcome);

//  The log line for an attempt, in the same "BBQ event=..." form as the other
//  BBQ logs so run audits can grep for it.
std::string target_attempt_summary(
    BBQuests quest,
    const TargetAttemptSignals& signals,
    std::chrono::milliseconds elapsed
);


//  Records one attempt: logs each checkpoint and the outcome. With
//  "save_files", it also saves the frame at each checkpoint, and the stream
//  history when the target may not have been reached, under
//  DebugDumps/BBQ/targets/. Files are written in finish(), after the attempt,
//  so saving them never delays a controller command during the approach.
class TargetAttemptRecorder{
public:
    TargetAttemptRecorder(VideoStream& stream, BBQuests quest, bool save_files);

    //  Logs a checkpoint and keeps the current frame. Call it only where the
    //  route already waits for its commands to finish, so the frame shows the
    //  result of the commands sent so far.
    void checkpoint(const std::string& name);

    TargetAttemptOutcome finish(const TargetAttemptSignals& signals);

private:
    VideoStream& m_stream;
    const BBQuests m_quest;
    const bool m_save_files;
    const WallClock m_start;
    const std::string m_file_prefix;
    std::vector<std::pair<std::string, VideoSnapshot>> m_frames;
};



}
}
}
#endif
