param(
    [Parameter(Mandatory=$true)][string]$AppDir,
    [Parameter(Mandatory=$true)][string]$SettingsUrl,
    [string]$Version = '1.6.0.0',
    [switch]$Restore,
    [switch]$Startup
)
$ErrorActionPreference = 'Stop'
$app = (Resolve-Path -LiteralPath $AppDir).Path
if (-not (Test-Path -LiteralPath (Join-Path $app 'vivaldi.exe') -PathType Leaf)) { throw 'AppDir must contain vivaldi.exe' }
# The startup caller runs before the browser entry point reads resources.pak.
if (-not $Startup) {
    $running = @(Get-Process -Name vivaldi -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq (Join-Path $app 'vivaldi.exe') })
    if ($running.Count) { throw 'Close this Vivaldi installation before changing its about-page resources.' }
}
Add-Type -Path (Join-Path $PSScriptRoot 'AboutPage.cs')
$directories = @(Get-ChildItem -LiteralPath $app -Directory | Where-Object {
    $_.Name -match '^\d+\.\d+\.\d+\.\d+$' -and (Test-Path -LiteralPath (Join-Path $_.FullName 'resources.pak'))
})
if (-not $directories.Count) { throw 'No supported Vivaldi version directory found.' }
$selected = $directories | Sort-Object { [version]$_.Name } -Descending | Select-Object -First 1
$pakPath = Join-Path $selected.FullName 'resources.pak'
$backup = $pakPath + '.vivaldi-plus-original'
$receipt = $pakPath + '.vivaldi-plus.json'
$currentHash = (Get-FileHash -LiteralPath $pakPath -Algorithm SHA256).Hash
if (Test-Path -LiteralPath $receipt) {
    $record = Get-Content -LiteralPath $receipt -Raw | ConvertFrom-Json
    if (-not (Test-Path -LiteralPath $backup) -or (Get-FileHash -LiteralPath $backup).Hash -ne $record.originalHash) { throw 'Original resource backup is missing or changed.' }
    if ($currentHash -ne $record.patchedHash -and $currentHash -ne $record.originalHash) { throw 'Browser resources changed after integration; refusing to overwrite them.' }
} elseif (Test-Path -LiteralPath $backup) { throw 'Untracked backup found; refusing to overwrite it.' }
if ($Restore) {
    if (-not (Test-Path -LiteralPath $receipt)) { throw 'No Vivaldi++ installation receipt to restore.' }
    [IO.File]::Copy($backup, $pakPath, $true)
    Remove-Item -LiteralPath $receipt
    Remove-Item -LiteralPath $backup
    [IO.File]::WriteAllText((Join-Path $app 'vivaldi-plus-about-disabled'), 'Remove this file to enable automatic about-page integration.')
    Write-Output 'Original about page restored.'
    exit 0
}
$originalPath = if (Test-Path -LiteralPath $backup) { $backup } else { $pakPath }
$original = [IO.File]::ReadAllBytes($originalPath)
$candidate = [VivaldiPlusAboutPage]::Patch($original, $SettingsUrl, $Version)
$candidateHash = [BitConverter]::ToString([Security.Cryptography.SHA256]::Create().ComputeHash($candidate)).Replace('-', '')
if ($currentHash -eq $candidateHash) { Write-Output 'About page already integrated.'; exit 0 }
if (-not (Test-Path -LiteralPath $backup)) { [IO.File]::WriteAllBytes($backup, $original) }
$temp = $pakPath + '.vivaldi-plus-tmp'
try {
    [IO.File]::WriteAllBytes($temp, $candidate)
    [IO.File]::Replace($temp, $pakPath, [System.Management.Automation.Language.NullString]::Value)
    @{ originalHash = (Get-FileHash -LiteralPath $backup).Hash; patchedHash = $candidateHash; version = $Version; settings = $SettingsUrl } |
        ConvertTo-Json | Set-Content -LiteralPath $receipt -Encoding UTF8
} catch {
    # Restore only our own candidate, never overwrite a concurrent updater's file.
    if ((Get-FileHash -LiteralPath $pakPath).Hash -eq $candidateHash) { [IO.File]::Copy($backup, $pakPath, $true) }
    throw
} finally {
    if (Test-Path -LiteralPath $temp) { Remove-Item -LiteralPath $temp }
}
Write-Output ('About page integrated: ' + $pakPath)
if (Test-Path -LiteralPath (Join-Path $app 'vivaldi-plus-about-disabled')) { Remove-Item -LiteralPath (Join-Path $app 'vivaldi-plus-about-disabled') }
