/*  Story Events
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A story checkpoint as a list of events. Each event states what must hold
 *  before it (require), does one thing (act), and checks what the game shows
 *  afterwards (expect). An event whose effect is already on screen (skip_if)
 *  is skipped, so resuming a checkpoint after a reload re-checks instead of
 *  replaying. A failed event is answered by its named recoveries, in order
 *  of escalation (wait, then re-anchor, then stop), and anything without a
 *  recovery stops the checkpoint with the event and the reason.
 *
 *  Interrupts (a wild battle, a trainer, a scene) are declared once per
 *  checkpoint: after each action the runner asks whether one is showing,
 *  clears it, marks what it did to the pose and resumes as it says.
 *
 *  The run state is the runner's to change. An action that knows where it
 *  left the player says so in its Outcome, and the runner writes that into
 *  the pose once the action has succeeded.
 *
 *  Observation and actuation are callables the game supplies, so this file
 *  knows nothing of any one game or console, and the runner can be tested
 *  against a scripted world. See docs/design/AutoStory-Event-Model.md.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryEvents_H
#define PokemonAutomation_Pokemon_StoryEvents_H

#include <cstddef>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include "Common/Cpp/MoveOnlyFunction.h"

namespace PokemonAutomation{
namespace Pokemon{


//  How much of the pose is known. COLUMN and ROW pin one axis (a wall, a
//  door); SET means one of the positions the pose holds.
enum class PoseCertainty{
    TILE,
    COLUMN,
    ROW,
    SET,
    UNKNOWN,
};
const char* certainty_name(PoseCertainty certainty);

//  Whether a pose known to `have` is good enough for an event that needs `need`.
bool certainty_covers(PoseCertainty have, PoseCertainty need);

//  Where on a map the player is, in the game's own terms: tiles and a
//  facing in a 2D game (Pokemon_TilePose.h), a map point or a fly point in
//  an open-world one. The engine only carries and prints it; the game's
//  actions read it back.
struct Position{
    virtual ~Position() = default;
    //  The text after the map in a log line, e.g. "(10, 20) facing north";
    //  none for a game that has nothing to say beyond the map.
    virtual std::string describe() const{
        return "";
    }
};

//  Where the player is believed to be. The screen is read; the pose is
//  tracked, and only an anchor (a wall, a door, an NPC who answered) makes
//  it certain. `where` is null until the game learns a position and after
//  the pose is lost; it is shared because poses are copied (an event's
//  learned pose into the run) and never changed in place.
struct Pose{
    std::string map;
    PoseCertainty certainty = PoseCertainty::UNKNOWN;
    std::shared_ptr<const Position> where;
};
std::string to_string(const Pose& pose);


//  What a run knows. Flags are set only by confirmed effects and are lost
//  on a reload.
struct RunState{
    Pose pose;
    std::set<std::string> flags;
};


//  How an action or a check ended: empty `failure` is success; otherwise a
//  name that recoveries match (e.g. "probe_exhausted"). A succeeded action
//  that knows where it left the player sets `pose`; the runner writes it
//  into the run before the event's expects are checked.
struct Outcome{
    std::string failure;
    std::string detail;
    std::optional<Pose> pose;

    bool succeeded() const;
};
Outcome outcome_ok();
Outcome outcome_ok(Pose pose);
Outcome outcome_failed(std::string failure, std::string detail = "");
//  "probe_exhausted: 4 rows": the failure and its detail, if any.
std::string to_string(const Outcome& outcome);


using Act = MoveOnlyFunction<Outcome(RunState& state)>;
using Check = MoveOnlyFunction<bool(RunState& state)>;

//  A question the screen (or the run state) answers, named for the log.
struct Condition{
    std::string name;
    Check check;
};
Condition condition(std::string name, Check check);

//  Holds while the pose is on `map` (any map if empty) and at least as
//  certain as `need`.
Condition pose_known(std::string map, PoseCertainty need);


enum class RecoveryKind{
    WAIT,       //  Something transient (a wanderer, a slow scene): wait, try again.
    REANCHOR,   //  The pose is in doubt: confirm it on a landmark, try again.
    STOP,       //  Nothing to do: stop the checkpoint.
};
const char* recovery_name(RecoveryKind kind);

//  Answers a failure named `on` (any failure if empty): runs `prepare`, if
//  any, and tries the event again, up to `attempts` times.
struct Recovery{
    std::string on;
    RecoveryKind kind = RecoveryKind::STOP;
    Act prepare;
    size_t attempts = 1;
    std::string why;
};


struct Event{
    std::string name;
    std::string why;
    //  Skipped when all of these hold (and there is at least one).
    std::vector<Condition> skip_if;
    //  Must hold before `act`; a failure is "require_failed".
    std::vector<Condition> require;
    Act act;
    //  Must hold after `act`; a failure is "expect_failed".
    std::vector<Condition> expect;
    //  Written to the run's pose once the event succeeds. An action that
    //  learns its pose as it goes (the row that answered) returns it in
    //  its Outcome instead.
    std::optional<Pose> pose_learned;
    //  In order of escalation.
    std::vector<Recovery> recoveries;
};


enum class InterruptPose{
    UNCHANGED,  //  Cleared where we stood (a talk-only trainer).
    WITHIN_SET, //  Moved an unknown part of a leg: still within the position we had, no longer exactly where.
    LOST,       //  Anywhere.
};
enum class ResumePolicy{
    RETRY_EVENT,    //  Run the event again, from its skip_if.
    RECHECK_EXPECT, //  The action may have finished: done if its expects hold, else retry.
    STOP,           //  Stop the checkpoint.
};

//  Something that can stop an action part-way.
struct Interrupt{
    std::string name;
    Check detect;
    Act handle;
    InterruptPose pose = InterruptPose::UNCHANGED;
    ResumePolicy resume = ResumePolicy::RETRY_EVENT;
};


struct Checkpoint{
    std::string id;
    //  Must hold before the first event (the save it starts from).
    std::vector<Condition> start;
    std::vector<Interrupt> interrupts;
    std::vector<Event> events;
    //  Must hold after the last event.
    std::vector<Condition> end;
    //  Saves the game after the end check, if set.
    Act save;
    size_t max_interrupts_per_event = 5;
};


struct CheckpointReport{
    bool completed = false;
    //  1-based number of the event that failed; 0 for the start or end check
    //  or the save, or when nothing failed.
    size_t failed_event = 0;
    std::string failed_name;
    Outcome failure;
    size_t events_run = 0;
    size_t events_skipped = 0;
};

using EventLogger = MoveOnlyFunction<void(const std::string& line)>;


//  One log line, in a format the lab watcher parses:
//      [Event] 00.04/3 talk_mom: start
//      [Event] 00.04/3 talk_mom: failed: expect_failed: has_parcel
std::string event_log_line(const std::string& checkpoint, size_t number, const std::string& event, const std::string& status);

//  Problems an author should fix before running: unnamed or empty events,
//  missing actions, recoveries out of escalation order.
std::vector<std::string> lint_checkpoint(const Checkpoint& checkpoint);

//  A readable dump for review, one line per item: the checkpoint's start,
//  interrupts and end by name, each event with its why, its conditions'
//  names, its learned pose and its recoveries. Nothing runs.
std::string describe(const Checkpoint& checkpoint);

//  Runs the checkpoint's events in order, from the first. Events whose
//  effect is already showing are skipped, so after a reload this resumes.
//  Logs each event's start and end through `log`.
CheckpointReport run_checkpoint(RunState& state, Checkpoint& checkpoint, EventLogger& log);



}
}
#endif
