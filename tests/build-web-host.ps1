param([string]$OutputDir)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
if (-not $OutputDir) { $OutputDir = Join-Path $repo 'build/test-host' }
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
Push-Location $OutputDir
try {
    & rc /nologo /i $repo /fo web_host.res (Join-Path $PSScriptRoot 'web_host.rc')
    if ($LASTEXITCODE -ne 0) { throw 'Embedded web test resources failed to compile' }
    & cl /nologo /std:c++20 /EHsc /W4 /utf-8 /DUNICODE /D_UNICODE /DNOMINMAX (Join-Path $PSScriptRoot 'web_host.cpp') (Join-Path $repo 'src/web_settings.cpp') (Join-Path $repo 'src/web_http.cpp') (Join-Path $repo 'src/web_peer.cpp') web_host.res /Fe:web_host.exe /link ws2_32.lib bcrypt.lib iphlpapi.lib advapi32.lib user32.lib
    if ($LASTEXITCODE -ne 0) { throw 'Embedded web test host failed to compile' }
} finally { Pop-Location }
