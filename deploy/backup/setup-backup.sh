#!/bin/sh
# Однократная настройка бэкапов на сервере (Ubuntu). Запускать от root.
# Идемпотентен. Пароль репозитория печатается ОДИН раз при создании:
# сохрани его вне сервера, без него бэкапы нельзя прочитать.
set -eu

REPO=/srv/backup/repo
PASSFILE=/root/.restic-pass

[ "$(id -u)" -eq 0 ] || { echo "run as root" >&2; exit 1; }

echo "[1/4] restic"
command -v restic >/dev/null 2>&1 || { apt-get update -qq && apt-get install -y restic; }
restic version

echo "[2/4] password file"
if [ ! -f "$PASSFILE" ]; then
    umask 077
    openssl rand -base64 33 > "$PASSFILE"
    echo "NEW restic password (save it elsewhere NOW):"
    cat "$PASSFILE"
fi
chmod 600 "$PASSFILE"

echo "[3/4] repository $REPO"
mkdir -p /srv/backup
chmod 700 /srv/backup
if [ ! -f "$REPO/config" ]; then
    restic -r "$REPO" --password-file "$PASSFILE" init
fi

echo "[4/4] units"
install -m 0755 /tmp/cloud-backup.sh /usr/local/bin/cloud-backup.sh
install -m 0644 /tmp/cloud-backup.service /etc/systemd/system/cloud-backup.service
install -m 0644 /tmp/cloud-backup.timer /etc/systemd/system/cloud-backup.timer
systemctl daemon-reload
systemctl enable --now cloud-backup.timer
systemctl list-timers cloud-backup.timer --no-pager
echo "done"
