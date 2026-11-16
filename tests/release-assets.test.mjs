import {test} from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import {execFileSync,spawnSync} from 'node:child_process';

const workflow=fs.readFileSync(new URL('../.github/workflows/release.yml',import.meta.url),'utf8');
function script(name){
  const lines=workflow.split(/\r?\n/);
  let at=lines.findIndex(line=>line.trim()===`- name: ${name}`);
  assert.notEqual(at,-1,`Missing release step: ${name}`);
  while(at<lines.length&&lines[at].trim()!=='run: |')at++;
  assert.ok(at<lines.length,`Missing shell script: ${name}`);
  const indent=lines[at].search(/\S/)+2,body=[];
  for(at++;at<lines.length;at++){
    if(lines[at].trim()&&lines[at].search(/\S/)<indent)break;
    body.push(lines[at].slice(indent));
  }
  return body.join('\n');
}

test('release archives retain updater filenames, architecture payloads and reject naming drift',{skip:process.platform==='win32'},()=>{
  const dir=fs.mkdtempSync(path.join(os.tmpdir(),'vivaldi-release-assets-'));
  try{
    for(const arch of ['x64','x86','arm64']){
      const folder=path.join(dir,'dist',`windows_${arch}`);fs.mkdirSync(folder,{recursive:true});
      fs.writeFileSync(path.join(folder,'version.dll'),`payload-${arch}`);
      fs.writeFileSync(path.join(folder,'config.ini.example'),'[general]\nwebui=0\n');
    }
    execFileSync('bash',['-euo','pipefail','-c',script('Prepare release files')],{cwd:dir,stdio:'pipe'});
    const expected=['windows_x64.zip','windows_x86.zip','windows_arm64.zip','chrome-windows-x64.zip','chrome-windows-x86.zip','chrome-windows-arm64.zip'];
    assert.deepEqual(fs.readdirSync(path.join(dir,'release')).sort(),expected.sort());
    const guard=script('Validate updater asset names');
    execFileSync('bash',['-euo','pipefail','-c',guard],{cwd:dir,stdio:'pipe'});
    for(const arch of ['x64','x86','arm64'])for(const asset of [`windows_${arch}.zip`,`chrome-windows-${arch}.zip`]){
      assert.equal(execFileSync('unzip',['-p',path.join(dir,'release',asset),'version.dll'],{encoding:'utf8'}),`payload-${arch}`);
    }
    fs.renameSync(path.join(dir,'release','windows_x64.zip'),path.join(dir,'release','vivaldi-windows-x64.zip'));
    assert.notEqual(spawnSync('bash',['-euo','pipefail','-c',guard],{cwd:dir,stdio:'pipe'}).status,0,'Renaming a public updater asset must fail before publishing');
    fs.renameSync(path.join(dir,'release','vivaldi-windows-x64.zip'),path.join(dir,'release','windows_x64.zip'));
    fs.unlinkSync(path.join(dir,'release','windows_x86.zip'));
    execFileSync('zip',[path.join(dir,'release','windows_x86.zip'),'config.ini.example'],{cwd:path.join(dir,'dist','windows_x86'),stdio:'pipe'});
    assert.notEqual(spawnSync('bash',['-euo','pipefail','-c',guard],{cwd:dir,stdio:'pipe'}).status,0,'An archive without the root version.dll must fail before publishing');
  }finally{fs.rmSync(dir,{recursive:true,force:true});}
});
