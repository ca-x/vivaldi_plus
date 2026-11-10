import { test } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { spawn } from 'node:child_process';

const host = process.env.VIVALDI_PLUS_WEB_HOST;
test('embedded DLL settings server reads, authenticates, writes and rejects stale configuration', {skip: !host, timeout:20000}, async () => {
  const app = fs.mkdtempSync(path.join(os.tmpdir(), 'vivaldi-plus-api-'));
  const child = spawn(host, [app], {stdio:['ignore', 'pipe', 'pipe'], windowsHide:true});
  let stderr = ''; child.stderr.on('data', data => { stderr += data; });
  try {
    const url = await new Promise((resolve, reject) => {
      let output = '';
      const timer = setTimeout(() => reject(new Error('No server endpoint: ' + stderr)), 5000);
      child.once('error', reject);
      child.once('exit', code => { clearTimeout(timer); reject(new Error(`Host exited ${code}: ${stderr}`)); });
      child.stdout.on('data', data => { output += data; if (output.includes('\n')) { clearTimeout(timer); resolve(output.trim()); } });
    });
    const origin = new URL(url).origin;
    const html = await fetch(url);
    assert.equal(html.status, 200);
    assert.equal(html.headers.get('x-frame-options'), 'DENY');
    assert.match(html.headers.get('content-security-policy'), /frame-ancestors 'none'/);
    const text = await html.text();
    const token = /name="vivaldi-plus-token"\s+content="([a-f0-9]{64})"/.exec(text)?.[1];
    assert.ok(token, 'Embedded HTML must contain the current session token');
    const auth = {'X-Vivaldi-Plus-Token':token};
    assert.equal((await fetch(origin + '/api/config')).status, 403);
    assert.equal((await fetch(origin + '/settings.css')).status, 200);
    assert.equal((await fetch(origin + '/not-a-resource')).status, 404);
    let config = await (await fetch(origin + '/api/config', {headers:auth})).json();
    assert.equal(config.exists, false); assert.equal(config.revision, 'missing');
    const ini = '; comment\r\n[dir_setting]\r\ndata=..\\中文数据\r\n[unknown]\r\nkeep=1\r\n';
    const bytes = Buffer.concat([Buffer.from([0xff, 0xfe]), Buffer.from(ini, 'utf16le')]);
    const put = (revision, extra={}) => fetch(origin + '/api/config', {method:'PUT', body:bytes, headers:{...auth, Origin:origin, 'Content-Type':'application/octet-stream', 'X-Vivaldi-Plus-Revision':revision, ...extra}});
    assert.equal((await put('missing', {Origin:'https://example.com'})).status, 403);
    assert.equal((await put('stale')).status, 409);
    const response = await put('missing'); assert.equal(response.status, 200);
    config = await response.json(); assert.equal(config.text, ini); assert.equal(config.exists, true);
    assert.deepEqual(fs.readFileSync(path.join(app, 'config.ini')), bytes);
    fs.appendFileSync(path.join(app, 'config.ini'), Buffer.from('; external edit\r\n', 'utf16le'));
    assert.equal((await put(config.revision)).status, 409);
    assert.match(fs.readFileSync(path.join(app, 'config.ini')).toString('utf16le'), /external edit/);
    assert.equal((await fetch(origin + '/api/config', {method:'PUT', headers:{...auth, Origin:origin, 'Content-Type':'application/octet-stream'}, body:'not UTF16'})).status, 400);
  } finally {
    child.kill();
    await new Promise(resolve => child.exitCode === null ? child.once('exit', resolve) : resolve());
    fs.rmSync(app, {recursive:true, force:true});
  }
});
