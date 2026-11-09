/*  AutoStory Failure Capture
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What a failed AutoStory event leaves behind, whatever the error-report
 *  settings say (a story failure throws with NO_ERROR_REPORT, so no error
 *  report is written for it): the stream history as a video, the frame on
 *  screen and a short JSON, in
 *
 *      USER_FILE_PATH()/<game>/AutoStoryFailures/<time>-<checkpoint>-<event>/
 *          Video.mp4       the stream history, when its setting is on
 *          Screenshot.png  the frame on screen when the event failed
 *          Failure.json    game, checkpoint, event, error, the last log lines
 *
 *  The video holds the whole stream history (the "Stream History" setting's
 *  seconds), saved before the reload. Pokemon_FrameReplay.h says how to cut
 *  the seconds before the failure into a frame-replay fixture.
 *
 */

#ifndef PokemonAutomation_Pokemon_AutoStoryFailureCapture_H
#define PokemonAutomation_Pokemon_AutoStoryFailureCapture_H

#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Json/JsonObject.h"
#include "CommonFramework/Tools/VideoStream.h"
#include "Pokemon_StorySegments.h"

namespace PokemonAutomation{
namespace Pokemon{


//  One failure, as Failure.json holds it.
struct FailureRecord{
    std::string game;
    std::string checkpoint;
    std::string event;
    std::string error;
    //  The last log lines before the failure, oldest first.
    std::vector<std::string> log;
};

//  "<time>-<checkpoint>-<event>", each character a folder name can't hold
//  made "_". An empty event (a start or end check failed) is "checkpoint".
std::string failure_folder_name(std::string_view time, const FailureRecord& record);

JsonObject failure_json(const FailureRecord& record);

//  The checkpoint and event a status line names:
//  "00.03/2 below_oaks_trigger: start" names 00.03 and below_oaks_trigger.
//  Both empty for a line that names no event ("00: start, 3 checkpoints").
struct StatusStep{
    std::string checkpoint;
    std::string event;
};
StatusStep step_of_status(std::string_view line);


//  Saves a capture for each failed event of the run it is attached to.
class FailureCapture{
public:
    //  `game` names the folder under USER_FILE_PATH(): "PokemonBDSP".
    FailureCapture(VideoStream& stream, std::string game);

    //  Sets `hooks.failed`, and wraps `hooks.status` to remember the step
    //  under way for save_thrown(). Must outlive the hooks.
    void attach(SegmentHooks& hooks);

    //  For an exception out of the run, saved under the last step a status
    //  line named.
    void save_thrown(const std::string& error);

    //  Writes the folder and returns its path, or "" if it couldn't be
    //  made. Logs what it skipped; never throws.
    std::string save(const std::string& checkpoint, const std::string& event, const std::string& error);

private:
    VideoStream& m_stream;
    std::string m_game;
    StatusStep m_last_step;
};



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
