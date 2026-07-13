# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & dev commands

```bash
make clean && make          # full rebuild with warnings (-Wall -Wextra -Wpedantic)
./digital-wellbeing         # test run (currently dumps config + 5s idle poll)
```

Other useful commands:
```bash
loginctl user-status                        # session info, idle hints
wayland-info                                # list compositor globals (Wayland)
gdbus introspect --session --dest org.kde.KWin --object-path /ScreenSaver  # KWin screen saver API
```

## Architecture

C23 (GNU23) daemon for Arch Linux. Single binary with two modes:
- **Daemon mode** (default): systemd user unit, forks off, runs the pomodoro-like state machine
- **CLI mode** (`status` / `today`): talks to the daemon over a Unix socket at `~/.cache/digital-wellbeing.sock`

Config is JSON at `~/.config/digital-wellbeing/config.json`, auto-generated on first run via `json-c`.

### State machine

```
WORKING(25min) → NOTIFIED → MONITORING(3min, 30s idle threshold) → LOCKED(5min) → WORKING
```

If user unlocks during LOCKED, notification spam (default 3-5s interval) or persistent notification fires until the 5 minutes elapse.

### Idle detection: dual backend, runtime-select

`src/idle.c` picks backend via `$XDG_SESSION_TYPE`:
- **X11**: `XScreenSaverQueryInfo` — works reliably
- **Wayland**: `ext-idle-notify-v1` protocol XML vendored in `protocols/`, compiled with `wayland-scanner`
  - **⚠ Known issue**: KWin (Plasma 6) does NOT advertise `ext_idle_notifier_v1` in its globals. So the Wayland backend will fail to init on KDE. Resolution TBD.

### Lock screen: desktop-aware matching

`src/lock.c` (not yet implemented) will identify the DE (`$XDG_CURRENT_DESKTOP`, process scan) and pick the right lock method from a lookup table. Unknown DEs fall back through loginctl → D-Bus → user-configurable command → +20s monitoring grace.

### Build flags

- `pkg-config` pulls in: `json-c`, `dbus-1`, `xscrnsaver`, `x11`, `xext`
- Wayland is conditional: only linked when `protocols/ext-idle-notify-v1.xml` exists and `wayland-scanner` is present. Defines `HAVE_WAYLAND` preprocessor macro.
- `wayland-protocols` package is a build-time dep for the XML protocol definitions (not yet installed on the dev machine)

## Current status (2026-07-13)

- ✅ Phase 1: clang-format, Makefile, README
- ✅ Phase 2: JSON config (json-c), auto-generate defaults
- 🔄 Phase 3: Idle detection — X11 backend works, Wayland code compiles but KWin doesn't implement ext-idle-notify-v1. See `PROGRESS.md` for detailed investigation log.
- ⏳ Phase 4-8: desktop detection, lock screen, notifications, state machine daemon, CLI, PKGBUILD — pending.

## Coding conventions

Allman brace style, 4-space indent, 80-col, `*` binds to variable name. Enforced by `.clang-format`. MixedCase for functions. Public API in `*.h` files documented with block comments.