$ErrorActionPreference = 'Stop'
$temp = Join-Path ([IO.Path]::GetTempPath()) ('vivaldi-plus-config-tests-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $temp | Out-Null
Push-Location $temp
try {
    & cl /nologo /std:c++20 /EHsc /W4 /utf-8 /DUNICODE /D_UNICODE (Join-Path $PSScriptRoot 'web_config_tests.cpp') /Fe:web_config_tests.exe /link bcrypt.lib user32.lib
    if ($LASTEXITCODE -ne 0) { throw 'Configuration test compilation failed' }
    & .\web_config_tests.exe
    if ($LASTEXITCODE -ne 0) { throw 'Configuration atomic-save tests failed' }
} finally {
    Pop-Location
    $resolved = [IO.Path]::GetFullPath($temp)
    $tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
    if ($resolved.StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase) -and [IO.Path]::GetFileName($resolved).StartsWith('vivaldi-plus-config-tests-')) {
        Remove-Item -LiteralPath $resolved -Recurse -Force
    }
}
