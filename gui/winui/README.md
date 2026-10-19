# WinUI experiment

This branch adds a real WinUI 3 frontend alongside the Qt frontend. It uses
Microsoft's NavigationView, InfoBar, RadioButton, ToggleSwitch, Slider,
ComboBox and standard theme resources, without replacement control templates.
The full settings window and tray quick panel have separate layouts. Mica and
Desktop Acrylic use WinUI system backdrops; unsupported systems use their fallback.

The native UI is C#/.NET 8 with Windows App SDK 1.8, x64, unpackaged and
self-contained. The shared `SoundConnectBackend` runs DeviceService, ViewModels, capability
checks and MdrSession with a Qt Core event loop. It has no dependency on
the Qt Quick frontend. Full pages bind the same `clients/common/catalog.json`
used by the other native adapters. JSON
state/commands travel over redirected child-process pipes, with no listening
network port. Protocol and device command mapping are unchanged.

## Build and run

Requires Windows 10 1809+ (Windows 11 recommended), .NET 8 SDK, and the repository's
Qt 6.5+ Core/Test SDK and a C++20 compiler. Run from a Visual Studio developer
PowerShell with `cmake` and `dotnet` on PATH:

```powershell
./gui/winui/Build.ps1
./build/native-winui/app/SoundConnect.WinUI.exe
```

`-Dotnet` and `-CMake` accept absolute paths. `-CMakeConfigureArguments` passes
Qt/toolchain options to a dedicated backend build. Release is the default;
the script rejects a Qt Quick cache or one belonging to another checkout.
It publishes .NET/Windows App SDK, including the generated application PRI,
and stages the shared backend/runtime through CMake install into `app/backend`.

```powershell
./gui/winui/Package.ps1 -AppDirectory build/native-winui/app
./gui/winui/Verify.ps1 -Installer /absolute/native-setup.exe -TestDirectory /absolute/fresh-test-directory
```

Packaging requires NSIS. The per-user installer adds a live-mode Start-menu
entry, optional desktop shortcut and uninstall entry. Uninstall preserves
preferences and unrelated files. `Verify.ps1` exercises installation, reinstall,
English/Chinese smoke tests with a restricted PATH, and removal; it refuses to
replace an existing native installation. Real clean-machine/Windows 10 checks
remain separate. See [the shared package guide](../../packaging/native/README.md).

The default is an isolated WH-1000XM6 simulation. It neither connects Bluetooth
nor saves appearance preferences. `--live` explicitly enables the existing real
device backend; use the Connection page to select a paired device.

```powershell
./build/native-winui/app/SoundConnect.WinUI.exe --panel --theme dark
./build/native-winui/app/SoundConnect.WinUI.exe --locale en --route equalizer
./build/native-winui/app/SoundConnect.WinUI.exe --live
```

Options: `--profile <existing-profile-id>`, `--backend <host-executable>`,
`--locale en|zh_CN`, `--theme system|light|dark`, `--panel`,
`--route home|connection|sound|noise|equalizer|behavior|touch|voice|devices|application` (`appearance` remains an alias).
Left-click the tray icon to toggle quick controls, or use its context menu.
Closing the main window hides it to the tray; Quit terminates both processes.

`--capture <absolute.png>` renders the actual WinUI visual tree and exits. It
also writes a JSON state/command-count sidecar to catch writes during UI startup.
Captures use the official opaque page brush because RenderTargetBitmap cannot
capture the compositor's Mica/Acrylic or nonclient titlebar. They are not mockups.

## Verification and limits

```powershell
python clients/common/test_backend.py cmake-build-debug-visual-studio/backend/SoundConnectBackend.exe
```

The host contract test verifies real shared ViewModel updates, invalid argument
rejection, capability enforcement, preview isolation and orderly EOF shutdown.
All device commands now have native controls, including advanced listening and
speak-to-chat settings, automatic power-off timing, NC-button assignment and
multipoint actions. That is command coverage, not complete production parity:
preferences, migration, dynamic-option localization and Windows global shortcuts
are implemented. Actual desktop/hardware acceptance remains tracked in
`docs/backend-migration.md`.

Run `--smoke-test` with the preview backend to traverse every device page,
check zero synchronization writes, then exercise one toggle and slider commit.
`--smoke-report <absolute.json>` selects the report path. Never use with `--live`.

## Shared-backend verification (2026-10-14)

- Windows 11 x64: .NET 8.0.426 / Windows App SDK 1.8 build succeeds.
- Native widget smoke test passes with `ok: true, sentCommands: 2`.
- The initial shared Qt Core backend check passed 14 CTest suites with the
  transitional Qt UI and six with Qt Quick disabled. After the preference follow-up,
  the 2026-10-15 rerun passed 16/16 and 8/8 respectively (existing Windows builds). The installed headless host also passes
  the process contract test.
- Offline NuGet restore reports unavailable vulnerability metadata (NU1900).
- Real headset operation, multi-monitor/taskbar placement, accessibility,
  high contrast, Windows 10 and clean-machine deployment are not yet accepted.

## Preference and desktop follow-up

- The application-settings page supports live persistent language and
  system/light/dark, including migration of the old Qt preferences. Preview
  changes are intentionally session-only. `--locale` / `--theme` override the
  initial session without saving. Both device pages and dynamic choices/errors
  update when switching English/Chinese.
- Ctrl+Alt+P / N / M invoke play/pause, next track and quick controls. Windows
  uses global registration in live mode and reports conflicts with a local fallback.
- Native dialogs confirm power-off and unpair; cancellation sends no command.
  Separate quick/full layouts, native menu styling and Qt Quick removal gates
  remain unchanged.

## Backend recovery

A valid initial snapshot enables device and preference commands. Missing/failed
children, incompatible responses, EOF and the ten-second startup timeout clear
stale state and expose Retry startup. Pipes are asynchronous, diagnostics are
bounded, and retry obtains fresh state without replaying commands or reconnecting.
Quit gives the owned child two seconds for EOF shutdown. Logs use the user's
SoundConnect data directory. Run `dotnet run --project gui/winui/tests/TransportTests.csproj -c Release`
for transport failure tests. The native smoke also exercises child restart.
