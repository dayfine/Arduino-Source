/*  Lab Run Files
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The lab channel's copy of the run records (CommonFramework/RunRecord.h)
 *  as files, one folder per run under "Lab/runs/<run_id>/":
 *
 *      meta.json       What ran: the program, the app's version, the git
 *                      commit it was built from, the program's settings.
 *                      Written when the run starts.
 *      result.json     The record without its events and log: status,
 *                      outcome, error, times, the frame's file name and the
 *                      last event. Rewritten as the run goes; final once
 *                      "status" is "terminal".
 *      events.jsonl    One JSON object per line, appended as the program
 *                      reports events; console probes (shot, detect) taken
 *                      during or right after the run are appended too.
 *      log.txt         The run's own log lines, appended as they come,
 *                      up to RunRecordStore::MAX_LOG_BYTES.
 *      frame-*.png     The frames of events and of the result.
 *      probe-*.png     Screenshots of probes attached to the run.
 *
 *  The newest KEEP folders stay; older ones are removed when a run starts.
 *  Lab.md documents the shapes.
 *
 */

#ifndef PokemonAutomation_LabRunFiles_H
#define PokemonAutomation_LabRunFiles_H

#include <map>
#include <optional>
#include <string>
#include "Common/Cpp/Time.h"
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "CommonFramework/RunRecord.h"

namespace PokemonAutomation{


//  "2026-11-01 15:30:12.123", local time, as the log writes it.
std::string lab_time_text(WallClock time);

//  The text of result.json. `frame_file` is the result frame's file name in
//  the run's folder, if it was written; `event_frames` maps an event's index
//  to its frame's file name.
std::string run_result_json(
    const RunRecord& record,
    const std::optional<std::string>& frame_file,
    const std::map<size_t, std::string>& event_frames
);

//  The text of meta.json. The settings are embedded as JSON when they
//  parse, else as a string.
std::string run_meta_json(const RunRecord& record);

//  One line of events.jsonl, without the newline.
std::string run_event_json_line(const RunEvent& event, const std::optional<std::string>& frame_file);

//  The file name of an event's frame: "frame-<index>[-<checkpoint>][-s<step>][-a<attempt>].png".
std::string event_frame_file(size_t index, const RunEvent& event);

//  Writes `text` to `path`; the atomic one in one step, so a reader never
//  sees a partial file.
void write_lab_file(const Filesystem::Path& path, const std::string& text);
void write_lab_file_atomically(const Filesystem::Path& path, const std::string& text);


class LabRunWriter{
public:
    static constexpr size_t DEFAULT_KEEP = 32;

    //  Probes taken this long after a run ended still belong to it.
    static constexpr Seconds PROBE_WINDOW = Seconds(120);

    //  `folder` is "Lab/runs/", ending in '/'. It is created on the first run.
    explicit LabRunWriter(std::string folder, size_t keep = DEFAULT_KEEP);

    const std::string& folder() const{ return m_folder; }
    std::string run_folder(const std::string& run_id) const{ return m_folder + run_id + "/"; }

    //  Writes what `store` has that the files don't yet. Call from one
    //  thread, now and then; nothing is held between calls but counters.
    void sync(const RunRecordStore& store);

    //  The run a probe taken now belongs to: the active run, or the last
    //  one if it ended less than PROBE_WINDOW ago.
    std::optional<std::string> run_for_probe(WallClock now) const;

    //  Appends a console command and what it printed to the run's
    //  events.jsonl as a "probe" event, and copies each file a "shot <path>"
    //  line names into the run's folder as "probe-<file name>".
    void attach_probe(const std::string& run_id, const std::string& command, const std::string& output);


private:
    struct Written{
        size_t events = 0;              //  Events written to events.jsonl so far.
        size_t log_lines = 0;           //  Log lines written to log.txt so far.
        std::map<size_t, std::string> event_frames = {};
        std::string result_text = {};   //  The last result.json written.
        bool finished = false;
    };

    void write_run(const RunRecord& record, Written& written);
    void append_log(const RunRecord& record, Written& written);
    void prune();

    const std::string m_folder;
    const size_t m_keep;
    std::map<std::string, Written> m_written;
    std::string m_last_run;             //  The newest run seen, "" before the first.
    std::optional<WallClock> m_last_end;    //  When it ended, once it has.
};



}
#endif
