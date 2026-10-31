# Windows acceptance: WH-1000XM5

Checked 2026-10-29 through 2026-10-31 on Windows 11 x64, build 26200,
with WH-1000XM5 firmware 2.3.1. This accepts the reproduced fixes and the
local candidate, not every Windows release gate or removal of Qt Quick.

## Source and delivery

- Shared fixes: main `26e951f` (equalizer) and `f9beb2e` (safe listening).
- Native presentation: `6b70bee`; tested merged WinUI source
  `d454bf93df20a337a7957d1686dd6e1ba168e2b7`.
- Installer: `SoundConnect-WinUI-2.0.4-windows-x64-setup.exe`, 98,075,530 bytes.
- SHA-256: `e7357572838cdd6670b11d75f983f35b9a7e9fcdaa8b1a049b69cb2a7810fd24`.
- Local delivery directory: `cmake-build-native/packages/windows-acceptance-2026-10-30/`.
  Its `manifest.json` records source and payload hashes. Documentation-only
  follow-ups do not change this tested code. This candidate has not been pushed
  or validated by a new hosted CI run.

## Hardware results

Persistent-setting checks stop the backend, start a fresh process, reconnect,
read the headset value and verify restoration. A successful command reply or
an optimistic local snapshot does not count as device readback.

| Cases | Result |
| --- | --- |
| ANC/off/ambient, ambient level, focus voice | Passed; restored |
| DSEE, touch panel, speak-to-chat | Passed; restored |
| Speak sensitivity and timeout choices | Passed; restored |
| Wear pause, auto-off choices, NC button assignments | Passed; restored |
| Voice guidance, connection quality, volume decrease | Passed; restored |
| All five EQ bands, Clear Bass, bright/flat presets | Passed; restored |
| Multipoint enable/disable | Final retest passed; restored to false |
| Safe listening | Separate same-session device notifications confirmed Off/On/Off |

These are 18 persistent-setting cases plus safe listening. They do not cover
unsupported XM5 features or other headset models.

Actual WinUI editing reproduced the original EQ failure: changing 400 Hz from
0 to 1 reverted to 0. Selecting Custom had sent a preset-only command and
allowed its notification to overwrite the pending band edit. Custom edits now
send the selected preset and bands together. Built-in presets are sent without
bands, since a band payload could cause the XM5 to select Custom instead of Flat.
Protocol regressions cover both paths.

After the fix, the native NumberBox accepted 400 Hz=1 and retained it. An
independent backend session on October 31 read Custom/[1,0,0,0,0]/bass=0, then
restored and independently read Flat/[0,0,0,0,0]/bass=0. Multipoint was false.

Safe-listening GET reports availability, not the current enabled mode. The old
decoder also mistook previewMode for safeListeningMode. The backend now keeps
these meanings separate. WinUI initially shows “State not reported; choose Off
or On”, and an explicit Off is sent even if the unknown local default is false.
Live Off/On/Off notifications increased the received-notification count and
matched each requested mode. Real WinUI selection changed the unknown display
to Off after the headset notification. A fresh reconnect alone cannot prove
the persisted mode. The original pre-test safe-listening state was unknown;
the final explicitly confirmed state is Off. Do not describe that as restoration
of a known original value.

Earlier reports remain as failure evidence: `all-settings.json` was interrupted
by disconnection; `equalizer-fixed.json` exposed the built-in-preset issue;
`all-settings-reconnected.json` passed 17 persistent cases but failed multipoint
restoration and used the incorrect safe-listening assumption. The final
`multipoint-final.json` passed both fresh readback and restoration with an
eight-second confirmation interval. `safe-listening-explicit-off.json` supersedes
the misleading availability-based check in `safe-listening-fixed.json`.

## Desktop, tests and packaging

- Main with transitional Qt Quick: 17/17 CTest suites passed.
- Backend-only WinUI build: 9/9 CTest suites passed.
- Native English and Chinese parity: both passed, 12 profiles, 45 contract
  commands and 1,414 requests per report. This includes unknown-state selection,
  narrow-layout control bounds, recovery, confirmation accept/cancel, quick
  controls, choices, numeric edits and unsupported-feature explanations.
- Actual Windows resizing enforces the minimum width/height; navigation folds
  into the framework menu. At the minimum size, EQ numeric inputs, scrolling
  through Clear Bass, scrolling the navigation and reaching Application/Quit
  were inspected. Automated bounds checks cover every catalog page at 480x400.
- The NSIS installer passed installation, reinstallation/upgrade, default backend
  lookup and both native parity runs with PATH limited to System32 and no SDK
  lookup. Uninstall removed the app and its registration while preserving the
  unrelated-file sentinel. No test client/backend process remained after cleanup.
- The live machine reports Ctrl+Alt+M registration conflict (1409). The warning
  is visible; this does not establish successful global registration for that key.
- Offline publish emitted NU1900 because vulnerability metadata could not be
  fetched; compilation and runtime verification passed.

Local detailed reports are in
`cmake-build-native/reports/windows-acceptance-2026-10-29/`, including
`main-tests.xml`, `native-backend-tests.xml`, `native-final-en.json`,
`native-final-zh_CN.json`, `native-equalizer-readback.json` and
`installer-verification.txt`. Installed reports are in
`cmake-build-native/installed-verification-2026-10-31/reports/`.
These ignored build artifacts are local evidence, not files distributed by Git.

## Reproduction and remaining gates

Use `backend/tests/verify_xm5_settings.py --backend <host.exe> --allow-writes
--report <report.json>` with the XM5 connected and no other live backend.
This explicitly changes reversible settings, saves originals before writing,
and stops on failed restoration. `--only multipoint` runs its isolated retest.
Safe listening is excluded from this persistence test because GET cannot supply
the current mode; its acceptance requires actual mode notifications.

On the WinUI branch, run `gui/winui/Build.ps1`, `Package.ps1` and `Verify.ps1`
as documented in `gui/winui/README.md`. Use a fresh verification directory.

Still unverified: Windows 10 and a clean Windows OS image; multiple monitors and
DPI changes; Narrator/high contrast; Explorer/tray restart; actual two-host audio
switching; remaining playback/device operations and other physical models.
The development-machine PATH isolation check is not a clean-OS check. Linux
and macOS acceptance is unchanged. Keep the transitional Qt Quick frontend
until the full matrix in [backend-migration.md](backend-migration.md) passes.
