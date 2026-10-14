# Shared device backend

`device_backend` contains the existing Qt Core device session, profiles,
capability checks and business models. `SoundConnectBackend` hosts it in a
private child process. It does not construct a QGuiApplication, load QML,
choose a widget style, or require Qt Gui/Quick/Widgets. KDE's presentation
continues to use its own Qt/KDE dependencies.

Configure with Qt 6.5+, then build and run CTest:

```sh
cmake -S . -B build -DMDR_BUILD_QT_FRONTEND=OFF -DMDR_ENABLE_CODEGEN=OFF -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```
With the option ON, the transitional Qt Quick application links the same
library, rather than maintaining a second business implementation.

Launch `SoundConnectBackend --live` for real devices, or
`SoundConnectBackend --profile WH-1000XM6` for an isolated simulation.
The default is simulation, for compatibility with existing native previews.
Simulation does not open Bluetooth or read/write user preferences.
Only one live backend per user may own a device session. Preview processes
can run concurrently. The frontend owns the child, drains stdout/stderr and
closes stdin at exit; EOF drains queued requests and shuts down the session.
Diagnostics use stderr. No TCP listener or system service is installed.

For distribution use a Release build. Backend-only CPack produces ZIP on
Windows and TGZ on other hosts. Windows uses CMake runtime dependency scanning
(including Qt Core's dependencies) plus the MSVC redistributable libraries;
it does not require windeployqt or ship a QML/widgets/plugin runtime. Debug
binaries remain developer-machine builds. Native application packaging must
include this backend and preserve its runtime layout.

On macOS the headless process is packaged as a background-only bundle so Qt's
official macdeployqt can deploy its frameworks. Launch the executable at
`build/backend/SoundConnectBackend.app/Contents/MacOS/SoundConnectBackend`, not
the bundle directory. This changes packaging, not the Qt Core process model.
macOS packaging/runtime verification is still pending on a macOS host.

## Pipe protocol v1

UTF-8 JSON objects, one per line. The initial message is a full `type: state`
snapshot with `app`, `connection`, `playback`, `noiseControl`, `equalizer`,
`settings`, `deviceManager`, and `preferences`. Snapshots contain desired values as well as
device-reported metadata. Subsequent snapshots also publish asynchronous
device/connection changes. The preference group contains locale, theme mode and
a persistence flag; system colors, rendered themes and navigation remain frontend-owned.

Example request:

```json
{"version":1,"id":"volume-1","target":"playback","method":"setVolume","args":[12]}
```

Every request produces one `type: result` followed by a full snapshot.
Results echo the optional string `id`, and contain `ok`, `status`, `errorCode`
and `error`. `status: accepted` means the existing business layer accepted
the command, **not** that the headphone acknowledged or applied it. An
asynchronous device failure is reported through connection state/errors.
Do not automatically retry requests after losing the process: commands such
as toggling playback are not idempotent. Restart explicitly, obtain a fresh
snapshot and reconnect. An id correlates replies; it is not a deduplication key.

`version` and `id` may be omitted by existing v1 clients. Argument types are
strict: booleans are not numbers, numeric arguments must be signed 32-bit
integers, and argument counts must match. Capability checks, value handling
and protocol mapping remain in the shared business models.

Send `{"target":"app","method":"describe","args":[]}` to retrieve the
complete allowed device command surface and profile IDs. Application commands
are `refreshDevices`, `connectDevice(name, mac)` and `disconnectDevice`.
QObject methods, arbitrary properties, UI navigation, profile substitution
are not remote device commands. Application preferences use their own target.

Frames are limited to 65536 bytes; malformed/oversized frames are rejected.
At most 64 frames wait for the event loop. The pipe applies backpressure.
Clients must keep consuming output while sending commands. State application
must suppress widget change handlers, so each user operation sends one command.

## Migration gate

See [migration status](../docs/backend-migration.md). Qt Quick removal is
conditional on native feature parity and platform/hardware validation; a
successful simulation or compile alone does not satisfy that condition.

## Application preferences

`preferences.setLocale("en" | "zh_CN")` and
`preferences.setThemeMode("system" | "light" | "dark")` work even while
headphones are disconnected. They use the same QSettings organization/application
(and macOS organization domain) as the transitional frontend. Existing
`appearance/locale` and `appearance/themeMode` are read; when no themeMode exists,
`appearance/darkMode` migrates to a manual light/dark choice. No old theme means
system mode. Invalid stored themes fall back to system. Unrelated keys are kept.
An explicit successful theme save removes the obsolete darkMode key.

Live operation persists changes. Simulation never reads or writes user settings.
`--locale` and `--theme` are session overrides and do not save by themselves;
subsequent explicit GUI choices persist only in live mode. A storage failure
returns `preference-save-failed`, and the previous in-memory preference remains.
Native adapters apply snapshots silently, use their framework's system-theme
mechanism, and send only actual user changes. No Bluetooth operation is involved.
