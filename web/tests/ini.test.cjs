'use strict';
const test = require('node:test');
const assert = require('node:assert/strict');
const ini = require('../ini.js');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

test('unknown sections, keys and comments survive a setting update', () => {
  const source = '; 用户注释\r\n[GENERAL]\r\n debug_log = 0\r\ncustom=stay\r\n\r\n[other]\r\npath=C:\\数据\\a=b\r\n';
  const result = ini.update(source, { 'general.debug_log': '1' });
  assert.equal(result, source.replace('debug_log = 0', 'debug_log = 1'));
});
test('absent sections and keys are appended without moving unknown data', () => {
  const result = ini.update('[unrelated]\nvalue=42\n', { 'hotkey.boss_key': 'Ctrl+Alt+B', 'dir_setting.data': 'D:\\数据' });
  assert.equal(ini.read(result)['hotkey.boss_key'], 'Ctrl+Alt+B');
  assert.equal(ini.read(result)['dir_setting.data'], 'D:\\数据');
  assert.ok(result.startsWith('[unrelated]\nvalue=42\n'));
});
test('INI value cannot inject a key, section, or null through a line break', () => {
  for (const value of ['x\r\n[other]', 'x\ny=1', 'x\0']) assert.throws(() => ini.update('', { 'general.command_line': value }));
  assert.throws(() => ini.update('', { 'unknown.setting': 'x' }));
});
test('duplicate keys use and update the first case-insensitive occurrence', () => {
  const source = '[GENERAL]\nDEBUG_LOG=1\ndebug_log=0\n';
  assert.equal(ini.read(source)['general.debug_log'], '1');
  assert.equal(ini.update(source, { 'general.debug_log': '0' }), '[GENERAL]\nDEBUG_LOG=0\ndebug_log=0\n');
});
test('quoted values are read as Win32 INI values, not escaped Javascript', () => {
  assert.equal(ini.read('[dir_setting]\ndata="D:\\数据"\n')['dir_setting.data'], 'D:\\数据');
});
test('UTF-16LE BOM preserves Chinese paths and supplementary Unicode', () => {
  const source = '[dir_setting]\r\ndata=D:\\用户资料\\🌍\r\n';
  const bytes = ini.encode(source);
  assert.deepEqual([...bytes.slice(0, 2)], [255, 254]);
  assert.equal(ini.decode(bytes), source);
});
test('UTF8 and UTF16BE imports decode while invalid legacy bytes are rejected', () => {
  assert.equal(ini.decode(new TextEncoder().encode('name=你好')), 'name=你好');
  assert.equal(ini.decode(Uint8Array.from([254, 255, 0, 65])), 'A');
  assert.throws(() => ini.decode(Uint8Array.from([0xff, 0x80, 0x81])), /encoding/);
});
test('empty and commented-only configs retain runtime defaults', () => {
  assert.deepEqual(ini.read(''), ini.defaults);
  assert.deepEqual(ini.read('; debug_log=1\n# win32k=1'), ini.defaults);
});
test('web settings are disabled by default and can be explicitly enabled', () => {
  assert.equal(ini.defaults['general.webui'], '0');
  assert.equal(ini.read('')['general.webui'], '0');
  assert.equal(ini.read('[general]\nwebui=1\n')['general.webui'], '1');
});
test('turning off web settings changes only its INI value', () => {
  const source = '; personal configuration\r\n[general]\r\nwebui=1\r\ndebug_log=1\r\ncustom=keep\r\n\r\n[dir_setting]\r\ndata=D:\\资料\r\n';
  assert.equal(ini.update(source, { 'general.webui': '0' }), source.replace('webui=1', 'webui=0'));
});

test('lifecycle actions require an explicit opt-in and blank commands by default', () => {
  for (const source of ['', '; existing configuration\n[general]\nwebui=1\n']) {
    const values = ini.read(source);
    assert.equal(values['general.lifecycle_actions'], '0');
    assert.equal(values['general.launch_on_startup'], '');
    assert.equal(values['general.launch_on_exit'], '');
  }
  const script = fs.readFileSync(path.join(__dirname, '../settings.js'), 'utf8');
  const template = script.match(/const template = ('[^\n]+');/)[1];
  assert.deepEqual(ini.read(vm.runInNewContext(template)), ini.defaults);
  for (const key of ['lifecycle_actions=0', 'launch_on_startup=', 'launch_on_exit=']) assert.ok(template.includes(key));
});

