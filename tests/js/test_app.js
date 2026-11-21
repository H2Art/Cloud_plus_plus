'use strict';
const assert = require('assert');
const app = require('../../static/app.js');

let n = 0;
function check(name, fn) { fn(); n++; }

check('joinPath', () => {
  assert.strictEqual(app.joinPath('', 'a.txt'), 'a.txt');
  assert.strictEqual(app.joinPath('docs', 'a.txt'), 'docs/a.txt');
  assert.strictEqual(app.joinPath('a/b', 'c'), 'a/b/c');
});

check('formatBytes', () => {
  assert.strictEqual(app.formatBytes(0), '0 B');
  assert.strictEqual(app.formatBytes(1023), '1023 B');
  assert.strictEqual(app.formatBytes(1024), '1.0 KB');
  assert.strictEqual(app.formatBytes(1536), '1.5 KB');
  assert.strictEqual(app.formatBytes(5 * 1024 * 1024), '5.0 MB');
  assert.strictEqual(app.formatBytes(2 * 1024 ** 3), '2.0 GB');
});

check('messageForStatus', () => {
  assert.match(app.messageForStatus(409, 'upload'), /уже существует/);
  assert.match(app.messageForStatus(409, 'rm'), /не пуста/);
  assert.match(app.messageForStatus(409, 'mkdir'), /занято/);
  assert.match(app.messageForStatus(413, 'upload'), /500/);
  assert.match(app.messageForStatus(507, 'upload'), /места/);
  assert.match(app.messageForStatus(404, 'list'), /Не найдено/);
  assert.match(app.messageForStatus(400, 'upload'), /Некорректное/);
  assert.match(app.messageForStatus(403, 'upload'), /отклонён/);
  assert.match(app.messageForStatus(500, 'upload'), /\(500\)/);
});

check('urls', () => {
  assert.strictEqual(app.uploadUrl('docs/a b.txt'), '/up?p=docs%2Fa%20b.txt');
  assert.strictEqual(app.checkUrl('x.txt', 12), '/upcheck?p=x.txt&size=12');
  assert.strictEqual(app.listUrl(''), '/list?p=');
  assert.strictEqual(app.listUrl('docs/sub'), '/list?p=docs%2Fsub');
  /* кириллица и спецсимволы кодируются, & и = не ломают query */
  assert.strictEqual(app.uploadUrl('к&и=р.txt'), '/up?p=%D0%BA%26%D0%B8%3D%D1%80.txt');
  assert.strictEqual(decodeURIComponent(app.uploadUrl('a"<b>.txt').slice(6)), 'a"<b>.txt');
});

check('summary', () => {
  assert.strictEqual(app.summary(2, []), 'Загружено файлов: 2');
  assert.strictEqual(app.summary(0, ['a: ошибка']), 'a: ошибка');
  assert.strictEqual(app.summary(1, ['b: ошибка', 'c: ошибка']),
                     'Загружено файлов: 1. b: ошибка; c: ошибка');
  assert.strictEqual(app.summary(0, []), '');
});

console.log('tests/js/test_app.js: ' + n + ' groups, 0 failed');
