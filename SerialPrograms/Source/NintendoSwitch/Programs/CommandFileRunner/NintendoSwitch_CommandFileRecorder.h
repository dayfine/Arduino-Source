/*  Command File Recorder
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The steps file of a command file: "<outbox>/<name>.steps.jsonl", one JSON
 *  object per command, appended as each command ends. The journal
 *  ("<name>.out") is for a person reading along; this file is for tools,
 *  such as devtools/lab/cfr_steps_to_story.py, which turns a story played by
 *  hand into story table rows. A line looks like:
 *
 *      {"after":{"lza_dialog":false,"lza_facing":87},"before":{...},
 *       "command":"lza_move","args":["90","1500"],"duration_ms":1640,
 *       "end":"2026-11-08T21:04:12.517Z","line":3,"ok":true,
 *       "screenshot":".../probe_step_L3.jpg","start":"2026-11-08T21:04:10.877Z"}
 *
 *  "before" and "after" hold the games' detectors and readings, and are left
 *  out when no game registered any. A failed command has "ok":false and an
 *  "error" message.
 *
 */

#ifndef PokemonAutomation_NintendoSwitch_CommandFileRecorder_H
#define PokemonAutomation_NintendoSwitch_CommandFileRecorder_H

#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/Time.h"
#include "Common/Cpp/Json/JsonObject.h"

namespace PokemonAutomation{
namespace NintendoSwitch{


//  One command as it ran.
struct CommandFileStep{
    size_t line = 0;                    //  In the command file, from 1.
    std::vector<std::string> words;     //  The command and its arguments.
    WallClock start = WallClock::min();
    WallClock end = WallClock::min();
    JsonObject before;                  //  Detector name -> what it saw. Empty: none registered.
    JsonObject after;
    std::optional<std::string> error;   //  Why the command failed; none if it succeeded.
    std::string screenshot;             //  The frame when it ended.
};

//  `step` as one line of JSON, without the newline.
std::string command_file_step_json(const CommandFileStep& step);

//  Appends `step` to the steps file at `path`.
void append_command_file_step(const std::string& path, const CommandFileStep& step);



} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
