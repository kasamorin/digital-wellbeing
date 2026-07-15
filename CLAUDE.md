# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & dev commands

```bash
make clean && make          # full rebuild with warnings (-Wall -Wextra -Wpedantic)
./digital-wellbeing         # run daemon (forks, installs systemd service on first run)
systemctl --user status digital-wellbeing   # check daemon status
journalctl --user -u digital-wellbeing -f   # follow daemon logs
```

## Architecture

C + C++ hybrid daemon for Arch Linux. Single binary, KISS design:

- **Daemon mode** (default): forks off, runs the pomodoro loop
- **First run**: writes `~/.config/systemd/user/digital-wellbeing.service` and enables it
- Config is JSON at `~/.config/digital-wellbeing/config.json`, auto-generated on first run via `json-c`

### Main loop (src/main.cpp)

```
sleep(workMinutes - 5)  →  notify-send "5 minutes until break"  →
sleep(5)                →  breakWindowShow(breakMinutes)        →  loop
```

Signal handling: SIGTERM/SIGINT set `volatile sig_atomic_t gRunning = 0`, loop exits cleanly.

### Break window (src/breakWindow.cpp)

Qt6 fullscreen overlay (`Qt::WindowStaysOnTopHint`), Breeze-themed on KDE:

- Large countdown label (96pt), `MM:SS` format
- "Skip Break" button below
- Ctrl+Q shortcut (same effect as button)
- Notify-send at 5min remaining (once per break)
- Blocks via QEventLoop until dismissed, then returns control to the daemon loop

No moc needed — no `Q_OBJECT` macro, signals connected via function pointers.

### Config (src/config.c/h)

Only two fields:
```json
{
  "workMinutes": 25,
  "breakMinutes": 5
}
```

### Service self-install (src/service.c/h)

On first run, writes a built-in systemd user unit to `~/.config/systemd/user/digital-wellbeing.service` and runs `systemctl --user enable`. Skips if file exists.

### Build flags

| component | pkg-config |
|-----------|------------|
| config    | `json-c` |
| break window + main loop | `Qt6Widgets Qt6Gui Qt6Core` |

C files compiled with `gcc -std=gnu23`, C++ files with `g++ -std=c++23 -fpic` (`-fpic` required for gcc 16 + Qt6 protected symbol compatibility). Final link with `g++`.

## Removed (2026-07-15 redesign)

- **Idle detection** (X11 XScreenSaver, Wayland ext-idle-notify, /dev/input, D-Bus logind) — all deleted. KDE Plasma 6 Wayland had no working backend.
- **Lock screen** and **desktop detection** — replaced by the fullscreen break window.
- **Notification spam/persistent** — replaced by a single notify-send at 25→20min work mark + one during break at 5min remaining.
- **State machine module** — loop is simple enough to inline in main.cpp.
- **CLI** (Unix socket, `status`/`today`) — not needed, KISS.
- **Wayland protocol vendoring** (`protocols/ext-idle-notify-v1.xml`) — deleted.

## Coding conventions

Allman brace style, 4-space indent, 80-col, `*` binds to variable name. MixedCase for functions. C headers wrapped with `extern "C"` for C++ compatibility. Qt6 code follows Qt conventions (camelCase signals/slots, no `Q_OBJECT` macro needed).