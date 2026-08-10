param(
    [Parameter(Mandatory = $true)]
    [string]$Executable,

    [Parameter(Mandatory = $true)]
    [string]$OutputDirectory,

    [string[]]$Profiles = @("WH-1000XM6", "WF-1000XM6", "WF-C510", "WF-L910"),

    [string[]]$ViewportSizes = @("360x600", "412x780", "720x900")
)

$ErrorActionPreference = "Stop"

$resolvedExecutable = (Resolve-Path -LiteralPath $Executable).Path
$resolvedOutput = [System.IO.Path]::GetFullPath($OutputDirectory)
[System.IO.Directory]::CreateDirectory($resolvedOutput) | Out-Null

$routes = @("connection", "home", "settings", "touch", "voice", "noise", "equalizer", "devices")
$locales = @("en", "zh_CN")
$themes = @("light", "dark")
$states = @("ready", "loading", "empty", "error", "disabled")

foreach ($profile in $Profiles) {
    foreach ($viewportSize in $ViewportSizes) {
        foreach ($theme in $themes) {
            foreach ($locale in $locales) {
                foreach ($route in $routes) {
                    $path = Join-Path $resolvedOutput "$profile-$route-ready-$locale-$theme-$viewportSize.png"
                    & $resolvedExecutable `
                        --preview-profile $profile `
                        --preview-route $route `
                        --preview-state ready `
                        --preview-theme $theme `
                        --preview-size $viewportSize `
                        --locale $locale `
                        --capture $path
                    if ($LASTEXITCODE -ne 0) {
                        throw "Capture failed: $path"
                    }
                }

                foreach ($state in $states) {
                    $path = Join-Path $resolvedOutput "$profile-home-$state-$locale-$theme-$viewportSize.png"
                    & $resolvedExecutable `
                        --preview-profile $profile `
                        --preview-route home `
                        --preview-state $state `
                        --preview-theme $theme `
                        --preview-size $viewportSize `
                        --locale $locale `
                        --capture $path
                    if ($LASTEXITCODE -ne 0) {
                        throw "Capture failed: $path"
                    }
                }
            }
        }
    }
}
