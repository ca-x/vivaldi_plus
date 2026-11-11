param([string]$OutputDir)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
if (-not $OutputDir) { $OutputDir = Join-Path $repo 'build/lifecycle-host' }
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
Push-Location $OutputDir
try {
    & cl /nologo /std:c++20 /EHsc /W4 /utf-8 /DUNICODE /D_UNICODE /DNOMINMAX (Join-Path $PSScriptRoot 'lifecycle_action_helper.cpp') /Fe:lifecycle_action_helper.exe
    if ($LASTEXITCODE -ne 0) { throw 'Lifecycle action helper failed to compile' }
    $sources = @('detours.cpp', 'modules.cpp', 'disasm.cpp', 'image.cpp', 'creatwth.cpp', 'disolx64.cpp') | ForEach-Object { Join-Path $repo "detours/src/$_" }
    & cl /nologo /std:c++20 /EHsc /W4 /utf-8 /DUNICODE /D_UNICODE /DNOMINMAX /I (Join-Path $repo 'detours/src') (Join-Path $PSScriptRoot 'lifecycle_host.cpp') /DVIVALDI_PLUS_LIFECYCLE_TEST (Join-Path $repo 'src/lifecycle.cpp') (Join-Path $repo 'src/lifecycle_owners.cpp') (Join-Path $repo 'src/lifecycle_commands.cpp') @sources /Fe:lifecycle_host.exe /link user32.lib shell32.lib
    if ($LASTEXITCODE -ne 0) { throw 'Lifecycle host failed to compile' }
} finally { Pop-Location }


