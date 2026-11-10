'use strict';
const test = require('node:test');
const assert = require('node:assert/strict');
const ini = require('../ini.js');

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
test('adding several keys to an existing section does not duplicate the section', () => {
  const result = ini.update('[general]\ncustom=keep\n[other]\nx=2\n', { 'general.debug_log': '1', 'general.command_line': '--force-dark-mode' });
  assert.equal((result.match(/\[general\]/g) || []).length, 1);
  assert.equal(ini.read(result)['general.command_line'], '--force-dark-mode');
  assert.ok(result.includes('[other]\nx=2'));
});
