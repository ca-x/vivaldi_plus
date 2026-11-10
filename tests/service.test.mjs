import { test } from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { spawn } from 'node:child_process';

const host = process.env.VIVALDI_PLUS_WEB_HOST;
test('instances share one service and preserve its port and session during takeover', {skip:!host, timeout:25000}, async () => {
  const app = fs.mkdtempSync(path.join(os.tmpdir(), 'vivaldi-plus-instances-'));
  const children = [];
  function launch() {
    const child = spawn(host, [app, '--service-test'], {stdio:['ignore','pipe','pipe'], windowsHide:true});
    child.output = ''; child.stdout.on('data', data => { child.output += data; });
    children.push(child); return child;
  }
  async function until(check, message) {
    const expires = Date.now() + 7000;
    while (Date.now() < expires) { if (await check()) return; await new Promise(resolve => setTimeout(resolve, 50)); }
    assert.fail(message);
  }
  async function stop(child) {
    if (child.exitCode !== null || child.signalCode !== null) return;
    child.kill(); await new Promise(resolve => child.once('exit', resolve));
  }
  const session = async url => {
    const response = await fetch(url); assert.equal(response.status, 200);
    const token = /name="vivaldi-plus-token"\s+content="([a-f0-9]{64})"/.exec(await response.text())?.[1];
    assert.ok(token); return token;
  };
  try {
    const owner = launch();
    await until(() => owner.output.includes('ready-count=1'), 'First owner did not initialize exactly once');
    const url = owner.output.split(/\r?\n/).find(line => line.startsWith('http://'));
    const token = await session(url);
    const standby = launch();
    await until(() => standby.output.includes('ready-count=0'), 'Second instance must remain standby after duplicate initialization');
    assert.ok(standby.output.includes(url));
    assert.equal(await session(url), token);
    assert.equal(standby.output.includes('service-ready'), false, 'Standby must not run another listener');
    await stop(owner);
    await until(() => standby.output.includes('service-ready'), 'Surviving instance did not acquire the abandoned lease');
    await until(async () => { try { return await session(url) === token; } catch { return false; } }, 'Takeover must retain the port and token');
    const response = await fetch(new URL('/api/config', url), {headers:{'X-Vivaldi-Plus-Token':token}});
    assert.equal(response.status, 200);
    assert.deepEqual(fs.readdirSync(app), [], 'Service coordination must not create disk metadata');
  } finally {
    for (const child of children) await stop(child);
    fs.rmSync(app, {recursive:true, force:true});
  }
});
