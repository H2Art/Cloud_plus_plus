# Развёртывание на сервере (Ubuntu)

Схема: браузер -> nginx (HTTPS, пароль) -> бэкенд на 127.0.0.1:8080.
Файлы отдаёт nginx напрямую из /srv/storage/files. Хранилище лежит в образе на 2 ГБ.

Ниже `user@server` это твой SSH-доступ, `IP` это публичный IPv4 сервера.

## 0. Один раз на Fedora
```
sudo dnf install glibc-static
```
Файл `static/htmx.min.js` должен лежать в репозитории.

## 1. Пакеты на сервере
```
sudo apt update && sudo apt install -y nginx
```

## 2. Подготовка хранилища и пользователя
```
scp deploy/setup-server.sh user@server:/tmp/
ssh -t user@server 'sudo sh /tmp/setup-server.sh && sudo systemctl restart nginx'
```
Скрипт создаёт пользователя cloud, образ /srv/storage.img на 2 ГБ, монтирует его
в /srv/storage, готовит каталог /srv/storage/files и добавляет www-data в группу cloud.
Перезапуск nginx нужен, чтобы он увидел новую группу.

## 3. Служба systemd
```
scp deploy/systemd/cloud.service user@server:/tmp/
ssh -t user@server 'sudo mv /tmp/cloud.service /etc/systemd/system/cloud.service && sudo systemctl daemon-reload && sudo systemctl enable cloud'
```

## 4. Первая выкладка (бинарник и статика)
```
deploy/deploy.sh user@server
```
Скрипт прогонит тесты, соберёт статический бинарник, загрузит его и статику,
перезапустит службу и напечатает `active`.

Проверка на сервере:
```
curl -si http://127.0.0.1:8080/ | head -5
journalctl -u cloud -n 20 --no-pager
```

## 5. HTTPS-сертификат на IP (самоподписанный, 10 лет)
На сервере, подставь свой IP в двух местах:
```
sudo mkdir -p /etc/nginx/ssl
sudo openssl req -x509 -newkey rsa:2048 -nodes \
  -keyout /etc/nginx/ssl/cloud.key -out /etc/nginx/ssl/cloud.crt \
  -days 3650 -subj "/CN=IP" -addext "subjectAltName=IP:IP"
sudo chmod 600 /etc/nginx/ssl/cloud.key
```

## 6. Пароль
```
printf 'ИМЯ:%s\n' "$(openssl passwd -apr1)" | sudo tee /etc/nginx/cloud.htpasswd >/dev/null
sudo chgrp www-data /etc/nginx/cloud.htpasswd && sudo chmod 640 /etc/nginx/cloud.htpasswd
```
Команда дважды спросит пароль.

## 7. Конфиг nginx
```
scp deploy/nginx/cloud.conf user@server:/tmp/
ssh -t user@server 'sudo mv /tmp/cloud.conf /etc/nginx/sites-available/cloud \
  && sudo ln -sf /etc/nginx/sites-available/cloud /etc/nginx/sites-enabled/cloud \
  && sudo rm -f /etc/nginx/sites-enabled/default \
  && sudo nginx -t && sudo systemctl reload nginx'
```
Если `nginx -t` ругнулся, ничего не применится: пришли вывод.

## 8. Файрвол (необязательно, но желательно)
Сначала разреши SSH, потом включай, иначе потеряешь доступ:
```
sudo ufw allow OpenSSH && sudo ufw allow 80/tcp && sudo ufw allow 443/tcp
sudo ufw enable
```
Порт 8080 наружу не открыт: бэкенд слушает только 127.0.0.1.

## 9. Проверка
Открой https://IP (браузер предупредит о сертификате, сверь отпечаток:
`openssl x509 -in cloud.crt -noout -fingerprint -sha256`). Проверь: вход по паролю,
создание папки, загрузка нескольких файлов, скачивание, файл 5 МБ (`cmp`), удаление,
файл с кириллицей и пробелами в имени, загрузка одного и того же файла дважды.

## Обновление в дальнейшем
```
deploy/deploy.sh user@server
```

## Полезное
```
sudo systemctl status cloud        # состояние
journalctl -u cloud -f             # логи
df -h /srv/storage                 # место в хранилище
sudo nginx -t                      # проверка конфига
```
