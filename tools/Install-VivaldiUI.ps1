param(
    [Parameter(Mandatory=$true)][string]$BundlePath,
    [Parameter(Mandatory=$true)][string]$SettingsUrl,
    [Parameter(Mandatory=$true)][string]$Version,
    [switch]$Restore
)
$ErrorActionPreference = 'Stop'
$backup = $BundlePath + '.vivaldi-plus-original'
$receipt = $BundlePath + '.vivaldi-plus.json'
$currentHash = Get-ResourceHash $BundlePath
if (Test-Path -LiteralPath $receipt) {
    $record = Get-Content -LiteralPath $receipt -Raw | ConvertFrom-Json
    if (-not (Test-Path -LiteralPath $backup) -or (Get-ResourceHash $backup) -ne $record.originalHash) { throw 'Vivaldi UI backup is missing or changed.' }
    if ($currentHash -ne $record.patchedHash -and $currentHash -ne $record.originalHash) { throw 'Vivaldi UI changed after integration; refusing to overwrite it.' }
} elseif (Test-Path -LiteralPath $backup) { throw 'Untracked Vivaldi UI backup found.' }
if ($Restore) {
    if (Test-Path -LiteralPath $receipt) {
        [IO.File]::Copy($backup, $BundlePath, $true)
        Remove-Item -LiteralPath $receipt
        Remove-Item -LiteralPath $backup
    }
    return
}
$originalPath = if (Test-Path -LiteralPath $backup) { $backup } else { $BundlePath }
$original = [IO.File]::ReadAllBytes($originalPath)
$candidate = [VivaldiPlusAboutPage]::PatchVivaldiUi($original, $SettingsUrl, $Version)
$sha = [Security.Cryptography.SHA256]::Create()
try { $candidateHash = [BitConverter]::ToString($sha.ComputeHash($candidate)).Replace('-', '') }
finally { $sha.Dispose() }
if ($currentHash -eq $candidateHash) { Write-Output 'Vivaldi native about page already integrated.'; return }
if (-not (Test-Path -LiteralPath $backup)) { [IO.File]::WriteAllBytes($backup, $original) }
$temp = $BundlePath + '.vivaldi-plus-tmp'
$swapped = $BundlePath + '.vivaldi-plus-swapped-' + [Guid]::NewGuid().ToString('N')
$removeSwapped = $false
try {
    [IO.File]::WriteAllBytes($temp, $candidate)
    if ((Get-ResourceHash $BundlePath) -ne $currentHash) { throw 'Vivaldi UI changed during integration.' }
    [IO.File]::Replace($temp, $BundlePath, $swapped)
    if ((Get-ResourceHash $swapped) -ne $currentHash) { throw 'Concurrent Vivaldi UI update detected.' }
    if ((Get-ResourceHash $BundlePath) -ne $candidateHash) { throw 'Vivaldi UI changed before integration completed.' }
    @{originalHash=(Get-ResourceHash $backup);patchedHash=$candidateHash;version=$Version} | ConvertTo-Json | Set-Content -LiteralPath $receipt -Encoding UTF8
    $removeSwapped = $true
} catch {
    if ((Get-ResourceHash $BundlePath) -eq $candidateHash) {
        [IO.File]::Copy($swapped, $BundlePath, $true)
        $removeSwapped = $true
    }
    throw
} finally {
    if (Test-Path -LiteralPath $temp) { Remove-Item -LiteralPath $temp }
    if ($removeSwapped -and (Test-Path -LiteralPath $swapped)) { Remove-Item -LiteralPath $swapped }
    elseif (Test-Path -LiteralPath $swapped) { Write-Warning ('UI recovery copy retained: ' + $swapped) }
}
Write-Output ('Vivaldi native about page integrated: ' + $BundlePath)
