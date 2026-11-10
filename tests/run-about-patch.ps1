$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$output = Join-Path ([IO.Path]::GetTempPath()) ('vivaldi-plus-about-tests-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $output | Out-Null
try {
    Push-Location $output
    try {
        & cl /nologo /std:c++20 /EHsc /W4 /utf-8 /DNOMINMAX /DUNICODE /D_UNICODE (Join-Path $repo 'tests/about_patch_tests.cpp') (Join-Path $repo 'src/about_patch.cpp') (Join-Path $repo 'src/about_integration.cpp') (Join-Path $repo 'src/chrome_about.cpp') (Join-Path $repo 'src/gzip_decode.cpp') /Fe:about_patch_tests.exe /link Bcrypt.lib Shlwapi.lib
        if ($LASTEXITCODE -ne 0) { throw 'About patch test compilation failed' }
        & .\about_patch_tests.exe
        if ($LASTEXITCODE -ne 0) { throw 'About patch tests failed' }
    } finally { Pop-Location }
} finally {
    $resolved = [IO.Path]::GetFullPath($output)
    if (!$resolved.StartsWith([IO.Path]::GetFullPath([IO.Path]::GetTempPath()), [StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe cleanup path' }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
