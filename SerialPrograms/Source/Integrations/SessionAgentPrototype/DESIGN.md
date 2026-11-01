# Run results and failure evidence for agent-driven testing

Status: proposed follow-up to the four-method prototype in [README.md](README.md).
The run API below is not implemented by #116. This document defines the next
experiment and the conditions for keeping it.

## Goal

An agent can start a configured program, identify that particular run, and
retrieve its terminal outcome and associated failure evidence even after a
later run starts. Build this into Lab's existing backend, using ProgramSession
as the lifecycle authority. File commands and a future MCP adapter should call
the same operations.

The intended improvement is reliable investigation: no guessing which attempt
an error belongs to, no treating an accepted start as completion, and no using
a screenshot taken after a retry as evidence of the original failure. This
does not make gameplay logic correct or establish story success.

The first proof is a failed checkpoint whose result includes the original
error and failure frame. Program selection, configuration editing, controller
inputs and interactive CFR investigation continue through existing Lab tools.

## Existing seams and gaps

- [ProgramSession](../../CommonFramework/ProgramSession.h) owns start/stop,
  the worker and listeners. Its STOPPED state does not distinguish normal
  completion, cancellation or failure. Listener errors are transient strings;
  some, such as a stats-save error, are not a gameplay failure.
- [run_program()](../../CommonFramework/ProgramSession.cpp) already classifies
  several exception types. Some setup work, prerequisite downloads and early
  returns occur outside its main catch block. Observing state changes alone
  cannot provide a complete run outcome.
- [ProgramTracker](../ProgramTracker.h) discovers existing sessions and consoles.
  Reuse it; do not add another registry of live session pointers. Its current
  locking does not by itself prove safety during derived-session destruction.
- [LabChannel](../../CommonFramework/Windows/LabChannel.cpp) supplies a command
  transport, ownership checks and a journal. Preserve those controls and make
  the run operations available through that channel.
- `OperationFailedExceptionWithScreenshot` and `FatalProgramException` already
  expose an owned screenshot. Preserve that frame before the exception dies;
  a later camera snapshot is weaker evidence.

## Identity and result model

| Identifier | Meaning | Lifetime |
|---|---|---|
| Program identifier | Descriptor/type, such as a particular AutoStory program | Across sessions |
| Session ID | One live ProgramSession | Until it is unloaded |
| Run ID | One accepted execution of that session | Until its record expires |
| Request ID | One client's start request, used for safe retries | Bounded deduplication window |

Use opaque string run IDs containing a fresh app-instance identity and a
monotonic run counter. A restart cannot accidentally resolve an old ID to a
new run. Do not encode a mutable selection or an implicit "current program".

Each record contains the run/session/program IDs, associated console IDs,
start/end timestamps, lifecycle status, terminal outcome, optional primary
error, bounded diagnostics, and evidence descriptors. Record the app revision
and the effective program configuration once at acceptance, excluding secrets;
identify any configuration fields that cannot be recorded. If a fingerprint
is used, version its canonicalization. Reproducibility should not depend on
whatever options happen to be selected when the agent later inspects a result.

Lifecycle status is `running`, `stopping` or `terminal`. Terminal outcomes are:

| Outcome | Meaning |
|---|---|
| `completed` | Normal return or ProgramFinishedException; the program declared completion |
| `cancelled` | Execution ended through the existing cancellation path |
| `failed` | A setup, connection or execution error prevented completion |

`completed` does not mean a story effect was independently verified. Optional
program-reported progress and verification results are separate fields. The
initial implementation does not invent checkpoint IDs by parsing log text.

A rejected start returns the existing rejection reason and creates no run.
An accepted start creates exactly one record before execution can emit events.
Even a program that finishes before the start response must remain queryable.
Record UI-initiated runs too, so an agent can inspect them without claiming
that its own request started them.

## Shared operations

These are proposed semantic operations; JSON field names can be settled during
implementation. They replace the prototype's parallel lifecycle dispatch once
the proof works.

| Operation | Contract |
|---|---|
| `sessions.list/status` | Existing session snapshot, plus active run ID when present |
| `runs.start(session_id, request_id)` | Validate ownership and configuration; return accepted run ID or actual rejection |
| `runs.get(run_id)` | Return an immutable snapshot of this run, including terminal result and evidence availability |
| `runs.stop(run_id)` | Cancel only if this exact run is still active; never stop a later run of its session |
| `runs.evidence(run_id, evidence_id)` | Retrieve evidence belonging to this run, without accepting arbitrary filesystem paths |

Initial clients poll `runs.get` with a deadline. A client timeout means the
client stopped waiting; it does not imply completion or trigger cancellation.
Do not put a blocking wait in Lab's serialized command queue: that could block
the stop request needed to end the run. A future long-poll or subscription must
release the command executor and handle disconnects independently.

Within the bounded request-history window, retrying the same start request ID
returns the original acceptance/rejection; reusing it with different arguments
is an error. After that window, return an explicit expired-request error rather
than executing an ambiguous retry. A new intentional execution uses a new ID.
One bounded implementation uses app/owner-scoped increasing request numbers,
a retained high-water mark and the last 128 replies. A number below that mark
whose reply was evicted is expired, not new. New submissions must increase the
number; retries can reuse a retained one. Retiring an owner epoch invalidates
its requests. This avoids an unbounded set of previously seen UUIDs.

Stop checks the run identity atomically with the lifecycle operation. Repeating
stop for a stopping or terminal run is harmless and returns that run's status.
No request may fall back to the selected session or the session's newest run.
Unknown, expired and foreign-app IDs have explicit errors. Preserve Lab's owner
checks for every operation; a run ID is not authorization or a new owner lease.
The first slice retains today's lease policy and does not add takeover.

