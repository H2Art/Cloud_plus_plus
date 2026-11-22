#!/bin/sh
# Сборка и выкладка с локальной машины: deploy/deploy.sh user@server
# Собирает СТАТИЧЕСКИЙ бинарник (не зависит от версии glibc на сервере).
# Нужно: sudo dnf install glibc-static
set -eu

HOST=${1:?usage: deploy/deploy.sh user@server}
cd "$(dirname "$0")/.."

[ -f static/htmx.min.js ] || { echo "static/htmx.min.js is missing" >&2; exit 1; }

make test
make build LDFLAGS=-static
BIN=build/release/cloud
if ldd "$BIN" >/dev/null 2>&1; then
    echo "binary is not static" >&2
    exit 1
fi

echo "uploading..."
scp "$BIN" "$HOST:/tmp/cloud.new"
tar -C static -cf - . | ssh "$HOST" 'rm -rf /tmp/cloud-static && mkdir /tmp/cloud-static && tar -C /tmp/cloud-static -xf -'

ssh -t "$HOST" '
set -e
sudo install -m 0755 -o root -g root /tmp/cloud.new /usr/local/bin/cloud.next
sudo mv -f /usr/local/bin/cloud.next /usr/local/bin/cloud
sudo rm -rf /srv/cloud/static
sudo mkdir -p /srv/cloud
sudo cp -r /tmp/cloud-static /srv/cloud/static
sudo chmod -R a+rX /srv/cloud/static
sudo systemctl restart cloud
sleep 1
systemctl is-active cloud
'
echo "deployed"
