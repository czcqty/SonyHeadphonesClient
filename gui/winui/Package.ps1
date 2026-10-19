param(
    [Parameter(Mandatory=$true)][string]$AppDirectory,
    [string]$OutputDirectory = "build/native-winui/packages",
    [string]$MakeNSIS = "makensis"
)
$ErrorActionPreference = "Stop"
$AppDirectory = (Resolve-Path -LiteralPath $AppDirectory).Path
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
$root = (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path
$version = (Select-String -LiteralPath (Join-Path $root 'CMakeLists.txt') -Pattern '^    VERSION (.+)$').Matches.Groups[1].Value
foreach ($file in 'SoundConnect.WinUI.exe','SoundConnect.WinUI.pri','catalog.json','backend/SoundConnectBackend.exe','backend/Qt6Core.dll') {
    if (!(Test-Path -LiteralPath (Join-Path $AppDirectory $file))) { throw "Missing package input: $file" }
}
if (Get-ChildItem -LiteralPath (Join-Path $AppDirectory 'backend') -Filter 'Qt6*d.dll') { throw 'Debug Qt runtime cannot be distributed' }
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$uninstall = @(Get-ChildItem -LiteralPath $AppDirectory -Recurse -File | ForEach-Object {
    'Delete "$INSTDIR\' + $_.FullName.Substring($AppDirectory.Length + 1).Replace('$', '$$') + '"'
})
$uninstall += @(Get-ChildItem -LiteralPath $AppDirectory -Recurse -Directory | Sort-Object { $_.FullName.Length } -Descending | ForEach-Object {
    'RMDir "$INSTDIR\' + $_.FullName.Substring($AppDirectory.Length + 1).Replace('$', '$$') + '"'
})
$uninstall | Set-Content -LiteralPath (Join-Path $OutputDirectory 'uninstall-files.nsh') -Encoding utf8
& $MakeNSIS "/DAPPDIR=$AppDirectory" "/DOUTDIR=$OutputDirectory" "/DVERSION=$version" "$PSScriptRoot/installer.nsi"
if ($LASTEXITCODE) { throw 'NSIS packaging failed' }
$manifest = [ordered]@{ version=$version; source=(& git -C $root rev-parse HEAD); files=@(Get-ChildItem -LiteralPath $AppDirectory -Recurse -File | ForEach-Object { [ordered]@{path=$_.FullName.Substring($AppDirectory.Length + 1); sha256=(Get-FileHash -LiteralPath $_.FullName).Hash} }) }
$manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $OutputDirectory 'manifest.json') -Encoding utf8
