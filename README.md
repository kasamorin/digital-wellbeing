# Digital Wellbeing

[English](README.md) | [中文](README.zh_CN.md)

A lightweight pomodoro-style break reminder for Linux (KDE Plasma / Qt6).

> **Note:** This project is entirely developed by AI (Claude Code).

## How it works

- **Work timer** (default 25 minutes) — focus session
- **Pre-notification** at 5 minutes remaining via desktop notification
- **Qt6 fullscreen break overlay** when work time ends
  - Large countdown timer (MM:SS)
  - "Skip Break" button and Ctrl+Q shortcut
  - Semi-transparent dark background, stays on top
- Cycle repeats until stopped (SIGTERM/SIGINT)

KISS principle: no idle detection, no lock screen, no notification spam, no CLI status queries — just work/break cycles and a fullscreen overlay.

## Requirements

- **Arch Linux** (or any Linux with the dependencies below)
- `gcc` + `make` — build
- `json-c` — config parsing
- `qt6-base` — break window GUI
- `libnotify` — provides `notify-send` for desktop notifications
- `gettext` — build-time only (msgfmt for .po → .mo)

## Installation

```bash
make
make install
```

This installs the binary to `~/.local/bin/digital-wellbeing`.

### systemd user service

To start on login:

```bash
./digital-wellbeing install    # writes the systemd user unit file
systemctl --user enable digital-wellbeing
systemctl --user start digital-wellbeing
```

Note: `./digital-wellbeing install` only writes the service file and prints instructions. Enable/start is done manually via systemctl.

## Usage

```bash
digital-wellbeing              # foreground — shows logs on stderr
digital-wellbeing --daemon     # background — fork and detach
digital-wellbeing install      # install systemd user service file
digital-wellbeing --help       # show usage
```

In foreground mode, cycle transitions are logged to stderr with timestamps:

```
[14:30:00] Starting digital-wellbeing — 25 min work, 5 min break
[14:30:00] Work period started — 25 minutes
[14:52:00] Break period started — 5 minutes
[14:55:00] Break completed, starting new work period
```

## Configuration

`~/.config/digital-wellbeing/config.json` (auto-generated on first run):

```json
{
  "workMinutes": 25,
  "breakMinutes": 5
}
```

## Internationalization (i18n)

gettext-based translations. Supported languages:

- English (`en`) — default / fallback
- Chinese Simplified (`zh_CN`)

The locale is detected from the `LANG` environment variable:

```bash
LANG=zh_CN.UTF-8 ./digital-wellbeing          # Chinese UI and messages
LANG=en_US.UTF-8 ./digital-wellbeing          # English
```

To add a new language:

1. Create `po/<lang>.po` from `po/digital-wellbeing.pot`
2. Fill in translations
3. Add the language code to `po/LINGUAS`
4. `make mo` to compile
5. Send a pull request

## Build targets

```bash
make              # build binary
make clean        # remove build artifacts
make install      # install binary + translations to ~/.local/
make uninstall    # remove installed files
make pot          # regenerate PO template from source
make po           # merge template into language PO files
make mo           # compile .mo files from .po
make locales      # pot + po + mo (full i18n refresh)
```

## License

GPLv3 — see `COPYING`.