# SonyHeadphonesClient

An unofficial, open-source Qt Quick desktop application for controlling the
local features of supported Sony headphones without the mobile-only Sound
Connect application.

The application uses a fixed portrait QML interface and keeps all device
communication in the existing C++ `libmdr` backend. Account, cloud, location,
activity-learning, third-party service, and firmware-update flows are outside
the application scope.

## Features

- Noise cancelling, Ambient Sound Control, ambient level, and Focus on Voice
- Device-reported equalizer band layouts, Clear Bass, and Flat/Custom editing
- Playback, volume, left/right/case battery, and track information
- DSEE, Speak-to-Chat on/off, BGM listening mode, auto power-off, and wearing
  pause
- Touch/button assignments, head gestures, and voice-guidance on/off and volume
- Pairing-mode and multipoint playback-target protocol commands when advertised
- MDR v2 connection with automatic MDR v1 fallback for implemented legacy
  operations
- Fixed 412 × 780 logical-pixel preview surface with system DPI scaling
- Light/dark themes plus English and Simplified Chinese UI text, with persistent
  in-app theme and language switches

Optional device settings are capability-driven. Unsupported operations are
hidden or disabled and must not enqueue protocol commands.

## Current scope and known gaps

The current GUI is complete enough for deterministic preview and local protocol
development, but several decoded backend features are not yet finished as user
features. Safe Listening has no QML surface, XM6 noise adaptation is not exposed,
and the Devices page still uses placeholder paired-device rows rather than the
live `libmdr` paired-device list. That page currently exercises pairing mode and
playback-target selection only; it is not complete device management.

MDR v1 fallback currently reads and writes Noise Cancelling/Ambient Sound state.
Other legacy operations remain unintegrated. See the
[parity roadmap](docs/official-parity-roadmap.md) for the implementation boundary
used by the project.

## Deterministic preview

Every route and state can be opened without Bluetooth hardware:

```powershell
SonyHeadphonesClient.exe `
  --preview-profile WH-1000XM6 `
  --preview-route home `
  --preview-state ready `
  --locale zh_CN
```

Routes:

```text
connection, home, settings, touch, voice, noise, equalizer, devices
```

States:

```text
ready, loading, empty, error, disabled
```

Add `--capture <path.png>` to save the window as a deterministic PNG and exit.
Available profile identifiers are documented in
[the GUI developer notes](gui/README.md).

## Supported devices

Support is documented from real-device reports rather than inferred from
bundled artwork or preview profiles. See the
[device support index](docs/README.md) and the individual matrices under
`docs/device-support/`.

The gap against Sony Sound Connect, licensing boundaries, and verification
requirements are recorded in
[the parity roadmap](docs/official-parity-roadmap.md).

## Build

### Requirements

- CMake 3.31 or newer
- A C++20 compiler
- Qt 6.5 or newer with LinguistTools, Qml, Quick, and QuickControls2
- Qt Test and QuickTest when `BUILD_TESTING=ON`
- Git and network access during the first configure so CMake can fetch `fmt`
- Linux only: BlueZ and D-Bus development packages

CI uses Qt 6.8.x; the release workflow is pinned to Qt 6.8.3.

On Debian or Ubuntu:

```bash
sudo apt install libbluetooth-dev libdbus-1-dev
```

### Configure and compile

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DMDR_ENABLE_CODEGEN=OFF
cmake --build build --config RelWithDebInfo --target SonyHeadphonesClient
```

On Windows, run the commands from a Visual Studio Developer PowerShell or
Developer Command Prompt. Set `CMAKE_PREFIX_PATH` or use a package-manager
toolchain when Qt is not on CMake's search path.

The Windows development build deploys its Qt Quick modules, platform plugin,
and Qt DLLs beside the executable so the CLion target runs without custom Qt
plugin environment variables. A distributable release still requires normal
release packaging and the MSVC redistributable.

Generated protocol sources are committed. Enable `MDR_ENABLE_CODEGEN` only when
LLVM/libclang is installed and protocol generation is intentionally required.

### Language selection

The floating `中` / `EN` button switches the running interface immediately.
Normal launches use an explicit `--locale` value first, then the saved choice,
then the operating-system language. Preview launches default to English when
`--locale` is omitted so captures remain deterministic.

### Release packages

The package identity is `io.github.czcqty.SonyHeadphonesClient`; the product,
display, and executable names are all `SonyHeadphonesClient`.

- Windows x64 uses the CPack NSIS generator and produces an EXE installer.
- macOS builds `x86_64;arm64` and uses the CPack DragNDrop generator for DMG.
- Linux x86_64 uses `packaging/package_appimage.sh` and linuxdeploy.

Run `cpack --config build/CPackConfig.cmake -C Release -B dist` after a Release
build to package Windows or macOS locally. The GitHub `Package release`
workflow builds all three platforms manually or for a `vX.Y.Z` tag. Tag builds
create a release only when the tag matches the CMake project version.

The packages are currently unsigned. Windows SmartScreen and macOS Gatekeeper
may therefore show security warnings.

### Tests

```bash
cmake --build build --config RelWithDebInfo --target \
  SonyHeadphonesClient_viewmodel_tests \
  SonyHeadphonesClient_backend_tests \
  SonyHeadphonesClient_qml_tests \
  SonyHeadphonesClient_visual_diff
ctest --test-dir build -C RelWithDebInfo --output-on-failure
```

`run_gui_tests` builds and runs the three GUI/backend regression suites.
When Python is available, CTest also validates the translation catalog,
application icons, and package metadata. `capture_qml_baselines` captures the
Windows route/state/locale/theme preview matrix.

### CMake options

| Option | Default | Purpose |
| --- | --- | --- |
| `BUILD_TESTING` | `ON` | Build ViewModel, backend, and QML regression tests |
| `MDR_BUILD_WITH_ASAN` | `OFF` | Enable AddressSanitizer on supported compilers |
| `MDR_DEBUG` | `OFF` | Enable additional `libmdr` diagnostics |
| `MDR_ENABLE_CODEGEN` | `ON` | Enable LLVM/libclang protocol regeneration |

## Repository layout

| Path | Purpose |
| --- | --- |
| [`gui/`](gui/README.md) | Qt Quick application, ViewModels, QML, tests, and device artwork |
| `packaging/` | Icon generation, package metadata, validation, and AppImage tooling |
| `libmdr/` | MDR protocol implementation and platform Bluetooth backends |
| `tooling/` | LLVM-based enum, traits, serialization, and validation generators |
| `contrib/` | CMake declarations for fetched third-party dependencies |
| `docs/` | Device support matrices, reference measurements, and maintenance notes |

The application depends on `libmdr`; `libmdr` selects a Windows, Linux, or
macOS Bluetooth transport backend at configure time.

## License and credits

The project is licensed under the [MIT License](LICENSE).

- Original SonyHeadphonesClient by
  [Plutoberth](https://github.com/Plutoberth/SonyHeadphonesClient)
- MDR protocol and cross-platform work by the SonyHeadphonesClient community
- Legacy MDR v1 connection and NC/ASM behavior adapted from SonyBridge under
  the MIT License; see `libmdr/THIRD_PARTY_NOTICES.md`
- Device artwork adapted from SonyBridge under the MIT License; see
  `gui/assets/SONYBRIDGE_LICENSE.txt`
