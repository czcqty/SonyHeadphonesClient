# WinUI experiment

This branch adds a real WinUI 3 frontend alongside the Qt frontend. It uses
Microsoft's NavigationView, InfoBar, RadioButton, ToggleSwitch, Slider,
ComboBox and standard theme resources, without replacement control templates.
The full settings window and tray quick panel have separate layouts. Mica and
Desktop Acrylic use WinUI system backdrops; unsupported systems use their fallback.

The native UI is C#/.NET 8 with Windows App SDK 1.8, x64, unpackaged and
self-contained. The private C++ helper runs the existing AppController,
ViewModels, capability checks and MdrSession with its own Qt event loop. JSON
state/commands travel over redirected child-process pipes, with no listening
network port. Protocol and device command mapping are unchanged.

## Build and run

Requires Windows 10 1809+ (Windows 11 recommended), .NET 8 SDK, and the repository's
existing configured Qt 6.5+ / C++20 build. Run from a Visual Studio developer
PowerShell with `cmake` and `dotnet` on PATH:

```powershell
./gui/winui/Build.ps1 -QtBuildDirectory cmake-build-debug-visual-studio
./cmake-build-winui/app/SoundConnect.WinUI.exe
```

`-Dotnet` and `-CMake` accept absolute paths. The build script restores the locked
NuGet dependency graph and copies the backend host/runtime into `app/backend`.
No system runtime installation or MSIX signing is needed for this preview.
The Debug C++ host still requires a development machine; use a Release Qt build
and the appropriate VC runtime before distributing outside development.

The default is an isolated WH-1000XM6 simulation. It neither connects Bluetooth
nor saves appearance preferences. `--live` explicitly enables the existing real
device backend; use the Devices page to select a paired device.

```powershell
./cmake-build-winui/app/SoundConnect.WinUI.exe --panel --theme dark
./cmake-build-winui/app/SoundConnect.WinUI.exe --locale en --route equalizer
./cmake-build-winui/app/SoundConnect.WinUI.exe --live
```

Options: `--profile <existing-profile-id>`, `--backend <host-executable>`,
`--locale en|zh_CN`, `--theme system|light|dark`, `--panel`,
`--route home|sound|equalizer|behavior|touch|devices|appearance`.
Left-click the tray icon to toggle quick controls, or use its context menu.
Closing the main window hides it to the tray; Quit terminates both processes.

`--capture <absolute.png>` renders the actual WinUI visual tree and exits. It
also writes a JSON state/command-count sidecar to catch writes during UI startup.
Captures use the official opaque page brush because RenderTargetBitmap cannot
capture the compositor's Mica/Acrylic or nonclient titlebar. They are not mockups.

## Verification and limits

```powershell
python gui/winui/test_backend.py cmake-build-debug-visual-studio/gui/SonyHeadphonesClient_winui_backend.exe
```

The host contract test verifies real shared ViewModel updates, invalid argument
rejection, capability enforcement, preview isolation and orderly EOF shutdown.
The native preview is intended to assess the toolkit and layout, not to replace
the production Qt frontend yet. Overview, sound/noise, EQ, basic behavior,
touch/voice, device connection and session appearance are present. Remaining
parity work includes advanced listening/speak-to-chat settings, device management
actions, global shortcuts, persisted preferences and complete option localization.
Real headset operation, multi-monitor/taskbar placement, accessibility tooling,
high-contrast, Windows 10 and clean-machine deployment require further acceptance.

## Local verification (2026-10-14)

- Windows 11 x64: .NET 8.0.426 / Windows App SDK 1.8 build succeeds without warnings.
- Shared Qt/C++ host builds with MSVC and Qt 6.11; all eight existing CTest suites pass.
- Backend contract test passes for supported actions and capability-limited profiles.
- Native WinUI window captures cover all seven routes and the separate quick panel,
  with Chinese/English and light/dark samples. Startup sends zero device commands.
- Qt main also passes its QML suite with the Windows and Fusion official styles.
- macOS/Linux runtime validation and WinUI feature parity are not claimed.

Do not merge the experimental frontend into main until that work is accepted.
