# Digital Wellbeing — Progress Tracking

## Phase 1: 项目骨架 + 编码规范 ✅
**commit**: `dd6d8b9` — init: project scaffold with clang-format, Makefile, and README

## Phase 2: 配置系统 ✅
**commit**: `d2e64f9` — feat: JSON config read/write with json-c — auto-generates default config on first run

**文件**: `src/config.c/h`, `src/main.c`
- 首运行生成默认 `config.json`
- `json-c` 读取/写入

## Phase 3: 空闲检测 🔄
**commit TBD**: `feat: idle detection — X11 XScreenSaver + /dev/input + D-Bus logind + Wayland ext-idle-notify`

**文件**: `src/idle.c/h`, `protocols/ext-idle-notify-v1.xml`

### 2026-07-13 — 初步实现
- X11: XScreenSaverQueryInfo ✅ — 编译通过，X11 下可用
- Wayland: ext-idle-notify-v1 ✅ — 代码完成，但 KWin 不实现此协议

### 2026-07-14 — 补充后端（pure-mapping-milner 计划执行）

**新增后端**:
1. **`/dev/input`** (`IDLE_BACKEND_INPUT`) — 扫描 `/dev/input/event*`，识别键盘/鼠标，通过 drain pending events 检测活动
   - 使用 monotonic clock 记录最后活动时间戳
   - DE/合成器/显示协议无关，通用性最高
   - **需要 `input` 组权限**

2. **D-Bus logind** (`IDLE_BACKEND_DBUS`) — system bus 调用 `org.freedesktop.login1.Session.IdleSinceHintMonotonic`
   - 编译通过，API 可调用
   - **已知缺陷**: KDE Plasma 6 不调用 `SetIdleHint`，因此 logind idle 始终为 0
   - 在其他 DE（GNOME 等）可能可用

**后端选择优先级** (Wayland 会话):
```
/dev/input → D-Bus logind → ext-idle-notify → NONE
```

X11 会话优先 XScreenSaver，fallback 顺序相同。

**当前阻塞**: `/dev/input` 在 Wayland 用户态返回全 0 idle。可能原因：
- 需要确认在 sudo 下 `read()` 能否 drain 到事件
- 可能是 `O_NONBLOCK` + 事件节奏问题（用户动作产生事件是 burst，测试窗口 5s 可能错过？）

**下一步调试**: sudo + evtest 验证设备事件流，排除读逻辑时序 bug

### idle.h 变更
- `IdleBackend` 枚举新增 `IDLE_BACKEND_INPUT` 和 `IDLE_BACKEND_DBUS`

### 已验证
- ✅ `make clean && make` — `-Wall -Wextra -Wpedantic` 零 warning
- ✅ Wayland 默认跳过不存在的 ext-idle-notifier（fallback 到下一个）
- ✅ X11 被 Wayland 忽视为 NONE 后在 X11 模式能上 XScreenSaver
- ⚠️ `/dev/input` 需要 input 组权限（未在 CI 验证）
- ⚠️ D-Bus logind 在 KDE Plasma 6 不工作（compositor 不更新 IdleHint）

## Phase 4-8: 待开始

详见 `PLAN.md`。