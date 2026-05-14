# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

A fork of [Omnispeak](https://github.com/sulix/omnispeak) (a Commander Keen 4/5/6 source port) that adds Archipelago multiworld randomizer support. Active development is on the `ap-client` branch. This continues `kodbyte/omnispeak-ap` (archived). The upstream Omnispeak engine code is unmodified-in-spirit ("pixel-perfect, bug-for-bug clone of the original"); AP integration is layered on top.

## Build / run

The canonical build system is `make` from `src/`. CMake exists for MSVC but the Makefile is what CI and releases use.

```sh
# One-time: fetch pinned asio + nlohmann-json into third_party_pinned/.
# Requires curl + tar on PATH. Skips if already present.
make -C src setup

# Native build → bin/omnispeak
make -C src

# Useful variants
make -C src DEBUG=1            # -O0 -g -DCK_DEBUG
make -C src RENDERER=null      # headless (used by demo dump tests)
make -C src VANILLA=1          # disables omnispeak-only extras
make -C src NO_SSL=1           # drops OpenSSL+zlib link deps, breaks wss://
make -C src PLATFORM=win64     # MinGW cross to 64-bit Windows
make -C src dumpprinter        # builds the dump-diff helper
make -C src help               # full option list
```

System deps: macOS `brew install sdl2 openssl@3`; Debian/Ubuntu `apt install libsdl2-dev libssl-dev zlib1g-dev`. The Darwin branch of the Makefile auto-adds `$(brew --prefix)/opt/openssl@3` to include/lib paths.

Build outputs are platform-suffixed: `bin/`, `obj/` for native; `bin-win64/`, `obj-win64/` for cross, etc. `make distclean` wipes them.

`bin/` is the runtime layout — the binary needs `connection.txt`, `cacert.pem`, the user-supplied `*.CK4`/`*.CK5` game files, and the data files copied from `data/keen{4,5,6e14,6e15}/` (the Makefile copies these as part of `binfiles`).

## Tests

The only automated test is a demo-replay regression check: build with `RENDERER=null`, replay the four `tests/demoN.dumpX` files, and diff against expected output.

```sh
make -C src RENDERER=null DEBUG=1
make -C src dumpprinter
cd bin
../tests/testdump.sh 0 4        # demo 0, episode 4
../tests/testdump.sh 0 5        # demo 0, episode 5
../tests/testdump.sh 0 6v15     # demo 0, episode 6 v1.5
```

Tests need proprietary Keen 5/6 data files in `bin/`. CI fetches them from a private URL via the `k56dataUrl` repo secret and skips the job entirely on forks where the secret isn't set (`.github/workflows/tests.yml`). Keen 4 shareware data is fetched from davidgow.net.

When a dump diverges the script writes printable diffs to `log/`.

## Architecture

### Engine vs AP layers

Omnispeak (the upstream engine) lives in `src/` as `ck_*.c` (Commander Keen game logic: maps, objects, physics, episodes) and `id_*.c` (id Software-style subsystems: `id_ca` cache manager, `id_mm` memory, `id_rf` refresh, `id_vl` video, `id_sd` sound, `id_in` input, `id_us` user/menu, `id_vh` video helper, `id_ti` titles, `id_fs` filesystem, `id_cfg` config). Episode-specific code is partitioned by prefix: `ck4_*`, `ck5_*`, `ck6_*`. Each episode can be compiled in/out with `WITH_KEEN4`/`5`/`6`.

The AP layer is three files plus a launcher:

- `ap_client.cpp` — the only C++ TU. Wraps `apclientpp` (vendored under `src/third_party/apclientpp`) and the websocket stack. Owns the connection, item/location state, slot data, and the polling loop. Exposes a C API via `ap_client.h`.
- `ap_hooks.c` / `ap_hooks.h` — C-side glue called from engine code. Holds globals like `ap_current_level`, `ap_current_episode`, `ap_has_pogo`, `ap_death_link_enabled`. Implements:
  - on-event callbacks (`ap_on_level_complete`, `ap_on_keygem_get`, `ap_on_security_card_get`, `ap_on_death`) which translate game events into AP location checks via `LOC_*` macros in `ap_defs.h`,
  - DeathLink send/receive with `ap_suppress_death_send` to break echo loops on incoming kills,
  - the in-game toast system (`ap_toast_push` / `ap_toast_tick` / `ap_toast_draw`) used for item-send, item-receive, and DeathLink notifications.
- `ap_defs.h` — location ID encoding (`LOC_LEVEL_COMPLETE(ep, lvl)`, `LOC_KEYGEM(ep, lvl, gem)`, `LOC_SECURITY_KEYCARD(ep, lvl)`, `LOC_POINTSANITY(ep, lvl, tier)`) and the `AP_ITEM_*` enum. Location IDs are derived from base + episode-stride + level-stride; the AP world definition on the server side must agree on this layout.
- `ap_launcher.c` — small Win32 GUI that just spawns `omnispeak.exe /Episode <N>`. Built into `AP Keen Launcher.exe` for releases.

### Engine integration points

`ck_play.c` is the engine's main game loop and is the primary site of AP integration:
- `ap_client_poll()` and `ap_apply_pending_death()` run once per frame.
- `ap_on_death(ap_random_death_message(AP_DEATH_FELL))` etc. fire from death-handling paths.
- `ap_force_abort` redirects the CTRL+R "abort level" path through `LS_Died` without consuming a life.
- `ap_toast_tick()` is called inline; **toasts deliberately do not register as an `rf_drawFunc`** because doing so caused a black-screen regression on this build (see comment near line ~2505). The toast draw is invoked directly from `RF_Refresh`.

`ck_keen.c`, `ck4_obj*.c`, `ck5_obj*.c`, `ck_inter.c` and friends call the `ap_on_*` hooks at the relevant pickup / level-completion / score-change sites.

### Runtime files

The binary expects these in CWD at startup:
- `connection.txt` — server / port / slot name / password, plus optional toast-tuning keys (`toasts`, `toasts_received`, `toasts_sent`, `toasts_deathlink`, `toast_duration`). Booleans accept 0/1, true/false, yes/no, on/off (case-insensitive). Parsing is in `ap_load_connection_info` in `ap_client.cpp`.
- `cacert.pem` — CA bundle for the wss:// connection.
- `ap_log.txt` — written by the client (appended). Useful first stop for connection failures.
- `ap_uuid.txt` — generated per-server UUID for slot identity.

### Third-party / vendor layout

`src/third_party/` is checked in (header-only): `apclientpp`, `wswrap`, `websocketpp`, `valijson`, plus an incomplete `nlohmann/` and an `asio/` tree that's too new for the vendored websocketpp.

`third_party_pinned/` is gitignored and auto-fetched on first build:
- `asio-<ASIO_COMMIT>/` — standalone asio pinned to the commit apclientpp's CI uses, because the vendored websocketpp predates asio's removal of `io_service` / `expires_from_now`.
- `nlohmann-<NLOHMANN_VERSION>/` — single-header amalgamated json, because the vendored modular copy is missing internal headers.

Include order in the Makefile matters: pinned paths come before `-Ithird_party` (so amalgamated nlohmann wins over the broken vendored one) and before the brew prefix (so pinned asio wins over any system asio). Don't reorder casually.

`AP_NO_SCHEMA` is defined globally — valijson schema validation is compiled out.

### C vs C++

Almost everything is C. `ap_client.cpp` is the only C++ TU, and exposes a C-linkage interface in `ap_client.h`. On Linux/Unix the default is `BUILDASCPP=1` (compile the .c files with the C++ compiler too) so the C++ runtime is linked in automatically. The Windows/dos/gcw0 paths also force `BUILDASCPP=1` to ensure libstdc++ is pulled in for `ap_client.cpp`. If you change this, the link will silently drop the C++ runtime and fail at link time.

## Version control

This repo is colocated `jj` + `git` (a `.jj/` directory exists). Per the user's global rule, prefer `jj` commands (`jj commit`, `jj new`, `jj log`, `jj diff`, `jj bookmark set`) over their `git` equivalents. The `jj` skill handles translation. Use `git` only for things `jj` can't do (e.g. `gh` interactions).

The default branch for PRs is `ap-client`, not `main`/`master`.

## Things that bite

- **Don't ship a release with `NO_SSL=1`** — Archipelago servers normally require `wss://`. The flag exists for toolchains (djgpp, some CI smoke builds) that lack cross OpenSSL/zlib.
- **Don't reorder Makefile include paths** without re-reading the comments around `ASIO_DIR` / `NLOHMANN_DIR` — there are concrete reasons pinned paths come first.
- **Don't move `ap_toast_draw` to an `rf_drawFunc` registration** — the comment in `ck_play.c` warns this causes a black-screen regression.
- **`ap_force_abort` riding the `LS_Died` state** is load-bearing for the CTRL+R graceful-exit feature; treat it as intentional, not a bug.
- Location ID stride changes in `ap_defs.h` are a hard ABI break with the Archipelago apworld (`kodbyte/Archipelago-Keen`) — they must be coordinated.
