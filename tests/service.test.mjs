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

for (const abandon of [false, true]) {
  test(abandon ? 'standby takes over when the first owner exits inside its readiness callback' : 'standby waits for the first readiness callback before entering the browser', {skip:!host, timeout:25000}, async () => {
    const app = fs.mkdtempSync(path.join(os.tmpdir(), 'vivaldi-plus-startup-barrier-'));
    const children = [];
    const pause = ms => new Promise(resolve => setTimeout(resolve, ms));
    function launch(mode) {
      const child = spawn(host, [app, mode], {stdio:['pipe','pipe','pipe'], windowsHide:true});
      child.output = '';
      child.stdout.on('data', data => { child.output += data; });
      children.push(child);
      return child;
    }
    async function until(check, message) {
      const expires = Date.now() + 7000;
      while (Date.now() < expires) { if (await check()) return; await pause(50); }
      assert.fail(message);
    }
    async function stop(child) {
      if (child.exitCode !== null || child.signalCode !== null) return;
      const exited = new Promise(resolve => child.once('exit', resolve));
      child.kill(); await exited;
    }
    async function tokenAt(url) {
      const response = await fetch(url);
      assert.equal(response.status, 200);
      const token = /name="vivaldi-plus-token"\s+content="([a-f0-9]{64})"/.exec(await response.text())?.[1];
      assert.ok(token);
      return token;
    }
    try {
      const owner = launch('--blocked-ready');
      await until(() => owner.output.includes('callback-start') && /callback-url=(http:\/\/[^\r\n]+)/.test(owner.output), 'Owner did not enter its blocked callback');
      const url = /callback-url=(http:\/\/[^\r\n]+)/.exec(owner.output)?.[1];
      assert.ok(url);
      const token = await tokenAt(url);
      const standby = launch('--service-test');
      await until(() => standby.output.includes('initializing'), 'Standby did not begin initialization');
      await pause(250);
      assert.equal(/^http:\/\//m.test(standby.output), false, 'Standby entered the browser while the about callback was still blocked');
      assert.equal(standby.output.includes('ready-count='), false);
      if (abandon) await stop(owner);
      else owner.stdin.write('continue\n');
      await until(() => /^http:\/\//m.test(standby.output) && standby.output.includes(abandon ? 'ready-count=1' : 'ready-count=0'), 'Standby did not finish initialization after callback completion or owner exit');
      assert.ok(standby.output.includes(url), 'Standby must retain the shared URL');
      assert.ok(standby.output.includes(abandon ? 'ready-count=1' : 'ready-count=0'));
      assert.equal(await tokenAt(url), token, 'Readiness takeover must retain the shared session token');
      assert.deepEqual(fs.readdirSync(app), [], 'Startup barrier must not create disk coordination files');
    } finally {
      for (const child of children) await stop(child);
      fs.rmSync(app, {recursive:true, force:true});
    }
  });
}
