import { test } from 'node:test';
import assert from 'node:assert/strict';
import { spawn } from 'node:child_process';
import { mkdtempSync, mkdirSync, readFileSync, copyFileSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { resolve, join } from 'node:path';
const build = resolve(process.env.LIFECYCLE_HOST_DIR || 'build/lifecycle-host');
const pause = ms => new Promise(r => setTimeout(r, ms));
function fixture(t) {
  const hosts = [];
  t.after(async () => {
    for (const host of hosts) { if (host.exitCode === null) host.kill(); await host.done; }
    rmSync(dir, {recursive:true,force:true,maxRetries:6,retryDelay:1000});
  });
  const dir = mkdtempSync(join(tmpdir(), 'lifecycle-中文 '));
  const app = join(dir, 'app'); mkdirSync(app);
  const profile = join(dir, 'profile'); mkdirSync(profile);
  const log = join(dir, 'actions.txt');
  copyFileSync(join(build, 'lifecycle_action_helper.exe'), join(app, 'helper.exe'));
  copyFileSync(join(build, 'lifecycle_host.exe'), join(app, 'host.exe'));
  const command = label => `"%app%\\helper.exe" "${log}" ${label}`;
  const lines = () => { try { return readFileSync(log, 'utf8').trim().split('\n'); } catch { return []; } };
  const run = (settings = {}) => {
    const p = spawn(join(app, 'host.exe'), [app, settings.profile ?? profile, settings.enabled ?? 'on', settings.start ?? command('start'), settings.exit ?? command('exit'), String(settings.duration ?? 1500), settings.hold ? 'hold' : ''], {cwd:settings.cwd,env:{...process.env,...settings.env,LIFECYCLE_EXPECT_CWD:app,LIFECYCLE_TEST_LABEL:'expanded'}});
    p.done = new Promise((r, reject) => { p.on('error', reject); p.on('exit', r); });
    p.ready = new Promise(r => p.stdout.once('data', r));
    hosts.push(p);
    return p;
  };
  return { dir, app, profile, command, lines, run };
}
async function until(check, timeout = 6000) {
  const deadline = Date.now() + timeout;
  while (!check()) { if (Date.now() > deadline) assert.fail('Timed out waiting for lifecycle action'); await pause(30); }
}
test('first owner starts once and last owner exits once', {timeout:20000}, async (t) => {
  const f = fixture(t); const a = f.run({duration: 1000});
  await until(() => f.lines().length === 1);
  const second = join(f.dir, 'profile2'); mkdirSync(second);
  const b = f.run({profile: second, duration: 2200});
  await a.done; assert.deepEqual(f.lines(), ['start']);
  await b.done; assert.deepEqual(f.lines(), ['start', 'exit']);
});
test('disabled, empty, forwarder and non-directory window are inert', {timeout:20000}, async (t) => {
  for (const settings of [{enabled:'off'}, {start:'',exit:''}, {profile:'none'}, {profile:'https://example.invalid/'}]) {
    const f = fixture(t); await f.run({...settings,duration:500}).done; assert.deepEqual(f.lines(), []);
  }
});
test('dead PID is cleaned while another owner retains shared memory', {timeout:20000}, async (t) => {
  const f = fixture(t); const a = f.run({duration:10000});
  await until(() => f.lines().length === 1);
  const second = join(f.dir, 'profile2'); mkdirSync(second);
  const keeper = f.run({profile:second,duration:1800}); await pause(500);
  a.kill(); await a.done; await keeper.done;
  const b = f.run({duration:500}); await b.done;
  assert.deepEqual(f.lines(), ['start', 'exit', 'start', 'exit']);
});
test('restart waits for previous exit command before starting', {timeout:20000}, async (t) => {
  const f = fixture(t); const a = f.run({duration:600, exit:f.command('exit') + ' 1000'});
  await until(() => f.lines().includes('exit'));
  const started = Date.now(); const b = f.run({duration:2000});
  await until(() => f.lines().filter(x=>x==='start').length===2);
  assert.ok(Date.now()-started >= 700);
  await Promise.all([a.done,b.done]); assert.deepEqual(f.lines(), ['start','exit','start','exit']);
});
test('exit wait is bounded to three seconds without killing the command', {timeout:20000}, async (t) => {
  const f = fixture(t); const started = Date.now();
  const a = f.run({duration:500,exit:f.command('exit')+' 5000 finished'});
  await a.done; const elapsed = Date.now()-started;
  assert.ok(elapsed >= 3000 && elapsed < 4800, `elapsed=${elapsed}`);
  assert.deepEqual(f.lines(), ['start','exit']);
  await until(() => f.lines().includes('finished'));
});

test('bare executable and environment substitutions preserve application cwd', {timeout:20000}, async (t) => {
  const f = fixture(t);
  const start = f.command('%LIFECYCLE_TEST_LABEL%').replace('"%app%\\helper.exe"', 'helper.exe');
  await f.run({start,duration:700}).done;
  assert.deepEqual(f.lines(), ['expanded','exit']);
});


test('startup-only and exit-only configurations remain active', {timeout:20000}, async (t) => {
  const startOnly = fixture(t);
  await startOnly.run({exit:'',duration:600}).done;
  assert.deepEqual(startOnly.lines(), ['start']);
  const exitOnly = fixture(t);
  await exitOnly.run({start:'',duration:600}).done;
  assert.deepEqual(exitOnly.lines(), ['exit']);
});

test('bare system executable ignores malicious inherited cwd and relative PATH entries', {timeout:20000}, async (t) => {
  const f = fixture(t);
  const hostile = join(f.dir, 'downloads'); mkdirSync(hostile);
  copyFileSync(join(build, 'lifecycle_action_helper.exe'), join(hostile, 'cmd.exe'));
  const log = join(f.dir, 'genuine.txt');
  const start = `cmd.exe /d /c echo genuine>>"${log}"`;
  await f.run({start,cwd:hostile,env:{PATH:`.;relative;${process.env.PATH}`},duration:900}).done;
  assert.equal(readFileSync(log, 'utf8').trim(), 'genuine');
  assert.deepEqual(f.lines(), ['exit']);
});

test('existing owner adopts a window owner blocked before registration', {timeout:20000}, async (t) => {
  const f = fixture(t);
  const a = f.run({duration:10000});
  await until(() => f.lines().length === 1);
  const profile = join(f.dir, 'pending-profile'); mkdirSync(profile);
  const b = f.run({profile,hold:true,duration:10000});
  await b.ready;
  a.stdin.write('exit\n'); await a.done;
  assert.deepEqual(f.lines(), ['start']);
  b.stdin.write('exit\n'); await b.done;
  assert.deepEqual(f.lines(), ['start','exit']);
});

test('adopted owner can later register without duplicate startup or duplicate record', {timeout:20000}, async (t) => {
  const f = fixture(t);
  const a = f.run({duration:10000});
  await until(() => f.lines().length === 1);
  const profile = join(f.dir, 'pending-profile'); mkdirSync(profile);
  const b = f.run({profile,hold:true,duration:10000});
  await b.ready;
  a.stdin.write('exit\n'); await a.done;
  const registered = new Promise(r => b.stdout.once('data', r));
  b.stdin.write('join\n'); await registered;
  assert.deepEqual(f.lines(), ['start']);
  b.stdin.write('exit\n'); await b.done;
  assert.deepEqual(f.lines(), ['start','exit']);
});

test('window adoption excludes another installation and same-install forwarder', {timeout:20000}, async (t) => {
  const f = fixture(t); const other = fixture(t);
  const a = f.run({duration:10000}); const separate = other.run({duration:10000});
  await until(() => f.lines().length === 1 && other.lines().length === 1);
  const forwarder = f.run({profile:'none',duration:10000}); await forwarder.ready;
  a.stdin.write('exit\n'); await a.done;
  assert.deepEqual(f.lines(), ['start','exit']);
  assert.deepEqual(other.lines(), ['start']);
  forwarder.stdin.write('exit\n'); await forwarder.done;
  assert.deepEqual(f.lines(), ['start','exit']);
  separate.stdin.write('exit\n'); await separate.done;
  assert.deepEqual(other.lines(), ['start','exit']);
});

