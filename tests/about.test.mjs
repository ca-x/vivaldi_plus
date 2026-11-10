import { test } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { spawnSync } from 'node:child_process';
import { gzipSync } from 'node:zlib';
import { writePak, readPak, decodeResource } from './helpers/pak.mjs';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const template = '<!doctype html><!-- about:version template page --><html><body><table><tr id="executable_path"></tr></table></body></html>';
function fixture(content = template) {
  const header = Buffer.alloc(12);
  header.writeUInt32LE(5); header[4] = 1; header.writeUInt16LE(2, 8); header.writeUInt16LE(1, 10);
  return writePak({ header, entries: [{id:7, bytes:Buffer.from('unrelated')}, {id:42, bytes:gzipSync(content)}], aliases:Buffer.from([43,0,1,0]) });
}
function run(app, ...extra) {
  return spawnSync('powershell.exe', ['-NoProfile', '-NonInteractive', '-ExecutionPolicy', 'Bypass', '-File', path.join(root, 'tools', 'Install-AboutPage.ps1'), '-AppDir', app, '-SettingsUrl', 'http://127.0.0.1:16877/settings.html', '-Startup', ...extra], {encoding:'utf8', timeout:30000});
}
test('production installer preserves resources, reinstalls safely, rejects changed resources, and restores exactly', {skip:process.platform !== 'win32'}, () => {
  const app = fs.mkdtempSync(path.join(os.tmpdir(), 'vivaldi-plus-about-'));
  try {
    fs.writeFileSync(path.join(app, 'vivaldi.exe'), 'fixture');
    fs.mkdirSync(path.join(app, '8.2.1.0'));
    const pakPath = path.join(app, '8.2.1.0', 'resources.pak');
    const original = fixture(); fs.writeFileSync(pakPath, original);
    const a = run(app); assert.equal(a.status, 0, a.stdout + a.stderr);
    const changed = fs.readFileSync(pakPath), parsed = readPak(changed), baseline = readPak(original);
    assert.deepEqual(parsed.entries[0], baseline.entries[0]);
    assert.deepEqual(parsed.aliases, baseline.aliases);
    assert.match(decodeResource(parsed.entries[1].bytes).toString(), /http:\/\/127.0.0.1:16877\/settings.html\?version=/);
    assert.match(decodeResource(parsed.entries[1].bytes).toString(), /id="vivaldi-plus-settings"/);
    const b = run(app); assert.equal(b.status, 0, b.stdout + b.stderr);
    assert.deepEqual(fs.readFileSync(pakPath), changed);
    fs.writeFileSync(pakPath, 'external browser update');
    const c = run(app); assert.notEqual(c.status, 0);
    assert.equal(fs.readFileSync(pakPath, 'utf8'), 'external browser update');
    fs.writeFileSync(pakPath, changed);
    const d = run(app, '-Restore'); assert.equal(d.status, 0, d.stdout + d.stderr);
    assert.deepEqual(fs.readFileSync(pakPath), original);
  } finally { fs.rmSync(app, {recursive:true, force:true}); }
});
test('unsupported template fails without touching the pack or creating a backup', {skip:process.platform !== 'win32'}, () => {
  const app = fs.mkdtempSync(path.join(os.tmpdir(), 'vivaldi-plus-unsupported-'));
  try {
    fs.writeFileSync(path.join(app, 'vivaldi.exe'), 'fixture'); fs.mkdirSync(path.join(app, '1.0.0.0'));
    const pak = path.join(app, '1.0.0.0', 'resources.pak'), original = fixture('unknown browser template');
    fs.writeFileSync(pak, original);
    assert.notEqual(run(app).status, 0);
    assert.deepEqual(fs.readFileSync(pak), original);
    assert.equal(fs.existsSync(pak + '.vivaldi-plus-original'), false);
  } finally { fs.rmSync(app, {recursive:true, force:true}); }
});
