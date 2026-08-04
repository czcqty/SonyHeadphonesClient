param(
    [Parameter(Mandatory = $true)]
    [string]$Executable,

    [Parameter(Mandatory = $true)]
    [string]$OutputDirectory,

    [string]$Profile = "WH-1000XM6"
)

$ErrorActionPreference = "Stop"

$resolvedExecutable = (Resolve-Path -LiteralPath $Executable).Path
$resolvedOutput = [System.IO.Path]::GetFullPath($OutputDirectory)
[System.IO.Directory]::CreateDirectory($resolvedOutput) | Out-Null

$routes = @("connection", "home", "settings", "touch", "voice", "noise", "equalizer", "devices")
$locales = @("en", "zh_CN")
$themes = @("light", "dark")
$states = @("ready", "loading", "empty", "error", "disabled")

foreach ($theme in $themes) {
    foreach ($locale in $locales) {
        foreach ($route in $routes) {
            $path = Join-Path $resolvedOutput "$Profile-$route-ready-$locale-$theme.png"
            & $resolvedExecutable `
                --preview-profile $Profile `
                --preview-route $route `
                --preview-state ready `
                --preview-theme $theme `
                --locale $locale `
                --capture $path
            if ($LASTEXITCODE -ne 0) {
                throw "Capture failed: $path"
            }
        }

        foreach ($state in $states) {
            $path = Join-Path $resolvedOutput "$Profile-home-$state-$locale-$theme.png"
            & $resolvedExecutable `
                --preview-profile $Profile `
                --preview-route home `
                --preview-state $state `
                --preview-theme $theme `
                --locale $locale `
                --capture $path
            if ($LASTEXITCODE -ne 0) {
                throw "Capture failed: $path"
            }
        }
    }
}
