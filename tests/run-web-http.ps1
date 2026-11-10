$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$temp = Join-Path ([IO.Path]::GetTempPath()) ('vivaldi-plus-http-tests-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $temp | Out-Null
Push-Location $temp
try {
    & cl /nologo /std:c++20 /EHsc /W4 /utf-8 (Join-Path $PSScriptRoot 'web_http_tests.cpp') (Join-Path $root 'src\web_http.cpp') (Join-Path $root 'src\web_peer.cpp') /Fe:web_http_tests.exe /link ws2_32.lib iphlpapi.lib advapi32.lib
    if ($LASTEXITCODE -ne 0) { throw 'HTTP test compilation failed' }
    & .\web_http_tests.exe
    if ($LASTEXITCODE -ne 0) { throw 'HTTP parser/socket tests failed' }
} finally {
    Pop-Location
    $resolved = [IO.Path]::GetFullPath($temp)
    $tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
    if ($resolved.StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase) -and [IO.Path]::GetFileName($resolved).StartsWith('vivaldi-plus-http-tests-')) {
        Remove-Item -LiteralPath $resolved -Recurse -Force
    }
}
