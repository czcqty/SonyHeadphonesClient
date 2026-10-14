# Qt platform-style validation

The main frontend now imports `QtQuick.Controls` and uses Qt's platform default:
Windows on Windows, macOS on macOS, Fusion on Linux. Existing environment/config
style selection is honored. Qt does not include separate GNOME/KDE/Xfce styles.
The previous hand-painted desktop skins and custom OS appearance adapters have
been removed. Legacy `--preview-desktop` values are preview metadata only.

Buttons, switches, sliders, combo boxes, radio buttons, item delegates, toolbars,
scrollbars and busy indicators use official Qt implementations. Device protocol,
capability checks and commands are unchanged. System fonts/palettes come from Qt.

Theme preferences still migrate from `appearance/darkMode`; preview never reads
or writes user settings. Manual light/dark uses `QStyleHints::setColorScheme`
(Qt 6.8+ and supporting platforms). Qt 6.5-6.7 follows the system and retains the
stored preference without painting a replacement theme.

## Checks

- Build on Windows and run CTest (ViewModels, backend, QML, appearance, tray
  lifecycle, translations, icons and package metadata).
- Exercise all nine routes in both languages with the Windows and Fusion styles.
- Check one business write per switch/slider operation and disabled controls.
- Inspect screenshots of the actual Qt styles, including popup menus.

## Remaining platform acceptance

macOS/Linux builds, actual tray positioning, multiple monitors, scaling,
Wayland compositor behavior and real headset operation still require testing
on the target systems. Previous cross-desktop painted-skin screenshots do not
constitute acceptance of the new official styles.
