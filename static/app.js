'use strict';
/* Фронтенд: загрузка файлов и показ ошибок. Списки, создание и удаление
 * папок делает HTMX. Чистые функции вынесены отдельно и тестируются в Node
 * (tests/js/test_app.js). Без inline-скриптов: работает под CSP default-src 'self'. */
(function () {
  var MAX_UPLOAD_MB = 500;

  function joinPath(dir, name) {
    return dir ? dir + '/' + name : name;
  }

  function formatBytes(n) {
    var units = ['KB', 'MB', 'GB', 'TB'], i = -1;
    if (n < 1024) return n + ' B';
    do { n /= 1024; i++; } while (n >= 1024 && i < units.length - 1);
    return n.toFixed(1) + ' ' + units[i];
  }

  /* ctx: 'upload' | 'mkdir' | 'rm' (код 409 значит разное) */
  function messageForStatus(status, ctx) {
    switch (status) {
      case 400: return 'Некорректное имя или путь';
      case 403: return 'Запрос отклонён';
      case 404: return 'Не найдено (возможно, папка уже удалена)';
      case 409:
        if (ctx === 'upload') return 'Файл с таким именем уже существует';
        if (ctx === 'rm') return 'Папка не пуста';
        return 'Такое имя уже занято';
      case 413: return 'Файл слишком большой (лимит ' + MAX_UPLOAD_MB + ' МБ)';
      case 507: return 'Недостаточно места на диске';
      default: return 'Ошибка сервера (' + status + ')';
    }
  }

  function checkUrl(path, size) {
    return '/upcheck?p=' + encodeURIComponent(path) + '&size=' + size;
  }

  function uploadUrl(path) {
    return '/up?p=' + encodeURIComponent(path);
  }

  function listUrl(dir) {
    return '/list?p=' + encodeURIComponent(dir);
  }

  function summary(okCount, errors) {
    var parts = [];
    if (okCount > 0) parts.push('Загружено файлов: ' + okCount);
    if (errors.length > 0) parts.push(errors.join('; '));
    return parts.join('. ');
  }

  var api = {
    joinPath: joinPath, formatBytes: formatBytes, messageForStatus: messageForStatus,
    checkUrl: checkUrl, uploadUrl: uploadUrl, listUrl: listUrl, summary: summary
  };
  if (typeof module !== 'undefined' && module.exports) module.exports = api;
  if (typeof document === 'undefined') return;

  /* ---- браузерная часть ---- */
  var msg = document.getElementById('msg');
  var fileInput = document.getElementById('file');
  var prog = document.getElementById('prog');
  var info = document.getElementById('proginfo');

  function showMsg(text, isError) {
    msg.textContent = text;          /* textContent: никакого HTML из имён файлов */
    msg.className = text ? (isError ? 'err' : 'ok') : '';
  }

  function currentDir() {
    var i = document.querySelector('#list input[name="dir"]');
    return i ? i.value : '';
  }

  function refresh(dir) {
    if (window.htmx) window.htmx.ajax('GET', listUrl(dir), '#list');
    else window.location.reload();
  }

  function uploadOne(file, dir) {
    var path = joinPath(dir, file.name);
    return fetch(checkUrl(path, file.size), { method: 'POST', headers: { 'HX-Request': 'true' } })
      .then(function (r) {
        if (!r.ok) throw new Error(file.name + ': ' + messageForStatus(r.status, 'upload'));
        return new Promise(function (resolve, reject) {
          var xhr = new XMLHttpRequest();
          xhr.open('PUT', uploadUrl(path));
          xhr.setRequestHeader('HX-Request', 'true');
          xhr.upload.onprogress = function (e) {
            if (!e.lengthComputable) return;
            prog.value = e.total ? Math.round(100 * e.loaded / e.total) : 100;
            info.textContent = file.name + ': ' + formatBytes(e.loaded) + ' из ' + formatBytes(e.total);
          };
          xhr.onload = function () {
            if (xhr.status === 200) resolve();
            else reject(new Error(file.name + ': загрузка не удалась'));
          };
          xhr.onerror = function () { reject(new Error(file.name + ': нет соединения')); };
          xhr.send(file);
        });
      });
  }

  function uploadAll(files) {
    var dir = currentDir(), ok = 0, errors = [], chain = Promise.resolve();
    prog.hidden = false;
    prog.value = 0;
    showMsg('', false);
    Array.prototype.forEach.call(files, function (f) {
      chain = chain.then(function () { return uploadOne(f, dir); })
        .then(function () { ok++; }, function (e) { errors.push(e.message); });
    });
    return chain.then(function () {
      prog.hidden = true;
      info.textContent = '';
      showMsg(summary(ok, errors), errors.length > 0);
      refresh(dir);
    });
  }

  fileInput.addEventListener('change', function () {
    if (fileInput.files.length === 0) return;
    uploadAll(fileInput.files).then(function () { fileInput.value = ''; });
  });

  /* перетаскивание файлов на зону загрузки */
  var drop = document.getElementById('drop');
  ['dragenter', 'dragover'].forEach(function (name) {
    drop.addEventListener(name, function (e) { e.preventDefault(); drop.classList.add('over'); });
  });
  ['dragleave', 'drop'].forEach(function (name) {
    drop.addEventListener(name, function (e) { e.preventDefault(); drop.classList.remove('over'); });
  });
  /* промах мимо зоны не должен открывать файл в самой вкладке */
  window.addEventListener('dragover', function (e) { e.preventDefault(); });
  window.addEventListener('drop', function (e) { e.preventDefault(); });
  drop.addEventListener('drop', function (e) {
    var dt = e.dataTransfer, files = [], skipped = false, i, entry;
    if (!dt || !dt.files || dt.files.length === 0) return;
    for (i = 0; i < dt.files.length; i++) {
      entry = dt.items && dt.items[i] && dt.items[i].webkitGetAsEntry
        ? dt.items[i].webkitGetAsEntry() : null;
      if (entry && entry.isDirectory) skipped = true;   /* папки не поддерживаем */
      else files.push(dt.files[i]);
    }
    if (files.length === 0) { showMsg('Папки перетаскивать нельзя, только файлы', true); return; }
    uploadAll(files).then(function () {
      if (skipped) showMsg(msg.textContent + ' (папки пропущены)', msg.className === 'err');
    });
  });

  /* ошибки HTMX (mkdir, rm, list) */
  document.body.addEventListener('htmx:beforeRequest', function () { showMsg('', false); });
  document.body.addEventListener('htmx:responseError', function (e) {
    var xhr = e.detail.xhr;
    var path = e.detail.pathInfo ? e.detail.pathInfo.requestPath : '';
    var ctx = path.indexOf('/rm') === 0 ? 'rm' : (path.indexOf('/mkdir') === 0 ? 'mkdir' : 'list');
    showMsg(messageForStatus(xhr.status, ctx), true);
  });
  document.body.addEventListener('htmx:sendError', function () {
    showMsg('Нет соединения с сервером', true);
  });
})();
