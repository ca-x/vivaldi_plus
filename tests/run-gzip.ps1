param([string]$BinaryOutput = '')
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$output = Join-Path ([IO.Path]::GetTempPath()) ('vivaldi-plus-gzip-tests-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $output | Out-Null
try {
    Push-Location $output
    try {
        & cl /nologo /std:c++20 /EHsc /W4 /utf-8 /DNOMINMAX (Join-Path $repo 'tests/gzip_tests.cpp') (Join-Path $repo 'src/gzip_decode.cpp') /Fe:gzip_tests.exe
        if ($LASTEXITCODE -ne 0) { throw 'Gzip decoder test compilation failed' }
        & .\gzip_tests.exe
        if ($LASTEXITCODE -ne 0) { throw 'Gzip decoder tests failed' }
        @'
const fs=require('node:fs'),z=require('node:zlib');
const random=Buffer.alloc(65536);let seed=0x12345678;
for(let i=0;i<random.length;i++){seed=(Math.imul(seed,1664525)+1013904223)>>>0;random[i]=seed>>>24;}
const data=Buffer.concat([random,Buffer.from('界面设置 Vivaldi++\n'.repeat(12000)),random.subarray(32768)]);
fs.writeFileSync('payload.bin',data);
for(const level of [0,1,6,9])fs.writeFileSync('node-'+level+'.gz',z.gzipSync(data,{level}));
'@ | node -
        if ($LASTEXITCODE -ne 0) { throw 'Node gzip corpus generation failed' }
        foreach ($level in @(0, 1, 6, 9)) {
            & .\gzip_tests.exe "node-$level.gz" "decoded-$level.bin"
            if ($LASTEXITCODE -ne 0) { throw "Node gzip corpus decode failed at level $level" }
        }
        & .\gzip_tests.exe --encode payload.bin native-stored.gz
        if ($LASTEXITCODE -ne 0) { throw 'Native stored gzip encode failed' }
        @'
const fs=require('node:fs'),z=require('node:zlib'),assert=require('node:assert/strict');
const expected=fs.readFileSync('payload.bin');
for(const level of [0,1,6,9])assert.deepEqual(fs.readFileSync('decoded-'+level+'.bin'),expected);
assert.deepEqual(z.gunzipSync(fs.readFileSync('native-stored.gz')),expected);
console.log('PASS Node zlib differential corpus: levels 0/1/6/9 + native stored encoder');
'@ | node -
        if ($LASTEXITCODE -ne 0) { throw 'Node zlib differential verification failed' }
        if ($BinaryOutput) { Copy-Item -LiteralPath .\gzip_tests.exe -Destination $BinaryOutput -Force }
    } finally { Pop-Location }
} finally {
    $resolved = [IO.Path]::GetFullPath($output)
    if (!$resolved.StartsWith([IO.Path]::GetFullPath([IO.Path]::GetTempPath()), [StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe cleanup path' }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
