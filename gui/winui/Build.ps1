param(
    [string]$QtBuildDirectory = "cmake-build-debug-visual-studio",
    [string]$OutputDirectory = "cmake-build-winui/app",
    [string]$Dotnet = "dotnet",
    [string]$CMake = "cmake",
    [ValidateSet("Debug", "Release")][string]$Configuration = "Debug"
)
$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path
Push-Location $root
try {
    # Reuse a configured Qt/C++ build and its compiler environment.
    & $CMake -S . -B $QtBuildDirectory -DMDR_BUILD_QT_FRONTEND=OFF
    if ($LASTEXITCODE) { throw "Backend configuration failed" }
    & $CMake --build $QtBuildDirectory --config $Configuration --target SoundConnectBackend --parallel 4
    if ($LASTEXITCODE) { throw "Backend build failed" }
    & $Dotnet restore "$PSScriptRoot/SoundConnect.WinUI.csproj" --locked-mode -p:Platform=x64
    if ($LASTEXITCODE) { throw "WinUI dependency restore failed" }
    & $Dotnet build "$PSScriptRoot/SoundConnect.WinUI.csproj" --no-restore -p:Platform=x64 -c $Configuration -o $OutputDirectory
    if ($LASTEXITCODE) { throw "WinUI build failed" }
    $backend = Join-Path $QtBuildDirectory "backend"
    if (Test-Path (Join-Path $backend "$Configuration/SoundConnectBackend.exe")) {
        $backend = Join-Path $backend $Configuration
    }
    $destination = Join-Path $OutputDirectory "backend"
    New-Item -ItemType Directory -Path $destination -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $backend "SoundConnectBackend.exe") -Destination $destination -Force
    # The existing CMake/vcpkg applocal step supplies the C++ host's runtime.
    Get-ChildItem -LiteralPath $backend -Filter *.dll | Copy-Item -Destination $destination -Force
    Write-Output "Ready: $OutputDirectory/SoundConnect.WinUI.exe"
} finally { Pop-Location }
