# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & dev commands

```bash
make clean && make          # full rebuild with warnings (-Wall -Wextra -Wpedantic)
make pot                    # regenerate PO template from source strings
make mo                     # compile .mo files from .po (to ~/.local/share/locale)
make locales                # full i18n refresh: pot + po + mo
make install                # install binary + translations to ~/.local/
./digital-wellbeing         # run foreground with timestamped stderr logs
./digital-wellbeing --daemon  # fork to background (daemon mode)
./digital-wellbeing install   # write systemd user unit and exit
./digital-wellbeing --help    # show usage
systemctl --user status digital-wellbeing   # check daemon status
journalctl --user -u digital-wellbeing -f   # follow daemon logs
```

## Architecture

C + C++ hybrid daemon for Linux. Single binary, KISS design:

- **Foreground mode** (default): runs with timestamped stderr logs (`[HH:MM:SS]`), no fork
- **Daemon mode** (`--daemon`/`-d`): forks, redirects stderr to /dev/null
- **Help** (`--help`/`-h`): prints usage and exits
- **Service install** (`install` subcommand): writes systemd user unit, exits immediately — does NOT auto-install on startup
- `--fg`/`-f` is still accepted but deprecated (prints a warning: foreground is now the default)
- Config is JSON at `~/.config/digital-wellbeing/config.json`, auto-generated on first run via `json-c`

### Main loop (src/main.cpp)

```
log "Work period started"
sleep(workMinutes - 5)  →  notify-send "5 minutes until break time"  →
sleep(5)                →  log "Break period started"                →
breakWindowShow(breakMinutes) → log "Break ended" → loop
```

Signal handling: SIGTERM/SIGINT set `volatile sig_atomic_t gRunning = 0`, loop exits cleanly.

### Break window (src/breakWindow.cpp)

Qt6 fullscreen overlay (`Qt::WindowStaysOnTopHint`), Breeze-themed on KDE:

- Large countdown label (96pt), `MM:SS` format
- "Skip Break" button below (text translated via gettext `_()`)
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

`./digital-wellbeing install` subcommand writes a built-in systemd user unit to `~/.config/systemd/user/digital-wellbeing.service`. Does NOT run `systemctl --user enable` (prints instructions for the user). Skips if file exists.

The unit's `Description=` line is translated at write time via `_()`. `ExecStart` defaults to `/usr/bin/digital-wellbeing` (PKGBUILD install target). Users of `make install` (which installs to `~/.local/bin/`) must edit `ExecStart=` in the generated service file.

### Packaging (pkg/)

`pkg/` contains PKGBUILD and pre-built files for Arch Linux packaging (`makepkg`). The binary and `.mo` files are copied here after `make && make mo`, then `makepkg` installs them directly without recompilation.

- `PKGBUILD` — Arch packaging script (`options=('!debug')`, no debug package)
- `uninstall-user-data.sh` — cleanup script for user data after `pacman -R`

### Logging (src/log.c/h)

`logMsg(const char *fmt, ...)` — single function:
- Prepends `[HH:MM:SS]` timestamp
- Writes to stderr
- Format strings are translatable (wrapped with `_()` at call sites)
- In daemon mode, stderr is redirected to /dev/null (logs go to journald)
- `__attribute__((format(printf, 1, 2)))` for compile-time format checking

### i18n (src/i18n.c/h)

gettext-based internationalization:

- `_("string")` macro shorthand for `gettext("string")` (defined in `i18n.h`)
- `i18nInit()` called once at startup: sets locale from env, binds `digital-wellbeing` domain to `$HOME/.local/share/locale` (falls back to `/usr/share/locale`)
- `.po` files in `po/`, compiled `.mo` files installed alongside binary
- `make pot` → extract strings via `xgettext --keyword=_`
- `make mo` → compile `.po` → `.mo` via `msgfmt`
- No extra link flags needed — libintl is in glibc
- **Locale priority**: GNU gettext uses `LANGUAGE` env var first, then `LC_ALL`/`LC_MESSAGES`/`LANG`. If `LANGUAGE=zh_CN:en_SG` is set (common on multi-lang systems), it overrides `LANG=en_US`.

All user-facing strings go through `_()`. Not translated: config JSON keys, CLI args, `perror()` arguments, countdown format.

### Build flags

| component | pkg-config |
|-----------|------------|
| config    | `json-c` |
| break window + main loop | `Qt6Widgets Qt6Gui Qt6Core` |
| i18n/log  | none (libintl in glibc) |

C files compiled with `gcc -std=gnu23`, C++ files with `g++ -std=c++23 -fpic` (`-fpic` required for gcc 16 + Qt6 protected symbol compatibility). Final link with `g++`.

## Coding conventions

Allman brace style, 4-space indent, 80-col, `*` binds to variable name. MixedCase for functions. C headers wrapped with `extern "C"` for C++ compatibility. Qt6 code follows Qt conventions (camelCase signals/slots, no `Q_OBJECT` macro needed).

i18n: all user-facing strings wrapped with `_("...")`. `_()` macro available via `#include "i18n.h"`. Log messages use `logMsg(_("..."), ...)` pattern — format string is translated before logging.

## Removed (2026-07-15 redesign)

- **Idle detection** (X11 XScreenSaver, Wayland ext-idle-notify, /dev/input, D-Bus logind) — all deleted. KDE Plasma 6 Wayland had no working backend.
- **Lock screen** and **desktop detection** — replaced by the fullscreen break window.
- **Notification spam/persistent** — replaced by a single notify-send at 25→20min work mark + one during break at 5min remaining.
- **State machine module** — loop is simple enough to inline in main.cpp.
- **CLI** (Unix socket, `status`/`today`) — not needed, KISS.
- **Wayland protocol vendoring** (`protocols/ext-idle-notify-v1.xml`) — deleted.
- **Auto serviceInstall on startup** (2026-07-15 refactor) — moved to explicit `install` subcommand.
- **`--fg`/`-f` flag** (2026-07-15 refactor) — foreground is now the default mode. `--daemon`/`-d` to fork.