# MgbaLink v1

The protocol between SerialPrograms (or the smoke client) and `bridge.lua` running inside mGBA. Design and
rationale: [docs/design/Lab-mGBA-Console.md](../../docs/design/Lab-mGBA-Console.md), section 1.

## Transport

- One TCP connection per mGBA instance, on `127.0.0.1`. The bridge tries port 47801 first and then the
  next 9 ports, and prints the one it uses in the scripting console. A second client gets `ERR busy` and
  is closed.
- Line based: each command is one UTF-8 line ending in LF (a CR before the LF is ignored), and each
  command gets exactly one reply line. The bridge drops a client whose line is longer than 4096 bytes.
- The bridge reads the socket once per emulated frame (mGBA's socket frame hook). It also reads it
  continuously while it stalls on a grant, so commands sent during a stall are answered at once.
- An unknown command gets `ERR unknown <cmd>`, bad arguments get `ERR usage ...`, and a command sent
  before a game is loaded gets `ERR no-game`. A Lua error inside a command gets `ERR internal <text>`.
- Closing the connection (or `BYE`) releases every key the bridge holds, clears the grant, and lets mGBA
  run free with its own keyboard input.

## Frame numbers

`F` is `emu:currentFrame()`: the number of frames finished. While the bridge stalls, or while it answers
a command between frames, the emulator is about to run frame `F`. Each frame lasts
`1000 / 59.7275 = 16.743 ms`.

## Commands

| Command | Reply | Meaning |
|---|---|---|
| `HELLO 1` | `OK 1 <code> <version> <mode>` | `code` is the cartridge game code without `AGB-` (FireRed US: `BPRE`); `version` is the bridge's (`bridge-0.1`); `mode` is `paced` or `free`. A version other than 1 gets `ERR version 1`. |
| `GRANT <F>` | `OK <current F>` | Paced mode: the emulator may run until `currentFrame() == F`, then stalls in its frame callback until a later grant arrives. Free mode: accepted and ignored. Before the first grant, the emulator runs free. |
| `KEYS <F> <mask>` | `OK`, or `LATE <n>` | From frame `F` on, hold the GBA keys in `mask` (0..1023). A command for a frame that has already passed is applied at once, and the reply says by how many frames it was late. Commands queue in frame order. |
| `FRAME` | `FRAME <F> 240 160 <path>` | Writes the last finished frame as a PNG to `path` (default `$TMPDIR/mgba-bridge-<port>.png`, overwritten each time) through `emu:screenshot`. The path is the rest of the line and may contain spaces. 0.11 may later add `FRAME <F> <w> <h> <bytes>` followed by raw pixels; v1 on 0.10.5 never sends that. |
| `SAVE <file>` | `OK <F>` | `emu:saveStateFile(file, 31)`. The file is the rest of the line. `ERR save failed` on failure. |
| `LOAD <file>` | `OK <F>` | `emu:loadStateFile(file, 31)` (flag 31 includes SAVEDATA; mGBA's load default of 29 does not). Clears queued `KEYS`. With a grant active, the bridge holds at the loaded frame until the next `GRANT`. |
| `RESET` | `OK` | `emu:reset()`. Clears queued `KEYS`; with a grant active, holds at the new frame. |
| `BYE` | `OK BYE` | The bridge replies, then closes the connection and releases keys and grant. |
| `STATUS` | `OK frame=<F> keys=<getKeys> read_frame=<F> read_keys=<mask> applied_frame=<F> applied_mask=<mask> grant=<F> mode=<mode>` | Diagnostic for the smoke client; the app must not depend on it. `read_*` is `emu:getKeys()` right after the bridge set the keys in the last `keysRead` callback; `applied_*` is the last time the held mask changed. A field that is not known yet prints `-`. |

## Key mask

GBA KEYINPUT bit order, as `C.GBA_KEY`: A=1, B=2, Select=4, Start=8, Right=16, Left=32, Up=64, Down=128,
R=256, L=512.

`KEYS` is the whole controller: it sends absolute key states, never press or release events, so a lost or
late command cannot leave a key stuck. While a client has sent `KEYS`, the bridge sets the held mask in
every `keysRead` callback, overriding the keyboard in the mGBA window.

## Paced lockstep

The client sends `GRANT F0 + floor((now - t0) * 59.7275 / 1000) + 2` about every 8 ms (design section
2), and schedules every `KEYS` ahead of the grant, so each one lands on the frame it names. The stall
loop waits on the client socket 1 ms at a time. A stall longer than 30 s with no new grant drops the
grant (logged as a warning), and the emulator runs free until the next `GRANT`.
