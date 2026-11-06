# Windows acceptance: WH-1000XM5

**Status on 2026-11-07: functional migration complete on `winui`.** The local
Windows 11 configuration passed the functional and desktop checks. The
[outstanding device and environment checks](#outstanding-device-and-environment-checks)
remain open for hardware and platform verification.

Hardware and installer checks ran on October 29–31, 2026 on Windows 11 x64
(build 26200) with WH-1000XM5 firmware 2.3.1. That WinUI candidate underwent
desktop checks on October 31 and November 5–6. For the current Windows installer,
see the [November 7 tray and icon update](#november-7-tray-and-icon-update).

## October candidate and installer

- Shared fixes: main `26e951f` (equalizer) and `f9beb2e` (safe listening).
- Native presentation: `6b70bee`; tested merged WinUI source
  `d454bf93df20a337a7957d1686dd6e1ba168e2b7`.
- Installer: `SoundConnect-WinUI-2.0.4-windows-x64-setup.exe`, 98,075,530 bytes.
- SHA-256: `e7357572838cdd6670b11d75f983f35b9a7e9fcdaa8b1a049b69cb2a7810fd24`.
- Local delivery directory: `cmake-build-native/packages/windows-acceptance-2026-10-30/`.
  Its `manifest.json` records source and payload hashes. Documentation-only
  follow-ups do not change this tested code. The October acceptance evidence
  came from local testing.

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
- Offline publish emitted NU1900 because vulnerability metadata could not be
  fetched; compilation and runtime verification passed.

Local detailed reports are in
`cmake-build-native/reports/windows-acceptance-2026-10-29/`, including
`main-tests.xml`, `native-backend-tests.xml`, `native-final-en.json`,
`native-final-zh_CN.json`, `native-equalizer-readback.json` and
`installer-verification.txt`. Installed reports are in
`cmake-build-native/installed-verification-2026-10-31/reports/`.
These ignored build artifacts are local evidence, not files distributed by Git.

## Windows 11 desktop follow-up

| Check | Observed result |
| --- | --- |
| Explorer restart | With the main window hidden, Explorer restarted. The same WinUI and backend processes survived. The tester found the recovered tray icon and opened quick controls with one click. Explorer returned within the two-second tray poll interval. |
| Prolonged tray loss | On a private Windows desktop with a test `Shell_TrayWnd`, a normal window close message hid the preview app. After the test removed the taskbar window, the candidate released its tray icon. An external observer saw the main window reappear after 1,968 ms. The quick-panel action kept the main window accessible. After another 6.5 seconds without a tray, a second close message exited the app with code 0 and no child left running. |
| Panel bounds | On the single display at 150% scaling, the panel was 660×840 physical pixels inside the 2560×1528 work area. |
| Page appearance and language | Ten pages and the separate quick panel were captured and inspected in English and Chinese with light, dark and system themes (66 initial viewports). System followed the PC's light app theme. All captures used the XM5 preview profile. Their state reports matched the requested locale and theme, with zero outgoing commands. |
| Quick controls | The noise menu stayed open for keyboard navigation. Escape canceled the highlighted dropdown change and kept the panel open. |
| Client termination | After the test client was terminated, its private preview backend exited on its own. |
| Foreground keyboard shortcuts | On November 5, pressing Ctrl+Alt+P and Ctrl+Alt+N in the WinUI window sent one `playback.togglePlayback` and one `playback.nextTrack` request, respectively. Ctrl+Alt+M was already taken. WinUI displayed Windows error 1409. |
| Background keyboard shortcuts | On November 6, with the main window hidden and Windows About (`winver`) in the foreground, Ctrl+Alt+P sent exactly one `playback.togglePlayback` request. Ctrl+Alt+N sent exactly one `playback.nextTrack` request. |

For both shortcut tests, WinUI used its normal live-mode key registration. A local
relay forwarded requests to the packaged shared backend running in preview and
recorded the target and method. Actual XM5 playback requires a separate hardware
check. Local evidence is under `cmake-build-native/reports/` in
`windows-desktop-2026-10-31/explorer-{before,after}.json`,
`windows-desktop-2026-11-05/keyboard-commands.jsonl` and the background key log
named in `windows-desktop-2026-11-06/session.json`.

The November 6 directory also contains `panel-workarea.jsonl` and
`catalog-WH-1000XM5/capture-report.json`, with the contact sheets and state
reports alongside it. Native control-tree captures cover the visible viewport.
The separate interactive checks cover scrolling and popups
only in the cases described above.

The same November 6 report directory contains `tray-wmclose.jsonl` with its
launcher log. The isolated tray test exercised
the candidate's two-second tray monitor with a synthetic taskbar window.
The Explorer restart row records the separate check with the real Windows shell.

During the November 6 live check the Windows Bluetooth radio was initially off.
With the radio temporarily on, the packaged backend completed two scans.
Each returned one device and zero WH-1000XM5 matches. The tester could not
connect the XM5 for this session. The backend sent no playback or settings
commands to the headset. An independent Windows radio query confirmed the
original Off state after cleanup. See `xm5-connection-radio-on.json` and
`xm5-radio-restored.json` in the same directory.

## November candidate and installer

- Source: WinUI `ef2ed5caba2453c034f14953f219cae39b1a6094`.
- Installer: `cmake-build-native/packages/windows-acceptance-ef2ed5caba/SoundConnect-WinUI-2.0.4-windows-x64-setup.exe` (98,068,921 bytes).
- SHA-256: `6b110573860fd094fb33aa0bbe1f1782c86229ee9c969724d176dec354bab226`.

In the earlier candidate, clicking Refresh on the live WinUI Connection page
with Windows Bluetooth off produced “蓝牙扫描失败: 句柄无效。”
Windows had reported that no radio was available, but the scanner continued
with an invalid handle. Main `8444b51` now distinguishes an unavailable adapter
from a successful scan with no connected devices. The scanner also releases
each radio handle and continues past a radio with no connected devices.
WinUI `776c2a3` displays the Windows connection instructions only when the device
list is empty and there is no active connection, scan or error.

With Bluetooth off, the corrected shared backend returned
`bluetooth-adapter-unavailable` on two successive scans. With Bluetooth on,
the same process completed 32 scans. The first and last each returned one
device, and the process handle count was 187 in both samples. An independent
radio query confirmed the original Off state after the test. See
`xm5-after-radio-fix.json` and `adapter-enumeration-fixed.json` in the
November 6 report directory.

On WinUI `ef2ed5ca`, both English and Chinese live sessions initially showed
how to connect headphones in Windows. After clicking Refresh with the radio
off, the status bar explained how to enable Bluetooth or connect an adapter
and refresh again. The ordinary empty-list hint disappeared. This was checked
through the native controls in both languages. The hardware results in the
October section apply to the earlier candidate. The November build still needs
playback and connection verification with an available XM5.

Main passed 17/17 CTest suites and the WinUI backend build passed 9/9.
Package verification installed the October installer, upgraded it using the
November installer, then reinstalled the November build. At each step all 801
payload files matched the corresponding manifest. The installed English and
Chinese parity runs passed with a restricted PATH and SDK lookup disabled.
Each covered 12 preview profiles, 45 commands and 1,414 requests. A normal
silent uninstall removed the application, uninstaller, registry entries and
shortcuts. Only the unrelated sentinel file remained. Local evidence
is in `cmake-build-native/reports/windows-desktop-2026-11-06/`, including
`main-radio-fix.xml` and the `candidate-ef2ed5caba/` subdirectory.

## November 7 tray and icon update

- Installer: `cmake-build-native/packages/windows-acceptance-00ab8243a0/SoundConnect-WinUI-2.0.4-windows-x64-setup.exe` (98,147,707 bytes).
- SHA-256: `7cae7511bdf75c5f523a4a23aa4f472249c73b7145de2d92fe9cc23a04b8571e`.

WinUI `00ab8243a0` anchors the quick panel to the notification area's corner
of the monitor work area. On the test PC with the taskbar at the bottom,
the panel occupied (1888, 676) to (2548, 1516) in a 2560×1528 work area at
150% scaling. The right and bottom gaps were both 12 physical pixels (8 DIPs).

With the panel open, the first Escape closed the noise selector. A second
Escape left the panel open. Open full settings activated the main window.
The same panel stayed visible while the navigation was collapsed there.
Clicking the panel's native × closed just the panel.
In a separate preview run, invoking the app's tray left-click handler twice
kept the same window handle, position and size. After a window close message,
the next invocation reopened that same panel at the same coordinates. See
`tray-repeat.jsonl` in the November 7 report directory.

All ten navigation items had native symbols in expanded and collapsed views,
including the gear for App settings. Both title bars showed the Sound Connect
headphones icon. The large and small window icons returned by Windows matched
the published application icon pixel for pixel. The geometry and icon report
is `cmake-build-native/reports/windows-desktop-2026-11-07/window-icons-and-panel.json`.

The backend passed all nine CTest suites. Upgrading the previous November
installer and reinstalling this build produced 802 files matching the release
manifest each time. The installed executable, its window, and the Windows
Installed apps entry used the project icon. English and Chinese native parity
each passed on 12 preview profiles and 45 commands. A normal silent uninstall
removed the application and its registrations and preserved the unrelated test
file. The package checks are under `candidate-00ab8243a0/` in the same report
directory.

## Reproduction

Use `backend/tests/verify_xm5_settings.py --backend <host.exe> --allow-writes
--report <report.json>` with the XM5 connected and no other live backend.
This explicitly changes reversible settings, saves originals before writing,
and stops on failed restoration. `--only multipoint` runs its isolated retest.
Safe listening is excluded from this persistence test because GET cannot supply
the current mode; its acceptance requires actual mode notifications.

On `winui`, run `gui/winui/Build.ps1`, `Package.ps1` and `Verify.ps1`
as documented in `gui/winui/README.md`. Use a fresh verification directory.

## Outstanding device and environment checks

| Environment needed | Verification still open |
| --- | --- |
| Connected WH-1000XM5 and a player with identifiable tracks | Check actual play/pause and previous/next track. Cancel a connection attempt and reconnect. |
| Two audio hosts and a suitable headset | Verify an actual audio-source switch. The XM5 multipoint setting itself passed enable/disable and restoration. Exercise device-list operations on a model that reports support for them. |
| Windows 10 and a clean Windows installation | Check Windows 10 behavior and install, run and uninstall the package on a clean OS image. |
| Multiple displays with different scaling | Verify panel placement and sizing across displays and DPI changes. |
| Windows session with Ctrl+Alt+M available | Open quick controls with the registered global shortcut. The test PC exercised the registration-conflict warning. |

Qt Quick stays available until the project's [removal gate](backend-migration.md#removal-gate) passes.
