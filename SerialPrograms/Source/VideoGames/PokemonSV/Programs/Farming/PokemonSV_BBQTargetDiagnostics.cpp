/*  BBQ Target Diagnostics
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <iomanip>
#include <sstream>
#include "Common/Cpp/PrettyPrint.h"
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "PokemonSV_BBQTargetDiagnostics.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{


namespace{

std::string quest_slug(BBQuests quest){
    const EnumEntry* entry = BBQuests_database().find(quest);
    return entry == nullptr ? "unknown" : entry->slug;
}

std::string seconds_to_string(std::chrono::milliseconds elapsed){
    std::ostringstream out;
    out << std::fixed << std::setprecision(1) << elapsed.count() / 1000.0 << "s";
    return out.str();
}

std::string target_diagnostics_directory(){
    return DEBUG_PATH() + "BBQ/targets/";
}

}


TargetAttemptOutcome classify_target_attempt(const TargetAttemptSignals& signals){
    switch (signals.kind){
    case TargetAttemptKind::battle:
        if (signals.encounter_detected){
            return TargetAttemptOutcome::battle_entered;
        }
        return signals.battle_menu_seen
            ? TargetAttemptOutcome::battle_menu_without_encounter
            : TargetAttemptOutcome::no_battle;
    case TargetAttemptKind::photo:
        if (signals.encounter_detected){
            return TargetAttemptOutcome::battle_during_photo;
        }
        return signals.photo_prompt_seen
            ? TargetAttemptOutcome::photo_prompt_seen
            : TargetAttemptOutcome::photo_prompt_timeout;
    }
    return TargetAttemptOutcome::no_battle;
}

const char* target_attempt_outcome_to_string(TargetAttemptOutcome outcome){
    switch (outcome){
    case TargetAttemptOutcome::battle_entered:                  return "battle-entered";
    case TargetAttemptOutcome::no_battle:                       return "no-battle";
    case TargetAttemptOutcome::battle_menu_without_encounter:   return "battle-menu-without-encounter";
    case TargetAttemptOutcome::photo_prompt_seen:               return "photo-prompt-seen";
    case TargetAttemptOutcome::photo_prompt_timeout:            return "photo-prompt-timeout";
    case TargetAttemptOutcome::battle_during_photo:             return "battle-during-photo";
    }
    return "unknown";
}

bool target_attempt_needs_history(TargetAttemptOutcome outcome){
    switch (outcome){
    case TargetAttemptOutcome::battle_entered:
    case TargetAttemptOutcome::photo_prompt_seen:
        return false;
    case TargetAttemptOutcome::no_battle:
    case TargetAttemptOutcome::battle_menu_without_encounter:
    case TargetAttemptOutcome::photo_prompt_timeout:
    case TargetAttemptOutcome::battle_during_photo:
        return true;
    }
    return true;
}

std::string target_attempt_summary(
    BBQuests quest,
    const TargetAttemptSignals& signals,
    std::chrono::milliseconds elapsed
){
    std::ostringstream out;
    out << "BBQ event=target-attempt quest=" << quest_slug(quest)
        << " outcome=" << target_attempt_outcome_to_string(classify_target_attempt(signals))
        << " encounter=" << signals.encounter_detected;
    switch (signals.kind){
    case TargetAttemptKind::battle:
        out << " battle_menu=" << signals.battle_menu_seen;
        break;
    case TargetAttemptKind::photo:
        out << " photo_prompt=" << signals.photo_prompt_seen;
        break;
    }
    out << " elapsed=" << seconds_to_string(elapsed);
    return out.str();
}


TargetAttemptRecorder::TargetAttemptRecorder(VideoStream& stream, BBQuests quest, bool save_files)
    : m_stream(stream)
    , m_quest(quest)
    , m_save_files(save_files)
    , m_start(current_time())
    , m_file_prefix(target_diagnostics_directory() + now_to_filestring() + "-" + quest_slug(quest))
{}

void TargetAttemptRecorder::checkpoint(const std::string& name){
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(current_time() - m_start);
    m_stream.log(
        "BBQ event=target-checkpoint quest=" + quest_slug(m_quest)
        + " checkpoint=" + name + " elapsed=" + seconds_to_string(elapsed)
    );
    if (m_save_files){
        m_frames.emplace_back(name, m_stream.video().snapshot());
    }
}

TargetAttemptOutcome TargetAttemptRecorder::finish(const TargetAttemptSignals& signals){
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(current_time() - m_start);
    m_stream.log(target_attempt_summary(m_quest, signals, elapsed));

    const TargetAttemptOutcome outcome = classify_target_attempt(signals);
    if (!m_save_files){
        return outcome;
    }

    try{
        Filesystem::create_directories(Filesystem::Path(target_diagnostics_directory()));
    }catch (...){}

    for (const auto& [name, frame] : m_frames){
        const std::string path = m_file_prefix + "-" + name + ".png";
        const bool saved = frame && frame->save(path);
        m_stream.log("BBQ event=target-file saved=" + std::to_string(saved) + " path=" + path);
    }
    m_frames.clear();

    //  The stream history covers the approach that led here, if the user has
    //  turned it on in the global settings.
    if (target_attempt_needs_history(outcome)){
        const std::string path = m_file_prefix + "-approach.mp4";
        const bool saved = m_stream.save_stream_history(path);
        m_stream.log("BBQ event=target-file saved=" + std::to_string(saved) + " path=" + path);
    }
    return outcome;
}



}
}
}
