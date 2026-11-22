#!/bin/sh
# Установка пользовательского таймера на Fedora (без root): sh install.sh
set -eu
D=$(cd "$(dirname "$0")" && pwd)
mkdir -p "$HOME/.local/bin" "$HOME/.config/systemd/user"
install -m 0755 "$D/pull-backup.sh" "$HOME/.local/bin/cloud-pull-backup.sh"
install -m 0644 "$D/cloud-backup-pull.service" "$D/cloud-backup-pull.timer" "$HOME/.config/systemd/user/"
systemctl --user daemon-reload
systemctl --user enable --now cloud-backup-pull.timer
systemctl --user list-timers cloud-backup-pull.timer --no-pager
