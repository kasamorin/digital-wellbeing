# Digital Wellbeing — Implementation Plan

## 项目概览

C 语言守护进程，Arch Linux 软件包，监控用户操作行为：

- **25min 工作周期** → 通知休息 5min
- **3min 监控窗口**：检测 ≥30s 连续空闲 → 立即锁屏；无空闲 → 3min 倒计时结束强制锁屏
- **5min 锁屏期**：提前解锁 → 密集轰炸通知（3-5s/条）直到 5min 结束；重新开始下一轮
- **通知策略**：配置文件可选密集轰炸或常驻通知，默认轰炸

## 技术栈

| 组件 | 方案 |
|------|------|
| 语言 | C23 (GNU23) + POSIX |
| 配置 | JSON (`~/.config/digital-wellbeing/config.json`)，用 `json-c` 库解析 |
| 空闲检测 | X11: XScreenSaver; Wayland: ext-idle-notify-v1（vendored XML + wayland-scanner） |
| 通知 | D-Bus `org.freedesktop.Notifications` |
| 锁屏 | 桌面识别表 → Wayland/X11 通用 → fallback + 20s 额外监控 → 轰炸 |
| 启动 | systemd user unit |
| 打包 | PKGBUILD |
| 编码风格 | Allman, 4空格, 80列, mixedCase, `*`靠变量, `.clang-format` |

## 构建依赖 (make deps)

- Arch: `gcc make json-c wayland wayland-protocols libxss dbus systemd`
- 其中 `wayland-protocols` 提供 `ext-idle-notify-v1.xml`

## 架构

```
digital-wellbeing
├── src/
│   ├── main.c            # 入口、daemonize、信号、主循环
│   ├── main.h
│   ├── config.c/h        # JSON 配置读写（json-c）
│   ├── stateMachine.c/h  # 核心状态机（WORKING→NOTIFIED→MONITORING→LOCKED→循环）
│   ├── idle.c/h          # 空闲检测：X11 + Wayland，自动探测
│   ├── notify.c/h        # D-Bus 通知发送
│   ├── lock.c/h          # 锁屏：桌面匹配表 → 通用方法 → fallback
│   ├── desktop.c/h       # 桌面环境识别
│   └── cli.c/h           # CLI 查询接口
├── protocols/
│   └── ext-idle-notify-v1.xml  # vendored Wayland protocol
├── systemd/
│   └── digital-wellbeing.service
├── .clang-format
├── Makefile
├── PKGBUILD
├── README.md
├── PLAN.md
└── PROGRESS.md
```

## 状态机

```
WORKING ──25min到──▶ NOTIFIED ──发通知──▶ MONITORING
                                                  │
                    ┌──30s空闲/3min到──锁屏──────▶│
                    ▼                            │
                  (在MONITORING内)               │
                                                  │
LOCKED ──5min到──▶ WORKING (重新周期)             │
   │                                              │
   └──提前解锁──▶ 轰炸/常驻通知直到5min结束──────▶ WORKING
```

## 锁屏桌面匹配表

| 桌面 | 方法 |
|------|------|
| Plasma (KDE) | `loginctl lock-session` |
| GNOME | `dbus-send ... org.gnome.ScreenSaver` |
| Cinnamon | `cinnamon-screensaver-command --lock` |
| XFCE | `xflock4` / `loginctl lock-session` |
| LXQt | `loginctl lock-session` |
| Sway | `swaylock` 或 `swaymsg exec swaylock` |
| Hyprland | `hyprlock` 或 `loginctl lock-session` |
| i3 (X11) | `i3lock` 或 `loginctl lock-session` |
| 未知 Wayland | `loginctl lock-session` |
| 未知 X11 | `xdg-screensaver lock` / `loginctl lock-session` |
| 全部失败 | 提醒用户，+20s 监测，有操作→轰炸 |

## 配置文件 (config.json)

```json
{
  "workMinutes": 25,
  "breakMinutes": 5,
  "monitorWindowMinutes": 3,
  "idleThresholdSeconds": 30,
  "lockScreenCommand": "",
  "notificationStrategy": "spam",
  "spamIntervalSeconds": 4
}
```

`lockScreenCommand` 为空时自动检测；`notificationStrategy`: `"spam"` | `"persistent"`。

## CLI

```bash
digital-wellbeing status     # 当前状态、周期计数
digital-wellbeing today      # 今日统计
```

通过 Unix socket (`~/.cache/digital-wellbeing.sock`) 与守护进程通信。

## 分阶段计划

### Phase 1: 项目骨架 + 编码规范 ✅
**commit**: `dd6d8b9`
**文件**: `.clang-format`, `Makefile`, `README.md`

### Phase 2: 配置系统 ✅
**commit**: `d2e64f9`
**文件**: `src/config.c/h`, `src/main.c`
- 首运行生成默认 `config.json`
- `json-c` 读取/写入

### Phase 3: 空闲检测 🔄
**文件**: `src/idle.c/h`, `protocols/ext-idle-notify-v1.xml`
- X11: XScreenSaver ✅
- Wayland: ext-idle-notify-v1 (代码完成，KWin 未实现此协议)
- **当前阻塞**: KDE Plasma 6 Wayland 空闲检测方法待确定

### Phase 4: 桌面识别 + 锁屏
**commit**: `feat: desktop detection and intelligent lock screen`

### Phase 5: 通知系统
**commit**: `feat: D-Bus notification with spam/persistent modes`

### Phase 6: 核心状态机 + 守护进程
**commit**: `feat: core state machine daemon with systemd service unit`

### Phase 7: CLI
**commit**: `feat: CLI status query via Unix socket`

### Phase 8: 打包 + 文档
**commit**: `feat: PKGBUILD for Arch Linux packaging`