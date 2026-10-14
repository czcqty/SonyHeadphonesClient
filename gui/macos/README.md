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
  --backend "$PWD/build/gui/SonyHeadphonesClient_native_backend" --locale zh_CN
```

`--panel`, `--route equalizer`, `--profile WF-1000XM5`, `--locale en`, `--live`
are optional. The bundled catalog is located automatically. The app follows
macOS appearance; simulation and language choices do not write user preferences.
Use `--catalog /path/catalog.json` only when developing outside the app bundle.

This is a prototype with the shared nine-page scope/limitations documented in
`gui/native/README.md`. Signing, notarization, bundled Qt runtime, Bluetooth
permission handling on real hardware, keyboard/VoiceOver acceptance and display
placement remain target-Mac acceptance work. Source and build entry are provided;
this Windows host has no macOS SDK, so AppKit compilation/runtime is not claimed.
The branch's GitHub Actions workflow compiles the Swift frontend on macOS when run.
