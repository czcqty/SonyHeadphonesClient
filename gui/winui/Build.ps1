param(
    [Alias("QtBuildDirectory")][string]$BackendBuildDirectory = "build/native-winui/backend",
    [string]$OutputDirectory = "build/native-winui/app",
    [string]$Dotnet = "dotnet",
    [string]$CMake = "cmake",
    [ValidateSet("Debug", "Release")][string]$Configuration = "Release",
    [string[]]$CMakeConfigureArguments = @()
)
$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path
Push-Location $root
try {
    $BackendBuildDirectory = [IO.Path]::GetFullPath($(if ([IO.Path]::IsPathRooted($BackendBuildDirectory)) { $BackendBuildDirectory } else { Join-Path $root $BackendBuildDirectory }))
    $OutputDirectory = [IO.Path]::GetFullPath($(if ([IO.Path]::IsPathRooted($OutputDirectory)) { $OutputDirectory } else { Join-Path $root $OutputDirectory }))
    $cache = Join-Path $BackendBuildDirectory "CMakeCache.txt"
    if (Test-Path -LiteralPath $cache) {
        $source = (Select-String -LiteralPath $cache -Pattern '^CMAKE_HOME_DIRECTORY:INTERNAL=(.*)$').Matches.Groups[1].Value
        if ([IO.Path]::GetFullPath($source) -ne $root) { throw "Backend build belongs to a different checkout: $source" }
        if (Select-String -LiteralPath $cache -Pattern '^MDR_BUILD_QT_FRONTEND:BOOL=ON$') {
            throw "Use a dedicated backend directory; refusing to reconfigure the transitional Qt frontend build."
        }
    }
    & $CMake -S . -B $BackendBuildDirectory "-DCMAKE_BUILD_TYPE=$Configuration" -DMDR_BUILD_QT_FRONTEND=OFF -DMDR_ENABLE_CODEGEN=OFF -DBUILD_TESTING=ON -DMDR_DEPLOY_BACKEND_RUNTIME=ON -DCMAKE_INSTALL_BINDIR=backend @CMakeConfigureArguments
    if ($LASTEXITCODE) { throw "Backend configuration failed" }
    & $CMake --build $BackendBuildDirectory --config $Configuration --parallel 2
    if ($LASTEXITCODE) { throw "Backend build failed" }
    & $Dotnet restore "$PSScriptRoot/SoundConnect.WinUI.csproj" --locked-mode -p:Platform=x64
    if ($LASTEXITCODE) { throw "WinUI dependency restore failed" }
    & $Dotnet publish "$PSScriptRoot/SoundConnect.WinUI.csproj" --no-restore -p:Platform=x64 -p:PublishSingleFile=false -c $Configuration -o $OutputDirectory
    if ($LASTEXITCODE) { throw "WinUI publish failed" }
    & $CMake --install $BackendBuildDirectory --config $Configuration --prefix $OutputDirectory
    if ($LASTEXITCODE) { throw "Backend runtime deployment failed" }
    Write-Output "Ready: $OutputDirectory/SoundConnect.WinUI.exe"
} finally { Pop-Location }
