# Digital Wellbeing — Build Progress

## Phase 1: Project scaffold ✅
**commit**: `dd6d8b9` — `init: project scaffold with clang-format, Makefile, and README`
- `.clang-format`: Allman, 4-space, 80-col, `*`靠变量, GNUC23
- `Makefile`: C23/GNU23, pkg-config for json-c/dbus-1/xscrnsaver/x11/xext + wayland optional
- `README.md`: 功能说明、配置项、依赖、安装方式

## Phase 2: JSON config system ✅
**commit**: `d2e64f9` — `feat: JSON config read/write with json-c — auto-generates default config on first run`
- `src/config.h`: Config struct, NotificationStrategy enum, API 声明
- `src/config.c`: json-c 解析/写入, 默认值, 自动生成 `~/.config/digital-wellbeing/config.json`
- `src/main.c`: 临时入口，测试 config 加载和 idle 检测
- 验证通过: `make clean && make` 零 warning, 运行正常

## Phase 3: Idle detection — in progress
**commit**: 待提交
**状态**: 编译通过，Wayland 后端在 KDE Plasma 6 下有问题

### 已完成
- `protocols/ext-idle-notify-v1.xml`: hand-vendored ext-idle-notify-v1 protocol
- `src/idle.h`: IdleBackend enum, idleInit/idleGetMs/idleShutdown API
- `src/idle.c`: X11 XScreenSaver backend ✅, Wayland ext-idle-notify backend (编译通过)
- `Makefile`: wayland-scanner codegen, protocol .o 链接
- `src/main.c`: 5 秒 polling 测试 idle 检测

### 当前问题：KDE Plasma 6 (Wayland) 空闲检测方案探索中
- `ext_idle_notifier_v1` 不在 KWin global list（`wayland-info` 验证）→ KWin 未实现此 staging 协议
- KWin `/ScreenSaver` D-Bus `GetSessionIdleTime` 返回 `NotSupported` (Wayland 下)
- X11 XScreenSaver 在 Wayland 下不可用（无 X server）
- XWayland 的 XScreenSaver 无 extension
- `org.kde.kidletime` D-Bus 服务名不存在（KF6 改用其他机制）
- **待解决**: 找到 KDE Plasma 6 下可用的空闲检测方法
  - 候选: `loginctl show-session` IdleHint（用户中断了测试）
  - 候选: `/dev/input/event*` 设备时间戳轮询

## Phase 4-8: 待开始