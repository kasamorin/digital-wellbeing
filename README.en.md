# Digital Wellbeing

[中文](README.md) | [English](README.en.md)

[![Release](https://img.shields.io/github/v/release/kasamorin/digital-wellbeing)](https://github.com/kasamorin/digital-wellbeing/releases)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](COPYING)

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
- `gcc` (14+, for `-std=gnu23`) + `make` — build
- `json-c` — config parsing
- `qt6-base` — break window GUI
- `libnotify` — provides `notify-send` for desktop notifications
- `gettext` — build-time only (msgfmt for .po → .mo)

## Installation

### Arch Linux (pacman package)

Every [Release](https://github.com/kasamorin/digital-wellbeing/releases) ships
`.pkg.tar.zst` packages for `x86_64` and `aarch64`. Download yours and install
with pacman:

```bash
sudo pacman -U digital-wellbeing-*-x86_64.pkg.tar.zst
```

Or build from the bundled `pkg/PKGBUILD` (works on any Arch architecture):

```bash
cd pkg && makepkg -si
```

### Prebuilt binary (generic tarball)

Generic tarballs for `x86_64` and `aarch64` are also on
[Releases](https://github.com/kasamorin/digital-wellbeing/releases) for
non-Arch distributions. Verify with the `.sha256` file, then extract into
`~/.local`:

```bash
sha256sum -c digital-wellbeing-*.tar.gz.sha256
tar -xzf digital-wellbeing-*.tar.gz -C ~/.local --strip-components=1
```

This places the binary at `~/.local/bin/digital-wellbeing` and translations
under `~/.local/share/locale/`.

### From source

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

## Directory layout

| Path | Purpose |
|------|---------|
| `/usr/bin/digital-wellbeing` | Binary (PKGBUILD install) |
| `/usr/share/locale/*/LC_MESSAGES/digital-wellbeing.mo` | gettext translations |
| `~/.config/digital-wellbeing/config.json` | User config |
| `~/.config/systemd/user/digital-wellbeing.service` | systemd user unit |

If you install to `~/.local/bin/` instead (e.g. `make install`), edit the
service file's `ExecStart=` line accordingly.

## Configuration

`~/.config/digital-wellbeing/config.json` (auto-generated on first run):

```json
{
  "workMinutes": 25,
  "breakMinutes": 5
}
```

| Key | Default | Valid range |
|-----|---------|-------------|
| `workMinutes` | 25 | 1–1440 |
| `breakMinutes` | 5 | 1–1440 |

Out-of-range values fall back to the defaults.

## Internationalization (i18n)

gettext-based translations. Supported languages:

- English (`en`) — default / fallback
- Chinese Simplified (`zh_CN`)

The locale is detected from the `LANG` environment variable:

```bash
LANG=zh_CN.UTF-8 ./digital-wellbeing          # Chinese UI and messages
LANG=en_US.UTF-8 ./digital-wellbeing          # English
```

Translations are looked up in `~/.local/share/locale` first, falling back to
`/usr/share/locale`, so pacman-installed translations work for regular users
too.

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

## Development

This is a small self-use project without a separate contributing guide. To work
on it:

- Build with `make clean && make`; the binary lands at `./digital-wellbeing`.
- Install with `make install` (defaults to `~/.local`). For packaging, override
  the destination: `make install PREFIX=/usr DESTDIR="$pkgdir"`.
- Refresh translations with `make locales`, then add a language to `po/LINGUAS`.
- Commit messages follow the existing `feat:` / `fix:` / `docs:` convention.
- AI-agent instructions live in `AGENTS.md`.

## License

GPLv3 — see `COPYING`.
