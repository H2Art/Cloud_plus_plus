#!/bin/sh
# Ежесуточный бэкап: снимок, ротация, проверка структуры репозитория.
set -eu
export RESTIC_REPOSITORY=/srv/backup/repo
export RESTIC_PASSWORD_FILE=/root/.restic-pass

# Незавершённые загрузки (.part-*) не копируем
restic backup /srv/storage/files --exclude '.part-*' --tag cloud --host cloud
restic forget --keep-daily 7 --keep-weekly 2 --prune
restic check
