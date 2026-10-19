# WinUI comparison with Qt Quick (2026-10-19)

The baseline is the existing QML pages and the shared ViewModels, not the whole
libmdr API. Qt Quick remains enabled. This follow-up does not close real-device,
Windows 10, DPI, accessibility or Explorer lifecycle acceptance.

| Qt Quick behavior | WinUI implementation and check |
| --- | --- |
| Overview metadata, batteries/charging, playback, volume, power off | Existing catalog fields retained; absent right/case batteries and unsupported pressure readings are hidden. Playback and power-off controls are invoked against the preview backend. Power-off pending disables repeat requests. |
| Noise mode, ambient level/voice focus, adaptive controls | All fields roundtrip. Ambient fields appear only in ambient mode; sensitivity appears when adaptive control is enabled. Quick controls also expose ambient level and focus. |
| EQ presets, five bands, Clear Bass | All reported preset values exercised. Sliders show an editable native NumberBox and both input paths are verified against resulting state. |
| DSEE, quality, background/cinema, room size, adaptive volume, safe listening | All supported toggles/options roundtrip. Shared backend retains background/cinema mutual exclusion. Room size follows background mode visibility. |
| Speak-to-chat, sensitivity/timeout, wear pause, power timing, gestures, voice capture | Every supported field is exercised; speech detail options follow the enable toggle. All power timing options, including wear detection when available, are exercised. |
| Touch left/right, NC button, voice on/off and volume | Backend-reported choices and all available static choices roundtrip without substituting array indexes for raw values. |
| Discovery, connecting/initializing/cancel, error and reconnect | Named rows include MAC and Connect actions. Scan and overlapping connects are disabled while busy, cancellation stays available, and disconnected pages offer a direct connection entry. |
| Per-device connected/playback status, connect/disconnect, source switch, unpair | Replaced anonymous action pickers with device rows and contextual native buttons. Confirm and cancel are tested separately. Reported connected/disconnected/source states are also supplied as deterministic presentation fixtures. |
| Language/theme, no initial writes | Existing native preference controls retained and awaited through the response and resulting snapshot. Page creation and refresh send no commands. |
| Tray quick controls versus full settings | Quick layout uses a small subset of the same bindings and a fixed Open full settings footer. It shares the reliable edit path, without displaying the entire settings layout. |

## Adjustment and disabled-state fixes

- The old picker refreshed SelectedIndex for every state frame. It could undo a
  keyboard selection while open or show the old value before its result arrived.
  Open pickers now keep their items/highlight, and edits stay pending until the
  matching result **and following full snapshot**. Closing applies a changed
  choice once; Escape cancels and closes the picker before the quick panel.
- Transport readiness and a connected headset are separate. Device commands,
  tray actions and shortcuts require the latter; preferences remain available
  with a ready backend. Scan cannot interrupt connecting/initializing.
- Grey controls now have visible, localized explanations: waiting for backend,
  connect first, feature not reported, choices not yet supplied, or power off
  pending. Capabilities are never forced on to conceal an unavailable operation.
- Each full-window slider has direct numeric input, a visible current value and
  commit deduplication. Pending edits are not overwritten by stale snapshots.
- Request rejection remains visible. Timeout clears stale state and offers
  restart. Pending requests are canceled on restart/exit and never replayed.

## Verification boundary

`--parity-test` runs the real WinUI controls against the C++ preview backend for
all twelve profiles and all 43 declared operation entry points. It checks
returned values, all available choice values, single writes, numeric input,
confirmation cancellation/acceptance, open picker refresh, quick controls,
initialization cancellation, missing backend and recovery. The installer
verification runs this in English and Chinese with developer runtime paths
removed. The transport harness separately verifies delayed snapshots, rejected
requests, operation timeout and restart cancellation.

This verifies presentation behavior and backend acceptance. Multipoint fixture
states and preview responses do not establish headphone acknowledgements. The
user's specific physical setup remains an acceptance gate; no claim of complete
migration is made from these checks.
