#!/bin/sh
# Однократная подготовка сервера (Ubuntu). Запускать от root: sudo sh setup-server.sh
# Идемпотентен: повторный запуск ничего не ломает.
set -eu

IMG=/srv/storage.img
MNT=/srv/storage
DATA=$MNT/files
SIZE=2G

[ "$(id -u)" -eq 0 ] || { echo "run as root" >&2; exit 1; }

echo "[1/5] user cloud"
if ! id cloud >/dev/null 2>&1; then
    useradd --system --user-group --home-dir /nonexistent --shell /usr/sbin/nologin cloud
fi

echo "[2/5] storage image $IMG ($SIZE)"
mkdir -p "$MNT"
if [ ! -f "$IMG" ]; then
    fallocate -l "$SIZE" "$IMG"
    mkfs.ext4 -F -q -m 0 -L cloudstore "$IMG"
fi
if ! grep -q "^$IMG " /etc/fstab; then
    echo "$IMG $MNT ext4 loop,nodev,nosuid,noexec,nofail 0 0" >> /etc/fstab
fi
mountpoint -q "$MNT" || mount "$MNT"

echo "[3/5] directories and permissions"
mkdir -p "$DATA" /srv/cloud/static
chown cloud:cloud "$DATA"
chmod 0750 "$DATA"
chmod 0755 /srv/cloud /srv/cloud/static

echo "[4/5] nginx may read files (group cloud)"
if getent passwd www-data >/dev/null; then
    usermod -aG cloud www-data
else
    echo "  www-data not found: install nginx first, then rerun this script" >&2
fi

echo "[5/5] result"
df -h "$MNT" | tail -1
ls -ld "$DATA"
echo "done"
