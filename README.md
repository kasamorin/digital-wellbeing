# Digital Wellbeing

A lightweight C daemon to promote healthy computing habits on Arch Linux.

## How it works

- **25-minute work timer** — focus session
- **5-minute break reminder** — desktop notification
- **3-minute monitoring window** — detects if you actually pause
  - 30+ seconds of idle → locks screen immediately
  - Continuous activity for 3 minutes → forced lock
- **5-minute locked rest** — unlocking early triggers notification spam until the break ends
- Cycle repeats

## Installation

### From AUR

```
paru -S digital-wellbeing
```

### Manual

```
make
sudo make install
systemctl --user enable --now digital-wellbeing
```

## Configuration

`~/.config/digital-wellbeing/config.json` (auto-generated on first run):

```json
{
  "workMinutes": 25,
  "breakMinutes": 5,
  "monitorWindowMinutes": 3,
  "idleThresholdSeconds": 30,
  "lockScreenCommand": "",
  "notificationStrategy": "spam",
  "spamIntervalSeconds": 4,
  "persistentNotificationTimeout": 0
}
```

| Key | Default | Description |
|-----|---------|-------------|
| `workMinutes` | 25 | Work session duration |
| `breakMinutes` | 5 | Rest session duration |
| `monitorWindowMinutes` | 3 | Post-notification activity monitoring |
| `idleThresholdSeconds` | 30 | Consecutive idle time to trigger lock |
| `lockScreenCommand` | `""` | Custom lock command (auto-detect if empty) |
| `notificationStrategy` | `"spam"` | `"spam"` or `"persistent"` for post-unlock notifications |
| `spamIntervalSeconds` | 4 | Interval between spam notifications (3-5s) |

## CLI

```
digital-wellbeing status    # Current cycle state and count
digital-wellbeing today     # Today's statistics
```

## Dependencies

- `json-c` — configuration parsing
- `dbus` — desktop notifications
- `libxscrnsaver` — X11 idle detection
- `wayland-client` + `wayland-protocols` — Wayland idle detection (optional)
- `systemd` — user service management

## License

MIT