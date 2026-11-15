# lab/mgba: mGBA bridge (slice 0)

Slice 0 of [docs/design/Lab-mGBA-Console.md](../../docs/design/Lab-mGBA-Console.md): a Lua bridge
that runs inside the official mGBA 0.10.5 and speaks [MgbaLink v1](PROTOCOL.md) on a localhost
socket, plus a Python smoke client.

| File | What |
|---|---|
| `bridge.lua` | The bridge. Only uses the 0.10 scripting API (sockets, `frame`/`keysRead` callbacks, `emu:*`). |
| `PROTOCOL.md` | MgbaLink v1: commands, replies, frame numbers, key mask. |
| `mgba_link.py` | Client library and CLI: `hello`, `status`, `smoke`. Takes a host and a port, never a ROM path. |
| `test_mgba_link.py` | Client tests against an in-process fake bridge (no mGBA, no ROM). |
| `bridge_test.lua` | Bridge tests against stub mGBA globals (no mGBA, no ROM). |

## Step 0 (once per mGBA launch, by hand)

1. Open `/Applications/mGBA.app` and load the local FireRed ROM (File > Load ROM).
2. Tools > Scripting, then File > Load script, and pick `lab/mgba/bridge.lua` from a checkout of `agents`.
3. The scripting console prints `[mgba-link] MgbaLink v1 ... listening on 127.0.0.1:47801`.

To load a changed `bridge.lua`, use File > Reset in the Scripting window first. Loading it again
without the reset starts a second bridge on the next port.

## Smoke run

```sh
python3 -I lab/mgba/mgba_link.py hello      # OK 1 BPRE bridge-0.1 paced
python3 -I lab/mgba/mgba_link.py smoke      # Markdown checklist of the slice-0 checks
```

`smoke` holds the game with a `GRANT`, saves a state to `~/FRLGLab/mgba/smoke/slice0.ss` (local, never
committed), presses B for one frame, and loads the state back, so the game ends where it started.
Run it on a title or intro screen, not over an in-game save.

## Tests

```sh
python3 -I -m unittest discover -s lab/mgba -p test_mgba_link.py
lua lab/mgba/bridge_test.lua                # Homebrew lua 5.4
```

## ROM rule

Nothing here reads, copies, hashes or names the ROM. Save states and frames stay on the local disk
(`~/FRLGLab/mgba/`, `$TMPDIR`) and are linked by path, never committed.
