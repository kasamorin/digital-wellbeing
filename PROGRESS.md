# Digital Wellbeing — Progress Tracking

## Phase 1: 项目骨架 + 编码规范 ✅
**commit**: `dd6d8b9` — init: project scaffold with clang-format, Makefile, and README

## Phase 2: 配置系统 ✅
**commit**: `d2e64f9` — feat: JSON config read/write with json-c — auto-generates default config on first run

## Phase 3: 空闲检测 → 重构为全屏休息窗口 🔄→✅
**commit TBD**

**2026-07-13 — 空闲检测实现**:
- X11: XScreenSaverQueryInfo ✅
- Wayland: ext-idle-notify-v1 ✅ — KWin 不实现此协议
- /dev/input backend ✅ — Wayland 用户态读不到事件
- D-Bus logind backend ✅ — KDE Plasma 6 不更新 IdleHint

**2026-07-15 — 架构重构**:
KDE Plasma 6 Wayland 下所有空闲检测后端均有缺陷。决定放弃空闲检测 + 锁屏方案，改用 Qt6 全屏置顶倒计时窗口。

**删除**:
- `src/idle.c/h` — 全部空闲检测后端 (~760 行)
- `protocols/ext-idle-notify-v1.xml` — vendored Wayland protocol
- `systemd/` 目录 — service 改为代码自安装
- Makefile Wayland codegen/链接逻辑
- pkg-config: `xscrnsaver x11 xext wayland-client dbus-1`
- src/notify.c/h, src/stateMachine.c/h, src/lock.c/h, src/desktop.c/h — 均未创建，无需删除

**新增**:
- `src/breakWindow.cpp/h` — Qt6 全屏置顶倒计时窗口
  - 96pt 大字 `MM:SS` 倒计时
  - "Skip Break" 按钮 + Ctrl+Q 快捷键
  - Breeze 主题自动继承
  - `notify-send` 在休息剩 5min 时发通知
  - `QEventLoop` 阻塞直到窗口关闭
- `src/service.c/h` — systemd user unit 自安装
  - 首次运行写入 `~/.config/systemd/user/digital-wellbeing.service`
  - 调用 `systemctl --user enable`

**修改**:
- `src/main.c` → `src/main.cpp` — daemonize + QApplication + 主循环
  - 循环: sleep 20min → notify-send → sleep 5min → breakWindow → repeat
  - SIGTERM/SIGINT 优雅退出
- `src/config.c/h` — 精简到只保留 `workMinutes`、`breakMinutes`
- `Makefile` — gcc/g++ 混合编译，`-fpic` 解决 gcc 16 + Qt6 protected symbol 兼容

### 已验证
- ✅ `make clean && make` — `-Wall -Wextra -Wpedantic` 零 warning
- ✅ `./digital-wellbeing` daemonize + service 自安装正常
- ✅ `systemctl --user enable` 成功
- ⏳ breakWindow 全屏显示 + 倒计时（需要 XDG 会话下测试）
- ⏳ 完整 25+5min 周期测试

## Phase 4-8: 不需要（已合并简化）

原计划的锁屏/桌面识别/通知轰炸/CLI/Unix socket 均不再需要。当前架构足够 KISS。

### 待完成
- 实际 Wayland 会话下测试 breakWindow 全屏显示和 Breeze 主题
- PKGBUILD (optional)
- 通知改用 Qt QDBusInterface 而非 shell `notify-send` (optional polish)