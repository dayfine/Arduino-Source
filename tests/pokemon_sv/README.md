# BBQ battle-entry replay

Run from the repository root with Python 3 and a Clang/GCC-compatible C++23 compiler:

```sh
python3 -B -m unittest discover -s tests/pokemon_sv -v
```

`CXX` can select the compiler. The test compiles a temporary executable and removes
it afterward. It does not use the application profile, controller or capture card.

The replay extracts the inner entry check and the post-entry dispatch block from
the actual Tera-self and wild-Tera quest functions. Scripted detector results feed
the production check; recording stubs count battle, capture, recovery and stop
calls. This avoids replacing the production guard with a separate model of it.

Nine scenarios exercise both routes: timeout, outer visual confirmation, inner
confirmation before the outer command finishes, both confirmations together,
nonvisual/unknown callback indices, shiny stopping with and without confirmation,
and the existing missing-audio error. The original code fails six assertions
because unconfirmed encounters still invoke the battle handler.

This is a focused control-flow regression check. It does not exercise the real
inference scheduler, controller command queue, video/audio detectors or navigation.
The modified C++ translation unit must also compile, and success/timeout paths
still need a Switch smoke check. The extraction markers intentionally fail if the
quest functions are reorganized; update this test alongside such a refactor.

## Recorded validation (2026-10-20)

All nine scenarios passed for both routes (18 route/scenario combinations). The
original dispatch failed six assertions: timeout, nonvisual callback and unknown
callback each incorrectly dispatched one battle per route; the guard dispatches
zero. Confirmed entry dispatches exactly once, even when both watchers confirm.

The full macOS arm64 app built successfully. A separate temporary hardware runner
called the unchanged production routes directly; the 5m07s Switch trial finished
normally:

- Tera-self: menu confirmed at 21:32:22.368 PDT, followed by `Turn 1: Tera` and
  battle inputs. Full recovery returned normally.
- Wild Tera: the 15-second menu wait timed out at 21:35:00.932 PDT. The new guard
  skipped battle handling and immediately started recovery. Recovery detected a
  late battle 3.556 seconds later, fled, and detected overworld 9.073 seconds after
  that confirmation. Full recovery returned normally.

This covers confirmed entry on Tera-self and timeout/late battle on wild Tera
live. The opposite combinations are covered by replay tests only. The late battle
means this run does not establish an absent target. No before/after runtime or
long-run reliability rate was measured. The temporary runner was removed and the
regular app rebuilt afterward; it is not part of this change.