## Lifecycle publication

Add a small typed run model and a bounded result store near the framework's
session lifecycle. ProgramSession publishes lifecycle facts; the store retains
value snapshots after a session is unloaded. Integrations serialize the model.
The framework must not depend on Lab command parsing, JSON transport or games.

The implementation needs an outer execution boundary covering validation in
the worker, downloads, environment construction, execution, cancellation and
early returns. Every accepted run must reach exactly one terminal result on
all supported in-process exit paths. Task-dispatch failure must also finalize
an already accepted run, or roll back acceptance before it becomes observable.
A scope finalizer can enforce this, but its fallback must not invent success.

Preserve existing notification behavior while recording outcomes at the actual
execution boundary. Do not translate every `report_error()` callback into a
terminal failure. Retain stats-save and evidence-write errors as diagnostics
without replacing the primary execution error.

Publish terminal status after program execution and required cleanup cease,
before another run can replace the active-run slot. Required failure-frame
ownership must already be secured. If image encoding is asynchronous, expose
`pending`, `ready`, `unavailable` or `expired` explicitly; terminal execution
does not promise that disk encoding has completed.

Cancellation requests do not overwrite an observed execution failure. A normal
return racing with a late stop remains completed; an actual cancellation exit
is cancelled. Serialize finalization so only one outcome wins. Ordinary setup
and download failures are failed; a deliberate cancellation during them is
cancelled. An app crash is outside the initial guarantee: in-memory history is
lost, and a missing app must never be reported as a completed run.

## Failure evidence

Prefer the screenshot carried by the exception, retaining its owning image
object rather than an ImageView or VideoStream pointer. Record its source,
console identity when known, dimensions and capture timestamp when available.
An unavailable timestamp stays unknown. Do not substitute the encoding time.

For errors without a frame, a console-aware boundary may attempt a bounded
snapshot before recovery changes the screen. Label it `post_failure_capture`;
it is not necessarily the failing frame. A generic session may have no camera,
several consoles, or a disconnected stream: missing evidence is an explicit
result, and capture failure must not mask the original error.

Keep a bounded run-scoped diagnostic tail at the logging boundary, tagging
entries while the run is active. Do not scrape the last lines of the global
log or include unrelated sessions. If propagation through background work is
not available, label that log coverage incomplete. The primary error and its
owned frame are the minimum first proof; broader log capture may follow.

For the Sandgem example, publish the actual OCR error and exception frame.
The agent can compare the visible banner with the rejection, then investigate
the matcher through existing CFR and saved-image tests. If AutoStory catches
the error and retries internally, the session sees one continuing run. Capturing
every checkpoint attempt requires a later typed progress/failure hook before
recovery; it is not obtained for free from ProgramSession.

## Lifetimes, locks and resource limits

Before a live adapter is considered supported, audit registration, lookup,
shutdown and derived-session destruction together. Reject new operations once
shutdown begins, detach observers safely, cancel/join execution while derived
resources are valid, then unregister. Requests must hold a defined lifetime
guard or be marshalled through an owner that provides that guarantee. Keeping
a raw pointer after releasing the tracker lock is insufficient.

Use short critical sections for run snapshots and transitions. Do not encode
images, write files, wait for workers, or invoke external callbacks while
holding the global tracker or result-store lock. Document lock ordering with
ProgramSession; existing listeners can run under its lock, so callbacks must
not re-enter it or acquire locks in the reverse order.

Choose fixed initial limits: at most 32 retained terminal records, 64 KiB of
diagnostic text per run, 64 MiB of retained encoded evidence app-wide, and two
pending image encodes. Keep active records; evict oldest terminal evidence and
then terminal records. Bound queued raw images separately by bytes before
enqueueing. Oversized frames or a full queue yield explicit unavailable
evidence, never unbounded allocation or a blocked stop operation. These are
implementation defaults to measure during the proof, not new UI settings.

Evidence is stored under an app-managed directory and retrieved by opaque ID.
Define deletion with eviction and shutdown; report expired evidence explicitly.
Do not return arbitrary paths as a general file-reading capability. Durable
history across restarts and crash recovery are deferred.

## Implementation slices and validation

1. **Typed lifecycle results.** Add run identity, bounded history and exactly-once
   finalization at ProgramSession. Native C++ tests exercise rejection, immediate
   completion, setup/download failure, cancellation, dispatch failure, and a
   second run preserving the first result. Cover execution paths with scripted
   actions rather than requiring a Switch.
2. **Lab operations.** Route start/get/stop through the shared service; preserve
   existing commands and defaults. Test duplicate request IDs, timeout without
   cancellation, stale-run stop while a later run is active, ownership rejection,
   unload races and shutdown. Audit lifetime and lock ordering in this slice.
3. **Failure evidence.** Retain the exception image and expose evidence lookup.
   Test missing frames, failed encoding, quota eviction and error preservation.
   Prove the result remains associated with run A after run B begins.
4. **One live checkpoint demonstration.** Under separately authorized console
   use, run a short checkpoint with a controlled failure and an owned frame.
   Retrieve outcome/error/frame through Lab, start a second run, and retrieve
   the first result again. Use Sandgem if the relevant setup is available;
   do not require recreating the historical OCR failure to validate correlation.

Keep each slice reviewable and run the applicable native tests and portable
builds. No live-console validation is claimed by this design or the current POC.

The decision gate is the demonstrated run-A/run-B investigation, including a
stale stop that cannot affect B. If it works, absorb the prototype dispatcher
into the Lab service and remove duplicate lifecycle routes. Only then consider
MCP as another adapter, typed checkpoint/attempt events, bounded progress
history or optional durable storage. A second transport alone is not the
reliability improvement being evaluated.
