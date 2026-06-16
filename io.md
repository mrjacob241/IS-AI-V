# io.v

`io.v` is the dedicated memory-mapped I/O chip for simulation-facing input.
It is separate from `gpu.v`; graphics owns pixels and scanout, while `io.v`
owns keyboard-style input state and typed-character delivery.

## Memory Map

`io.v` registers live at `0x00050000`.

| Address | Access | Meaning |
|---------|--------|---------|
| `0x00050000` | read/write | Current key-state bitmask |
| `0x00050004` | read/write | Current typed character, low 8 bits |
| `0x00050008` | read/write | Typed-character sequence number |
| `0x0005000C` | read/write | Typed-character acknowledge sequence |

Current key-state bits:

| Bit | SDK name | Meaning |
|-----|----------|---------|
| 0 | `IO_KEY_W` | W key / up |
| 1 | `IO_KEY_S` | S key / down |
| 2 | `IO_KEY_Q` | Q key / quit |

## Typed Character Stream

The host testbench queues typed characters and publishes one character at a
time through `stream_char` / `stream_seq`.

Bare-metal software reads `stream_seq`. If it differs from the last seen
sequence, it reads `stream_char`, then writes that same sequence number to
`stream_ack`. The host publishes the next queued character only after seeing
the acknowledge update.

This prevents fast input such as `12<Enter>` from overwriting characters before
the simulated CPU has consumed them.

## Bare-Metal SDK

Programs can include `IO_SDK.h`. `codes/build_riscv.sh` automatically links
`IO_SDK.cpp` when that include is present.

```cpp
#include "../../IO_SDK.h"

Keyboard.Update();

if (Keyboard.KeyPressed(IO_KEY_W)) {
    // key is currently held/latching
}

if (Keyboard.KeyDown(IO_KEY_W)) {
    // transitioned from up to down since previous Update()
}

if (Keyboard.KeyUp(IO_KEY_W)) {
    // transitioned from down to up since previous Update()
}

char buf[2];
if (Keyboard.KeysStream(buf, sizeof(buf)) > 0) {
    // buf[0] is the next typed character
}
```

## Host Bridge

`pong_input_io.cpp` is the Verilator host bridge. It puts the terminal into
raw nonblocking mode, reads stdin on a background thread, queues typed
characters, and applies the latest state to `io0` from the simulation thread.

`PongInputIO(false)` disables host-side `Q` quit handling so programs like
`calc` can receive `q` as normal input. `PongInputIO(true)` keeps `Q` as a
host quit button for live Pong.
