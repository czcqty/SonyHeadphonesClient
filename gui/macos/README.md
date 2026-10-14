# macOS Swift / AppKit prototype

This branch preserves the Qt frontend and adds a separate Swift executable using
AppKit `NSWindow`, a source-list `NSTableView`, `NSSwitch`, `NSSlider`,
`NSPopUpButton`, `NSScrollView` and native menus. No custom-drawn controls, CSS or
copied macOS visuals. `NSStatusItem` opens an independent transient `NSPopover`
with quick controls and a link to full settings. Native menus/popups retain their
system keyboard, focus and accessibility behavior. Closing settings leaves the
menu-bar item available; Quit or Command-Q stops both UI and backend.

Requires macOS 12+, Xcode command-line tools (Swift 5.5+), and the shared backend
dependencies described in [../native/README.md](../native/README.md).

```sh
sh gui/macos/build.sh
"build/macos/Sound Connect.app/Contents/MacOS/SoundConnect" \
  --backend "$PWD/build/backend/SoundConnectBackend.app/Contents/MacOS/SoundConnectBackend" --locale zh_CN
```

`--panel`, `--route equalizer`, `--profile WF-1000XM5`, `--locale en`, `--live`
are optional. The bundled catalog is located automatically. The app supports system/light/dark via AppKit. Live language/theme choices
persist using the shared backend; simulation and CLI overrides do not save.
Use `--catalog /path/catalog.json` only when developing outside the app bundle.

This is a prototype with the shared device pages and application-settings scope/limitations documented in
`gui/native/README.md`. Signing, notarization, bundled Qt runtime, Bluetooth
permission handling on real hardware, keyboard/VoiceOver acceptance and display
placement remain target-Mac acceptance work. Source and build entry are provided;
this Windows host has no macOS SDK, so AppKit compilation/runtime is not claimed.
The branch's GitHub Actions workflow compiles the Swift frontend on macOS when run.

## Shared backend migration

The adapter now consumes the main branch's `SoundConnectBackend` and
`clients/common/catalog.json`. The former branch-local backend source and
catalog copies have been removed. Advanced device operations are present in
the shared catalog; application/desktop parity still follows the migration gate.
macOS build and runtime acceptance remain unrun: this host has no macOS SDK.

## Preference and desktop follow-up

- The application-settings page supports live persistent language and
  system/light/dark, including migration of the old Qt preferences. Preview
  changes are intentionally session-only. `--locale` / `--theme` override the
  initial session without saving. Both device pages and dynamic choices/errors
  update when switching English/Chinese.
- Ctrl+Alt+P / N / M invoke play/pause, next track and quick controls. Windows
  uses global registration in live mode and reports conflicts with a local
  fallback. Linux shortcuts are application-local, as in the previous Qt UI;
  AppKit uses Control+Option menu equivalents.
- Native dialogs confirm power-off and unpair; cancellation sends no command.
  Separate quick/full layouts, native menu styling and Qt Quick removal gates
  remain unchanged.

These are source changes only on this host: no macOS SDK compilation, AppKit
runtime, menu-bar placement or VoiceOver verification has been performed.
