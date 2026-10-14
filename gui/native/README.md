# Native frontend experiments

`main` remains the production Qt Quick frontend. `codex/winui` remains a separate
Windows experiment. The macOS, KDE, GNOME and Xfce branches each contain one native
presentation layer, with no replacement painting or copied desktop control skins.

All four branches share this backend commit and `catalog.json`, which contains
only labels, capabilities and existing ViewModel bindings (no visual styling).
The actual widgets, navigation, windowing and quick-panel layouts belong to each
native toolkit. `generate_catalog.py` regenerates the catalog.

The host is adapted from the WinUI branch's tested private JSON-lines pipe bridge.
It runs the existing `AppController`, `ViewModels`, `MdrSession` and libmdr. No
Bluetooth packet format or device command mapping is changed. It remains Qt/C++;
a native presentation layer does not require rewriting the backend.

## Build the shared host

Use the existing platform dependencies and CMake configuration from the root
README (Qt 6.5+, C++20, CMake 3.31+):

```sh
cmake -S . -B build -DMDR_ENABLE_CODEGEN=OFF -DBUILD_TESTING=OFF -DMDR_BUILD_NATIVE_BACKEND=ON
cmake --build build --target SonyHeadphonesClient_native_backend --parallel
python3 gui/native/test_backend.py build/gui/SonyHeadphonesClient_native_backend
python3 gui/native/test_catalog.py build/gui/SonyHeadphonesClient_native_backend
```

Frontends require an explicit `--backend /absolute/path/to/host`. They default to
isolated `WH-1000XM6` simulation. `--profile` selects another existing profile;
only `--live` enables real Bluetooth. `--locale en|zh_CN` selects session language.
Preferences are not persisted. The frontend launches its own child with private
stdin/stdout pipes; EOF requests shutdown. No network listener is used.

`state` events contain ViewModel properties. Commands are objects with `target`,
`method` and `args`; `result` events report acceptance. Unavailable capabilities
disable controls and the backend independently rejects unsupported operations.
State updates must never emit device writes. Sliders commit at the end of an
interaction, not during backend synchronization.

## Prototype scope

Nine full settings pages plus a separate small quick-control layout. This is a
toolkit/layout prototype, not production parity: advanced auto-power-off options,
adaptive sensitivity, NC-button remapping, multipoint management actions, global
shortcuts, persisted settings, packaging and complete backend option translation
remain future work. Connection status/options/error feedback come from the real
shared backend, not frontend mock data. Real headset and native desktop acceptance
are separate from compilation or headless smoke tests.

Framework references:
- macOS: https://developer.apple.com/documentation/appkit
- KDE: https://api.kde.org/kwidgetsaddons-module.html
- GNOME: https://gnome.pages.gitlab.gnome.org/libadwaita/doc/main/
- Xfce: https://developer.xfce.org/libxfce4ui/index.html
