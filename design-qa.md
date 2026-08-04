# Sound Connect 13.2.1 design QA

## Scope

- Target: fixed portrait Qt Quick desktop frontend
- Reference: local Sound Connect 13.2.1 split package
- Local-only areas: device home shortcuts, playback, ambient sound control,
  equalizer, device settings, and multipoint device management
- Explicit exclusions: account, cloud, discovery, location/activity learning,
  third-party services, and firmware update

## Evidence status

| Route/state | Static reference | Dynamic reference screenshot | Implementation capture | Result |
|---|---:|---:|---:|---|
| Device home / ready | Yes | No | Yes, 412 × 780 | `reference-blocked` |
| Device settings / ready | Partial | No | Yes, 412 × 780 | `reference-blocked` |
| Ambient sound / ready | Partial | No | Yes, 412 × 780 | `reference-blocked` |
| Equalizer / ready | Partial | No | Yes, 412 × 780 | `reference-blocked` |
| Device management / ready | Partial | No | Reproducible by CLI | `reference-blocked` |
| Loading / empty / error / disabled | Partial | No | Reproducible by CLI | `reference-blocked` |
| Simplified Chinese equivalents | Strings only | No | Yes, selected routes | `reference-blocked` |

## Automated comparison contract

Captures use the QML executable’s stable CLI:

```text
SonyHeadphonesClient
  --preview-profile <profile>
  --preview-route <route>
  --preview-state <state>
  --locale <en|zh_CN>
  --capture <output.png>
```

The intended Windows baseline is 412 × 780 logical pixels at 100% content
scale. Future comparison masks may cover only Android system-owned clock,
status-bar, and navigation-bar pixels; product content must not be masked.
`SonyHeadphonesClient_visual_diff` performs a channel-threshold pixel
comparison, writes a red overlay image, accepts repeatable `--ignore x,y,w,h`
masks, and fails when the mismatch ratio exceeds the configured gate.

## Verification completed

- Qt 6.11 Qml, Quick, QuickControls2, and QuickTest are installed through the
  existing vcpkg toolchain.
- Qt Quick is the only GUI implementation; the legacy Widgets frontend and its
  translation catalog have been removed.
- The Qt Quick application, ViewModel test, backend regression test, QuickTest
  target, and visual diff utility build with MSVC.
- CTest result after the Widgets removal: 3/3 passing (`gui-viewmodels`,
  `gui-backend-regressions`, and `gui-qml-quicktest`).
- The visual diff utility self-check completed with zero mismatched pixels.
- Local implementation captures were visually inspected for the home,
  settings, ambient sound, equalizer, error, and empty states.

## Blocking issues

1. The Android emulator/toolchain and same-state official screenshots have not
   yet been captured.
2. Exact licensed art is missing for three documented device models.

final result: blocked
