param(
    [Parameter(Mandatory=$true)][string]$AppDir,
    [Parameter(Mandatory=$true)][string]$SettingsUrl,
    [string]$Version = '1.6.0.0',
    [switch]$Restore,
    [switch]$Startup
)
$ErrorActionPreference = 'Stop'
function Get-ResourceHash([string]$Path) {
    $stream = [IO.File]::OpenRead($Path)
    $hash = [Security.Cryptography.SHA256]::Create()
    try { return [BitConverter]::ToString($hash.ComputeHash($stream)).Replace('-', '') }
    finally { $stream.Dispose(); $hash.Dispose() }
}
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
$currentHash = (Get-ResourceHash $pakPath)
if (Test-Path -LiteralPath $receipt) {
    $record = Get-Content -LiteralPath $receipt -Raw | ConvertFrom-Json
    if (-not (Test-Path -LiteralPath $backup) -or (Get-ResourceHash $backup) -ne $record.originalHash) { throw 'Original resource backup is missing or changed.' }
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
$swapped = $pakPath + '.vivaldi-plus-swapped-' + [Guid]::NewGuid().ToString('N')
$removeSwapped = $false
try {
    [IO.File]::WriteAllBytes($temp, $candidate)
    if ((Get-ResourceHash $pakPath) -ne $currentHash) { throw 'Browser resources changed during integration.' }
    # Capture the exact destination replaced, then verify it as well as the preflight hash.
    [IO.File]::Replace($temp, $pakPath, $swapped)
    if ((Get-ResourceHash $swapped) -ne $currentHash) { throw 'A concurrent browser update was detected; restoring its resources.' }
    if ((Get-ResourceHash $pakPath) -ne $candidateHash) { throw 'Browser resources changed before integration completed.' }
    @{ originalHash = (Get-ResourceHash $backup); patchedHash = $candidateHash; version = $Version; settings = $SettingsUrl } |
        ConvertTo-Json | Set-Content -LiteralPath $receipt -Encoding UTF8
    $removeSwapped = $true
} catch {
    # Restore only our own candidate, never overwrite a concurrent updater's file.
    if ((Get-ResourceHash $pakPath) -eq $candidateHash) {
        $recovery = if (Test-Path -LiteralPath $swapped) { $swapped } else { $backup }
        [IO.File]::Copy($recovery, $pakPath, $true)
        $removeSwapped = $true
    }
    throw
} finally {
    if (Test-Path -LiteralPath $temp) { Remove-Item -LiteralPath $temp }
    if ($removeSwapped -and (Test-Path -LiteralPath $swapped)) { Remove-Item -LiteralPath $swapped }
    elseif (Test-Path -LiteralPath $swapped) { Write-Warning ('Recovery copy retained: ' + $swapped) }
}
Write-Output ('About page integrated: ' + $pakPath)
if (Test-Path -LiteralPath (Join-Path $app 'vivaldi-plus-about-disabled')) { Remove-Item -LiteralPath (Join-Path $app 'vivaldi-plus-about-disabled') }
