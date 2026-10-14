# KDE native prototype

C++ Qt Widgets + KDE Frameworks: actual `KPageWidget` settings navigation,
`QCheckBox`, `QSlider`, `QComboBox`, native menus, `QScrollArea` and
`KStatusNotifierItem`. This is KDE's own toolkit family. No hand-painted Breeze
skin or forced QStyle. Plasma supplies its configured widget style, font and
palette. The frontend supports KF6/Qt6 and KF5/Qt5; the device helper independently
retains the repository's Qt 6.5+ requirement.

```sh
# Plasma 5 / Ubuntu 24.04 example; Plasma 6 needs the KF6 equivalents.
sudo apt install cmake g++ qtbase5-dev libkf5widgetsaddons-dev libkf5notifications-dev
cmake -S gui/kde -B build/kde
cmake --build build/kde --parallel
build/kde/SoundConnectKDE --backend "$PWD/build/gui/SonyHeadphonesClient_native_backend"
```

Build the shared host using [../native/README.md](../native/README.md). Optional
`--panel`, `--route`, `--profile`, `--locale en|zh_CN` and explicit `--live`.
The separately laid out quick panel is a native popup. KDE's native tray menu
opens it or full settings. Closing settings hides to tray only when a host is
available; otherwise it quits. Qt handles popup Escape, focus dismissal and
nested combo popups. Popup screen bounds are clamped to the selected display.

```sh
xvfb-run -a dbus-run-session -- build/kde/SoundConnectKDE --backend /absolute/backend --smoke-test
```

The smoke test instantiates all pages and checks startup/state refresh isolation
and one write per toggle/slider update. A KDE Plasma session, KF6 build, tray
positioning on Wayland/multiple displays, keyboard/accessibility and real-device
acceptance are still required. No target desktop appearance is inferred from
Xvfb. See the shared README for remaining prototype feature and packaging work.
