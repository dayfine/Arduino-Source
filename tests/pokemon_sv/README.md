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
