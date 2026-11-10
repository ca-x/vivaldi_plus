$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$output = Join-Path ([System.IO.Path]::GetTempPath()) ('vivaldi-plus-tests-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $output | Out-Null
try {
    Push-Location $output
    try {
        & cl /nologo /std:c++20 /EHsc /W4 /utf-8 /DUNICODE /D_UNICODE /DNOMINMAX (Join-Path $repo 'tests/native_tests.cpp') /Fe:native_tests.exe /link Shell32.lib Shlwapi.lib
        if ($LASTEXITCODE -ne 0) { throw 'Native regression test compilation failed' }
        foreach ($scenario in @('missing', 'empty', 'custom')) {
            & .\native_tests.exe $scenario
            if ($LASTEXITCODE -ne 0) { throw "Native regression scenario failed: $scenario" }
        }
    } finally { Pop-Location }
} finally {
    $resolved = [System.IO.Path]::GetFullPath($output)
    $tempRoot = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath())
    if (!$resolved.StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe test cleanup path' }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
