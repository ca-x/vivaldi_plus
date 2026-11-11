param([string]$BinaryOutput = '')
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$output = Join-Path ([System.IO.Path]::GetTempPath()) ('portable-launch-tests-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $output | Out-Null
try {
    Push-Location $output
    try {
        & cl /nologo /std:c++20 /EHsc /W4 /utf-8 /DUNICODE /D_UNICODE /DNOMINMAX (Join-Path $repo 'tests/portable_launch_tests.cpp') /Fe:portable_launch_tests.exe /link Shell32.lib Shlwapi.lib
        if ($LASTEXITCODE -ne 0) { throw 'Portable launch test compilation failed' }
        if ($BinaryOutput) { Copy-Item -LiteralPath (Join-Path $output 'portable_launch_tests.exe') -Destination $BinaryOutput -Force }
        & .\portable_launch_tests.exe
        if ($LASTEXITCODE -ne 0) { throw 'Portable launch regression tests failed' }
    } finally { Pop-Location }
} finally {
    $resolved = [System.IO.Path]::GetFullPath($output)
    $tempRoot = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath())
    if (!$resolved.StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe test cleanup path' }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