test('saving lifecycle commands preserves executable quotes, semicolons, variables and unrelated data', () => {
  const source = '; keep my comment\r\n[general]\r\ncustom=untouched\r\nwebui=0\r\n';
  for (const command of ['"C:\\Program Files\\工具.exe"', '"%app%\\工具.exe" "a;b"', '"%app%\\工具.exe" "O\'Brien"', 'cmd.exe /d /c "echo %TEMP%"', 'powershell.exe -File "%app%\\ready.ps1"']) {
    const values = { 'general.lifecycle_actions': '1', 'general.launch_on_startup': command, 'general.launch_on_exit': command };
    const saved = ini.update(source, values);
    const reloaded = ini.read(ini.decode(ini.encode(saved)));
    for (const [key, value] of Object.entries(values)) assert.equal(reloaded[key], value);
    assert.ok(saved.startsWith(source));
    const off = ini.update(saved, { 'general.lifecycle_actions': '0' });
    assert.equal(off, saved.replace('lifecycle_actions=1', 'lifecycle_actions=0'));
    assert.equal(ini.read(off)['general.launch_on_startup'], command);
  }
});

test('lifecycle navigation and labelled command fields have complete bilingual help', () => {
  const html = fs.readFileSync(path.join(__dirname, '../settings.html'), 'utf8');
  assert.match(html, /href="#general"[\s\S]*href="#lifecycle"[\s\S]*href="#paths"/);
  for (const [id, key] of [['lifecycle-actions', 'lifecycle_actions'], ['startup-action', 'launch_on_startup'], ['exit-action', 'launch_on_exit']]) {
    assert.match(html, new RegExp(`label for="${id}"`));
    assert.match(html, new RegExp(`id="${id}"[^>]+data-setting="general\\.${key}"`));
    if (id !== 'lifecycle-actions') assert.match(html, new RegExp(`id="${id}"[^>]+maxlength="4094"`));
  }
  const context = {};
  vm.runInNewContext(fs.readFileSync(path.join(__dirname, '../i18n.js'), 'utf8'), context);
  for (const language of ['en', 'zh-CN']) {
    for (const key of ['lifecycle', 'lifecycleActions', 'lifecycleHelp', 'startupAction', 'startupActionHelp', 'exitAction', 'exitActionHelp', 'actionCommandHelp']) {
      assert.ok(context.VivaldiMessages[language][key], `${language}: ${key}`);
      assert.ok(html.includes(`data-i18n="${key}"`), key);
    }
  }
});
test('adding several keys to an existing section does not duplicate the section', () => {
  const result = ini.update('[general]\ncustom=keep\n[other]\nx=2\n', { 'general.debug_log': '1', 'general.command_line': '--force-dark-mode' });
  assert.equal((result.match(/\[general\]/g) || []).length, 1);
  assert.equal(ini.read(result)['general.command_line'], '--force-dark-mode');
  assert.ok(result.includes('[other]\nx=2'));
});
test('every static label and status key is available in both languages', () => {
  const context = {};
  vm.runInNewContext(fs.readFileSync(path.join(__dirname, '../i18n.js'), 'utf8'), context);
  const messages = context.VivaldiMessages;
  assert.deepEqual(Object.keys(messages.en).sort(), Object.keys(messages['zh-CN']).sort());
  const html = fs.readFileSync(path.join(__dirname, '../settings.html'), 'utf8');
  for (const match of html.matchAll(/data-i18n="([^"]+)"/g)) {
    for (const language of ['en', 'zh-CN']) assert.ok(messages[language][match[1]], `${language}: ${match[1]}`);
  }
});
test('settings retain useful controls without promotional copy or decorative numbering', () => {
  const html = fs.readFileSync(path.join(__dirname, '../settings.html'), 'utf8');
  const context = {};
  vm.runInNewContext(fs.readFileSync(path.join(__dirname, '../i18n.js'), 'utf8'), context);
  const removed = ['subtitle', 'local', 'offline', 'eyebrow', 'intro', 'generalHelp', 'pathsHelp', 'hotkeyHelp', 'aboutHelp', 'aboutText', 'defaultsBadge', 'footer', 'connectedHelp'];
  for (const messages of Object.values(context.VivaldiMessages)) {
    for (const key of removed) assert.equal(Object.hasOwn(messages, key), false, `Unused translation ${key}`);
    assert.equal(messages.ready, '');
  }
  assert.doesNotMatch(html, /sidebar-bottom|sidebar-caption|section-number|class="eyebrow"|class="pill"|<footer/);
  assert.doesNotMatch(html, /aria-hidden="true">0[1-5]</);
  for (const id of ['webui', 'win32k', 'debug', 'features', 'command', 'data', 'cache', 'boss', 'language', 'theme', 'file-name', 'status']) {
    assert.match(html, new RegExp(`id="${id}"`), `Required control ${id}`);
  }
});
