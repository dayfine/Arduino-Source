# AutoStory: save policy by phase

Agreed with the user 2026-11-10, after FRLG 00.31 reported a false pass and its
checkpoint save overwrote 00.31's start save (one save slot on console B).

A segment (checkpoint) is always in one of three phases. The phase decides when a
run may save. `segments.tsv` next to this file holds each segment's phase.

| Phase | When | Runs | Save |
|---|---|---|---|
| **testing** | new segment, or code for it changed | one run, then a separate postcondition check | **never**; the start save stays |
| **verification** | testing run passed its postcondition check | 2-3 runs, each from the same start save (reload between) | **once, after every run passed** its postcondition check, at the end state |
| **reproduction** | the segment is verified (BDSP playthrough now) | full playthrough | at each checkpoint, as the playthrough goes |

Rules:
- A failed run never saves, in any phase. The console keeps the start save.
- **Postcondition check** is its own step after the run, not the run's own
  verdict: position (map + tile + facing by shot), bag items the segment
  picks up, flags that moved (a trainer gone, a door open), party state. The
  segment's `end` in its plan is the position check; items and flags are
  listed per segment in `segments.tsv` until the plan carries them.
- **Precondition check** before every run: the console's save is this
  segment's start (shot + map), and the items/flags it expects before it
  (the previous segment's postconditions) hold.
- A segment moves testing -> verification -> reproduction only on passes;
  any code change to it moves it back to testing.
- Starting segment N+1 needs N in verification-passed (saved) or
  reproduction. Never start N+1 from a hand recovery.
- Until the run options carry the phase, set the save option by hand per run
  and write the phase in the run notes line.

Code: the AutoStory run options get a save mode (`none` / `after_verification` /
`per_checkpoint`) and a postcondition-only action; see the follow-up PR.
