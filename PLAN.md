# Digital Wellbeing — Implementation Plan

## 项目概览

C + C++ (Qt6) 混合守护进程，Arch Linux，pomodoro 式数字健康工具：

- **25min 工作周期** → 20min 时发通知提醒 → 25min 到弹出全屏休息窗口
- **5min 休息窗口**：全屏置顶、大字倒计时、"跳过休息"按钮、Ctrl+Q 关闭
- 跳过或倒计时到 0 → 窗口关闭，重新开始下一轮工作周期
- KISS 原则：不锁屏、不监控空闲、不轰炸通知、不提供 CLI

## 技术栈

| 组件 | 方案 |
|------|------|
| 语言 | C23 (GNU23) + C++23 |
| GUI 窗口 | Qt6 Widgets（全屏置顶倒计时） |
| 配置 | JSON (`~/.config/digital-wellbeing/config.json`)，`json-c` |
| 通知 | shell `notify-send`（D-Bus org.freedesktop.Notifications） |
| 启动 | systemd user unit（首次运行自安装） |
| 编码风格 | Allman, 4空格, 80列, mixedCase, `*`靠变量, `.clang-format` |

## 构建依赖

- Arch: `gcc make json-c qt6-base libnotify`
- `libnotify` 提供 `notify-send` 命令（runtime 依赖，非 link 依赖）

## 架构

```
digital-wellbeing
├── src/
│   ├── main.cpp          # 入口、daemonize、信号、主循环、QApplication
│   ├── config.c/h        # JSON 配置读写（json-c），wrapped for C++
│   ├── breakWindow.cpp/h # Qt6 全屏休息窗口（extern "C" API）
│   └── service.c/h       # systemd user unit 自安装
├── .clang-format
├── Makefile
├── README.md
├── CLAUDE.md
├── PLAN.md
└── PROGRESS.md
```

## 主循环

```
while running:
    sleep((workMinutes - 5) × 60)     # 前段工作
    notify-send "5 minutes until break time" (30s timeout)
    sleep(5 × 60)                     # 最后 5min 工作
    breakWindowShow(breakMinutes × 60) # 阻塞直到休息窗口关闭
    loop back
```

SIGTERM/SIGINT → `volatile sig_atomic_t gRunning = 0` → 循环退出，进程正常结束。

## 休息窗口 (Qt6)

- `Qt::WindowStaysOnTopHint` 置顶
- `showFullScreen()` 全屏
- 96pt 大字体 `MM:SS` 倒计时
- "Skip Break" 按钮（220×60，浅色半透明）
- Ctrl+Q 快捷键
- 倒计时剩 5min 时 `notify-send` 提醒一次
- Breeze 主题自动继承（KDE Plasma）
- 无 `Q_OBJECT` 宏，无需 moc
- `QEventLoop` 嵌套事件循环阻塞直到关闭
- 返回值: `true` = 用户跳过, `false` = 倒计时到期

## 配置文件 (config.json)

```json
{
  "workMinutes": 25,
  "breakMinutes": 5
}
```

## systemd 自安装

首次运行写入 `~/.config/systemd/user/digital-wellbeing.service`:

```ini
[Unit]
Description=Digital Wellbeing — pomodoro-style break reminder

[Service]
Type=simple
ExecStart=%h/.local/bin/digital-wellbeing
Restart=on-failure
RestartSec=10

[Install]
WantedBy=default.target
```

`make install` 安装到 `~/.local/bin/digital-wellbeing`。

## 已删除（与原始计划不同）

- 空闲检测（X11 XScreenSaver / Wayland ext-idle-notify / /dev/input / D-Bus logind）
- 锁屏（桌面匹配表 → loginctl → fallback）
- 桌面环境识别
- 通知密集轰炸/常驻通知策略
- CLI（status/today via Unix socket）
- 状态机独立模块（太简单，main.cpp 直接跑循环）
- MONITORING / LOCKED 状态
- Wayland protocol XML vendoring