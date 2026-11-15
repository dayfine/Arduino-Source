# BDSP Starter RNG frame-pipeline performance plan

## Status and scope

This document describes the performance work needed on top of the BDSP Starter RNG adoption in fork PR
[#526](https://github.com/dayfine/Arduino-Source/pull/526) (`bdsp/starter-rng-adopt`). The measurements below
were taken from commit `122697311aff6fef3e3492da16ebdecabda2d7c2` on an Apple M2 Pro with a 1920 x 1080 NV12
capture stream. This PR intentionally contains no production-code changes.

The goal is to make visual blink recovery reliable on current hardware without changing the RNG model, weakening
the state constraints, or requiring users to disable Stream History manually. The immediate work is to remove
avoidable contention and duplicate inference. More invasive image-pipeline changes should be attempted only if live
measurements show that those changes are still necessary.

## Current pipeline

The capture device was configured for 60 FPS, but delivered about 30 FPS in the measured runs. Each source frame can
currently reach two independent CPU-heavy paths:

```text
QVideoFrame (1920 x 1080 NV12; about 30 source FPS)
|
+-- Stream History
|   +-- map/toImage and convert to RGB888
|   +-- smooth-scale to 720p
|   +-- convert RGB to BGR
|   +-- JPEG-encode at a requested 30 FPS
|
+-- SnapshotManager
    +-- convert the complete frame to ARGB/RGB32
    +-- cache the converted snapshot
        +-- Player eye ROI -> grayscale -> template match
        +-- Barry eye ROI  -> grayscale -> template match
```

The two eye regions together cover about 0.18% of the source image, but `SnapshotManager` must first convert the
entire 1080p frame. That conversion is shared by Player and Barry when their callbacks run back-to-back; it is not
performed once per eye. However, each watcher is scheduled every 16 ms and can run again on the same cached snapshot,
so the two template matches and observation append can be repeated for a frame that has not changed.

Relevant implementation points are:

- `PokemonBDSP_BlinkRecovery.cpp`: constructs the Player and Barry watchers with a 16 ms period.
- `PokemonBDSP_EyeBlinkDetector.cpp`: performs the ROI match and appends watcher observations.
- `VisualInferencePivot.cpp`: supplies the most recent cached snapshot without rejecting an unchanged timestamp.
- `SnapshotManager.cpp`: converts complete `QVideoFrame` objects and can have several conversions in flight.
- `StreamHistoryTracker_SaveFrames.cpp`: queues up to ten frames for scaling and JPEG compression on its worker.

## Evidence from failed live runs

The figures are aggregated from the existing periodic performance logs. "Analyzed FPS" is converted snapshots divided
by the logged intervals; it is not the callback rate.

| Metric | Failed attempt 1 | Attempt 2, cancelled |
|---|---:|---:|
| Measured source rate | 30.1 FPS | about 30.0 FPS |
| Converted snapshots | 13,711 | 5,722 |
| Effective analyzed rate | 13.6 FPS | 12.7 FPS |
| Weighted mean full-frame conversion | 59.1 ms | 64.3 ms |
| Maximum full-frame conversion | 2,524 ms | 1,856 ms |
| Stream History queue-drop messages | 4,886 | 3,281 |
| Watcher callbacks per watcher | 46,082 | about 19,989 |
| Outcome | no state after 156 blinks | same degradation; cancelled |

Before blink recovery became active, the same log showed about 30.0 source FPS, an 8.3 ms weighted conversion mean,
an 828 ms maximum, and 445 Stream History drops. The interaction therefore increases both average conversion cost and
long stalls substantially.

For attempt 1, each watcher ran about 44.9 times per second while only about 13.6 distinct converted snapshots per
second became available. Comparing callback and conversion counts indicates that approximately 70% of watcher calls
reprocessed a cached frame. This percentage is derived rather than directly measured, which is why adding explicit
unique/duplicate counters is part of the plan.

The RNG solver is not the observed CPU bottleneck. The failure starts earlier: conversion stalls create multi-frame or
multi-second gaps in the blink stream, while repeated callbacks can add duplicate samples and timestamps. The solver
then receives an incomplete or distorted observation sequence and cannot find a state.

## Primary design: scoped Stream History video pause

Blink observation should programmatically suspend the expensive video side of Stream History. The pause must be a
runtime scope, not a mutation of `GlobalSettings::STREAM_HISTORY` or the user's persisted Enabled setting.

The proposed program-facing shape is a move-only RAII token returned through `VideoStream`, backed by
`StreamHistorySession`. Exact names may change during implementation; the required semantics are:

1. Acquiring the first token prevents new video frames from entering the history tracker.
2. Acquisition discards queued, uncompressed frames and waits for any compression already in flight to finish. Blink
   timing does not start until the competing conversion/compression work is quiescent.
3. Already-compressed history remains available. Audio history may continue because it is not the measured source of
   contention.
4. Nested users are safe: a reference count resumes video history only after the last token is destroyed.
5. Destruction resumes video capture on normal return, cancellation, or exception, without relying on a cleanup call
   at every exit point.
6. Resume re-anchors the history frame schedule so it does not iterate over every skipped 30 FPS interval or delay the
   first post-pause frame.
7. Logging is one message at pause and resume, with an aggregate count of discarded frames. It must not emit one log
   message for each rejected or dropped frame.

`VideoStream` is the correct public boundary because Pokémon programs already receive it and should not depend on Qt,
the concrete tracker, or global settings. `StreamHistorySession` is the correct owner of pause state because it owns
the current tracker across video resets. The tracker must implement queue discard and worker quiescence because only
it can distinguish queued work from compression already in flight.

The pause should cover every period in which visual blink watchers are active, including initial recovery and any
hold/re-anchor observation. A small BDSP blink-observation scope should own both the pause token and watcher lifetime
if that avoids acquiring and releasing the pause between adjacent phases.

### Alternatives not selected for the first implementation

- Changing the persistent Stream History setting surprises the user and is difficult to restore safely after every
  cancellation and exception.
- Merely detaching the frame listener leaves up to ten queued frames plus one in-flight compression job competing with
  the start of blink recovery.
- Reducing history FPS helps but does not guarantee that no compression overlaps a timing-sensitive observation.
- Direct NV12-luma ROI extraction could remove most full-frame conversion work, but it is a larger, format-specific
  change and should be justified by measurements after contention and duplicate work are removed.

## Independent fix: analyze each frame once

Each `EyeBlinkWatcher` should remember the last processed snapshot timestamp. If a callback receives that timestamp
again, it should return before template matching and before appending an observation. The 16 ms scheduling interval can
remain: it lets the pivot notice a newly converted source frame promptly, while timestamp suppression makes the work
source-frame-driven in practice.

Player and Barry must still analyze the same fresh snapshot. A later cleanup may group both eye matches into one
callback to make that invariant explicit, but grouping is not required to remove cached-frame repeats. End-of-session
diagnostics should report unique snapshots, skipped duplicates, longest inter-frame gap, and matcher time without
logging every sample.

## Implementation sequence

### 1. Fresh-frame gating and measurement

- Add timestamp suppression before the eye template match and observation append.
- Add aggregate counters for callbacks, unique frames, duplicate skips, and longest gap.
- Add a focused test that sends repeated timestamps followed by a new timestamp and verifies that only the two unique
  frames produce matches/observations.
- Keep RNG solving and blink classification unchanged.

This is a small BDSP-local change and provides trustworthy measurements for the remaining work.

### 2. Shared scoped-pause primitive

- Add the move-only pause token at the `VideoStream`/`StreamHistorySession` boundary.
- Gate new video frames at the session before they reach the tracker.
- Add tracker operations to clear the pending queue, wait for an in-flight compression to finish, and re-anchor frame
  scheduling on resume.
- Make pause state reference-counted and safe across tracker replacement caused by video reset.
- Test normal destruction, nested scopes, cancellation-style unwinding, pending-queue discard, in-flight quiescence,
  preservation of compressed history, and resume.

Worker tests will need a controllable compression barrier or injected compression function. Sleeping and hoping to
hit an in-flight encode would make the concurrency test flaky.

### 3. BDSP integration

- Acquire the pause before starting blink watchers and release it only after the final watcher stops.
- Ensure recovery, re-anchor, early return, cancellation, and exception paths all use the same RAII lifetime.
- Log a concise before/after performance summary for live validation.
- Do not change solver inputs, thresholds, or state-confirmation policy in this slice.

### 4. Live A/B validation

Run the same Starter RNG flow on the measured M2 Pro/capture-card setup with Stream History enabled in settings. Compare
source FPS, unique analyzed FPS, conversion latency, duplicate count, longest frame gap, and solver outcome against the
failed-run baseline above.

The first implementation is acceptable when:

- duplicate timestamps produce zero matcher invocations and zero observations;
- no Stream History video compression or queue-drop messages occur inside the pause;
- completed pre-pause history is still saveable and video history resumes after normal completion and cancellation;
- no persistent Stream History option changes;
- unique analyzed rate remains at least 90% of the measured source rate over a full recovery attempt;
- after warm-up, full-frame conversion has a weighted mean below 15 ms and no conversion stall above 250 ms;
- three consecutive fresh program starts recover and confirm a state on the target setup.

The timing thresholds are validation targets, not user-visible contracts. If the source itself falls below its current
30 FPS baseline, diagnose capture delivery separately rather than hiding that regression in watcher logic.

### 5. Only if the acceptance targets still fail

Instrument `SnapshotManager` queue depth and completion order, then make it single-flight/latest-only so obsolete
full-frame conversions cannot accumulate. After that, prototype a gray/luma ROI path that maps only the two eye regions
from formats with a directly usable luminance plane, with a safe fallback to the existing QImage conversion for other
formats. Each step should have a separate before/after measurement; neither belongs in the first fix by default.

## Unknowns to resolve during implementation

- How much conversion latency remains after Stream History is quiescent and duplicate matches are removed.
- Whether multiple in-flight `SnapshotManager` conversions are responsible for the remaining long-tail stalls.
- Whether the capture backend provides stable, portable access to the NV12 Y plane on every supported platform.
- The minimum unique-frame cadence and maximum tolerable gap for reliable blink classification; current evidence only
  proves that roughly 13 FPS plus multi-second stalls is insufficient.
- Whether pausing only video history is sufficient on configurations where audio history is active.
- Whether a Stream History save requested during a pause needs an explicit UI indication that the video contains a
  deliberate gap.

## Expected PR stack

This document is the first, documentation-only child of fork PR #526. Code should follow in reviewable slices:

1. BDSP fresh-frame gating and cadence instrumentation.
2. Shared scoped Stream History video pause, including deterministic worker tests.
3. BDSP pause integration and live validation results.
4. `SnapshotManager` or luma-ROI work only if the recorded acceptance targets still fail.

Keeping the first two code changes separate makes it possible to measure the value of each and keeps common-framework
concurrency work out of the BDSP detector review.
