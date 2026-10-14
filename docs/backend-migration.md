# Shared backend migration

The target is a backend-only main branch, retaining Qt Core for now, with
WinUI, AppKit, KDE, GNOME and Xfce presentations on their existing branches.
Native controls, navigation, window/tray lifetime, accessibility, shortcuts
and appearance belong to presentation adapters. Bluetooth, capabilities,
validation, connection recovery and device operations belong to one backend.

## Removal gate

The existing Qt Quick frontend must remain available until every row below
has been implemented and validated in its native replacement. This is a
functional migration, not just removal of the QML build dependency.

| Area | Required coverage |
| --- | --- |
| Connection | discovery, connection progress/errors, cancellation, disconnect, protocol fallback, recovery |
| Playback | metadata, codec/firmware, all battery and charging states, volume, previous/play/next, power off, device alerts |
| Noise | ANC/off/ambient, ambient level, focus voice, adaptive enable and sensitivity |
| Equalizer | device presets, bands, Clear Bass, unsupported controls |
| Sound/behavior | DSEE, quality, speak-to-chat enable/sensitivity/timeout, wear pause, auto power off choices and wear support, head gestures, listening/cinema/room, adaptive volume, safe listening, capture voice |
| Touch/voice | left/right reported assignments, NC button assignment, voice enable/volume |
| Device management | pairing, reported device list, source switch, connect/disconnect/unpair |
| Application | English/Chinese including dynamic choices/errors, existing preference migration and persistence, system/light/dark, all navigation |
| Desktop | distinct quick panel/full settings, native tray menu, focus/popups/Escape, shortcuts, no-tray fallback, multi-monitor/DPI, startup/shutdown |
| Distribution | per-platform build, packaging, child lookup/lifetime, missing backend/error handling |

Backend command coverage and native widget coverage are separate acceptance
criteria. The backend contract test must cover all business methods for all
device profiles; it does not establish native accessibility or hardware
behavior. Linux windows under Xvfb and a Windows simulation host do not
establish BlueZ or real desktop integration. macOS needs its own SDK/runtime.

## Current status

- Device logic is extracted to `backend/`; protocol command mappings and
  session polling/recovery remain the existing implementation.
- The Qt Quick application is a transitional consumer of that same library.
- `MDR_BUILD_QT_FRONTEND=OFF` builds the standalone Qt Core backend and tests.
- Native presentation parity and real-device desktop checks are still release
  gates. Do not delete the transitional frontend or claim parity from IPC tests.

Validation results and remaining platform limitations will be recorded here
as checks complete.
