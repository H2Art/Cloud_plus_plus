#!/bin/sh
# Забирает restic-репозиторий с сервера на этот компьютер (pull, только чтение).
# Ключ ограничен на сервере через rrsync -ro /srv/backup.
set -eu

HOST=${CLOUD_BACKUP_HOST:-root@SERVER_IP}
KEY=${CLOUD_BACKUP_KEY:-$HOME/.ssh/cloud_backup}
DEST=${CLOUD_BACKUP_DEST:-$HOME/cloud-backup}

mkdir -p "$DEST"
rsync -a --delete-after \
    -e "ssh -i $KEY -o BatchMode=yes -o ConnectTimeout=20 -o ServerAliveInterval=15" \
    "$HOST:./" "$DEST/"

# Грубая проверка: без config репозиторий непригоден
[ -f "$DEST/repo/config" ] || { echo "pull-backup: repo/config missing in $DEST" >&2; exit 1; }
date +%s > "$DEST/.last-pull"
echo "pull-backup: ok"
