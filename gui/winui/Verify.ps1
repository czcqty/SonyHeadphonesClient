param(
    [Parameter(Mandatory=$true)][string]$Installer,
    [Parameter(Mandatory=$true)][string]$TestDirectory
)
$ErrorActionPreference = 'Stop'
$Installer = (Resolve-Path -LiteralPath $Installer).Path
$TestDirectory = [IO.Path]::GetFullPath($TestDirectory)
if (Test-Path -LiteralPath 'HKCU:/Software/SoundConnectWinUI') { throw 'An existing native installation must not be replaced by this test.' }
if (Test-Path -LiteralPath $TestDirectory) { throw 'Use a fresh test directory.' }
New-Item -ItemType Directory -Path $TestDirectory | Out-Null
$install = Join-Path $TestDirectory 'installed'
$reports = Join-Path $TestDirectory 'reports'
New-Item -ItemType Directory -Path $reports | Out-Null
function Run-Installer {
    $process = Start-Process -FilePath $Installer -ArgumentList @('/S', "/D=$install") -Wait -PassThru -WindowStyle Hidden
    if ($process.ExitCode) { throw "Installer exited $($process.ExitCode)" }
}
Run-Installer
try {
    # A file not owned by the package must survive upgrades and uninstall.
    Set-Content -LiteralPath (Join-Path $install 'unrelated-test-file.txt') -Value 'keep'
    Run-Installer
    $shell = New-Object -ComObject WScript.Shell
    $shortcut = $shell.CreateShortcut((Join-Path ([Environment]::GetFolderPath('Programs')) 'Sound Connect (WinUI)/Sound Connect.lnk'))
    if ($shortcut.Arguments -ne '--live') { throw 'Installed shortcut must explicitly select live mode' }
    $oldPath = $env:PATH
    $oldRoot = $env:DOTNET_ROOT
    try {
        $env:PATH = "$env:WINDIR/System32"
        $env:DOTNET_ROOT = Join-Path $TestDirectory 'no-sdk'
        $env:DOTNET_MULTILEVEL_LOOKUP = '0'
        foreach ($locale in 'en','zh_CN') {
            $report = Join-Path $reports "$locale.json"
            $process = Start-Process -FilePath (Join-Path $install 'SoundConnect.WinUI.exe') -WorkingDirectory $TestDirectory -ArgumentList @('--locale', $locale, '--parity-test', '--smoke-report', ('"' + $report + '"')) -PassThru -WindowStyle Hidden
            if (!$process.WaitForExit(180000)) { $process.Kill($true); throw 'Native parity checks timed out' }
            if ($process.ExitCode -or !(Test-Path -LiteralPath $report)) { throw "Native smoke failed: $locale" }
            $result = Get-Content -LiteralPath $report -Raw | ConvertFrom-Json
            if (!$result.ok) { throw "Native controls failed: $locale" }
            if (@($result.parity | Where-Object { $_.profile -and $_.ok }).Count -ne 12 -or
                !($result.parity | Where-Object { $_.commands -eq 43 -and $_.quickControls -and $_.failureUiAndRetry })) {
                throw "Native verification did not cover all profiles and operations: $locale"
            }
        }
    } finally { $env:PATH = $oldPath; $env:DOTNET_ROOT = $oldRoot }
} finally {
    if (Test-Path -LiteralPath (Join-Path $install 'Uninstall.exe')) {
        $process = Start-Process -FilePath (Join-Path $install 'Uninstall.exe') -ArgumentList @('/S', "_?=$install") -Wait -PassThru -WindowStyle Hidden
        if ($process.ExitCode) { throw 'Uninstall failed' }
    }
}
if (Test-Path -LiteralPath (Join-Path $install 'SoundConnect.WinUI.exe')) { throw 'Uninstall left the application installed' }
if (!(Test-Path -LiteralPath (Join-Path $install 'unrelated-test-file.txt'))) { throw 'Uninstall removed an unrelated file' }
Write-Output 'PASS: install, upgrade, default backend lookup, English/Chinese native parity checks on 12 profiles, no SDK path and uninstall'
