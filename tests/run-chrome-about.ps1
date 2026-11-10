param([string]$BinaryOutput = '')
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$output=Join-Path ([IO.Path]::GetTempPath()) ('vivaldi-plus-chrome-tests-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $output | Out-Null
Push-Location $output
try {
    & cl /nologo /std:c++20 /EHsc /W4 /utf-8 (Join-Path $repo 'tests/chrome_about_tests.cpp') (Join-Path $repo 'src/chrome_about.cpp') (Join-Path $repo 'src/gzip_decode.cpp') /Fe:chrome_about_tests.exe
    if($LASTEXITCODE -ne 0){throw 'Chrome about test compilation failed'}
    & .\chrome_about_tests.exe
    if($LASTEXITCODE -ne 0){throw 'Chrome about tests failed'}
    if($BinaryOutput){Copy-Item -LiteralPath .\chrome_about_tests.exe -Destination $BinaryOutput -Force}
} finally {
    Pop-Location
    $resolved=[IO.Path]::GetFullPath($output)
    if(!$resolved.StartsWith([IO.Path]::GetFullPath([IO.Path]::GetTempPath()),[StringComparison]::OrdinalIgnoreCase)){throw 'Unsafe cleanup path'}
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
