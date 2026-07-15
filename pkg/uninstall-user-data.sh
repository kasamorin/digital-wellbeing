#!/bin/bash
#
# digital-wellbeing 卸载清理脚本
# 用于清理 pacman -R digital-wellbeing 无法清理的用户数据文件
#
# pacman -R 会移除 /usr/bin/digital-wellbeing 和 /usr/share/locale/*/LC_MESSAGES/digital-wellbeing.mo
# 本脚本清理程序运行时在用户目录下产生的文件

set -e

echo "==> digital-wellbeing 卸载清理"

# 配置文件
CONFIG_FILE="$HOME/.config/digital-wellbeing/config.json"
CONFIG_DIR="$HOME/.config/digital-wellbeing"

# systemd 用户单元
SERVICE_FILE="$HOME/.config/systemd/user/digital-wellbeing.service"

# 禁用并停止 systemd 用户单元（如果存在）
if systemctl --user is-enabled digital-wellbeing.service &>/dev/null; then
    echo "  -> 禁用 systemd 用户单元..."
    systemctl --user disable --now digital-wellbeing.service
elif systemctl --user is-active digital-wellbeing.service &>/dev/null; then
    echo "  -> 停止 systemd 用户单元..."
    systemctl --user stop digital-wellbeing.service
fi

# 删除 service 文件
if [ -f "$SERVICE_FILE" ]; then
    echo "  -> 删除 $SERVICE_FILE"
    rm -f "$SERVICE_FILE"
fi

# 删除配置文件和目录
if [ -f "$CONFIG_FILE" ]; then
    echo "  -> 删除 $CONFIG_FILE"
    rm -f "$CONFIG_FILE"
fi
if [ -d "$CONFIG_DIR" ] && [ -z "$(ls -A "$CONFIG_DIR" 2>/dev/null)" ]; then
    echo "  -> 删除空目录 $CONFIG_DIR"
    rmdir "$CONFIG_DIR"
fi

echo "==> 清理完成"