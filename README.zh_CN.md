# Digital Wellbeing

[English](README.md) | [中文](README.zh_CN.md)

一个轻量级的番茄工作法休息提醒工具，适用于 Linux（KDE Plasma / Qt6）。

> **注意：** 本项目完全由 AI（Claude Code）开发。

## 工作原理

- **工作计时器**（默认 25 分钟）—— 专注工作时段
- **预通知**：在工作结束前 5 分钟通过桌面通知提醒
- **Qt6 全屏休息覆盖窗口**：工作时间结束时弹出
  - 大字体倒计时（MM:SS 格式）
  - "跳过休息"按钮和 Ctrl+Q 快捷键
  - 半透明深色背景，始终置顶
- 循环重复，直到收到停止信号（SIGTERM/SIGINT）

KISS 原则：不检测空闲、不锁屏、不轰炸通知、不提供 CLI 状态查询 —— 只有工作/休息循环和全屏覆盖窗口。

## 依赖

- **Arch Linux**（或其他具有以下依赖的 Linux 发行版）
- `gcc` + `make` — 构建
- `json-c` — 配置文件解析
- `qt6-base` — 休息窗口 GUI
- `libnotify` — 提供 `notify-send` 桌面通知命令
- `gettext` — 仅构建时需要（msgfmt 用于 .po → .mo 编译）

## 安装

```bash
make
make install
```

将二进制文件安装到 `~/.local/bin/digital-wellbeing`。

### systemd 用户服务

设置开机自启：

```bash
./digital-wellbeing install    # 写入 systemd 用户单元文件
systemctl --user enable digital-wellbeing
systemctl --user start digital-wellbeing
```

注意：`./digital-wellbeing install` 仅写入 service 文件并打印提示信息。启用和启动需手动通过 systemctl 完成。

## 使用方法

```bash
digital-wellbeing              # 前台模式 — 在 stderr 输出日志
digital-wellbeing --daemon     # 后台模式 — fork 并脱离终端
digital-wellbeing install      # 安装 systemd 用户 service 文件
digital-wellbeing --help       # 显示帮助信息
```

前台模式下，循环转换会以时间戳格式记录到 stderr：

```
[14:30:00] Starting digital-wellbeing — 25 min work, 5 min break
[14:30:00] Work period started — 25 minutes
[14:52:00] Break period started — 5 minutes
[14:55:00] Break completed, starting new work period
```

## 配置

`~/.config/digital-wellbeing/config.json`（首次运行自动生成）：

```json
{
  "workMinutes": 25,
  "breakMinutes": 5
}
```

| 键 | 默认值 | 说明 |
|----|--------|------|
| `workMinutes` | 25 | 工作时段时长（分钟） |
| `breakMinutes` | 5 | 休息时段时长（分钟） |

## 国际化 (i18n)

基于 gettext 的翻译。支持的语言：

- 英文 (`en`) — 默认/回退语言
- 简体中文 (`zh_CN`)

语言通过 `LANG` 环境变量检测：

```bash
LANG=zh_CN.UTF-8 ./digital-wellbeing          # 中文界面和消息
LANG=en_US.UTF-8 ./digital-wellbeing          # 英文界面和消息
```

添加新语言：

1. 从 `po/digital-wellbeing.pot` 创建 `po/<语言代码>.po`
2. 填入翻译
3. 将语言代码添加到 `po/LINGUAS`
4. `make mo` 编译
5. 发送 pull request

## 构建目标

```bash
make              # 构建二进制文件
make clean        # 清理构建产物
make install      # 安装二进制文件和翻译文件到 ~/.local/
make uninstall    # 移除已安装文件
make pot          # 从源代码重新生成 PO 模板
make po           # 将模板合并到各语言 PO 文件
make mo           # 从 .po 编译 .mo 文件
make locales      # pot + po + mo（完整 i18n 刷新）
```

## 许可证

GPLv3 — 详见 `COPYING`。