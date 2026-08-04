# Sound Connect 13.2.1 clean-room reference

This document records the measurements and provenance used by the Qt Quick
frontend. The APK/XAPK and all decoded output are local, ignored inputs and are
not part of the repository.

## Input identity

| Field | Value |
|---|---|
| Product | Sound Connect |
| Package | `com.sony.songpal.mdr` |
| Version | `13.2.1` |
| Version code | `130215029` |
| Minimum SDK | 30 |
| Target SDK | 36 |
| Distribution container SHA-256 | `7B1D286FB73E26FF37209CBD6D2121BEEA40EFCAB190F0F9F10EB525C3963777` |
| Included splits | base, arm64-v8a, English, mdpi |
| Region declaration | Not present in the XAPK manifest |
| APK signer certificate | Pending `apksigner` capture; v1/JAR signature is not present |

The reference was inspected with Apktool 3.0.2 and JADX 1.5.5. Static
decompilation is used only to identify view hierarchy, strings, dimensions,
colors, state names, and navigation. No Sony code or extracted resources are
copied into the product.

## Measured device-home structure

The 13.2.1 headphone entry opens a portrait-only settings top screen. Its
observable static hierarchy is:

1. A collapsible 402 dp device header with a pinned 56 dp toolbar.
2. A scrollable shortcut area with 16 dp horizontal padding.
3. A 56 dp “All device settings” row with 12 dp top margin.
4. A “Shortcuts” heading with 24 dp top and 12 dp bottom spacing.
5. Capability-dependent shortcut cards.
6. A 120 dp edit-shortcuts card.

The initial plan’s Status/Sound/System/Services tab assumption is not the
13.2.1 main-screen structure. Local controls are therefore implemented as
shortcuts on the device home screen, with Sound and System sections inside
“All device settings.” Account, cloud, discovery, scene/sensor, third-party
service, and firmware-update destinations are intentionally absent.

## Design tokens

| Token | Light value |
|---|---|
| Primary | `#364fa8` |
| Primary container | `#c5d4ff` |
| Surface / window background | `#f8fafa` |
| Surface container | `#eef1f1` |
| Surface container high | `#f2f4f4` |
| Surface container highest | `#e1e3e3` |
| On surface | `#191c1d` |
| On surface variant | `#444748` |
| Outline | `#747878` |
| Outline variant | `#c4c7c7` |
| Shortcut card radius | 13 dp |
| Common horizontal content inset | 16 dp |

Observed type sizes used by the implementation are 12, 13, 16, 18, 20, and
22 sp. The desktop build uses system sans-serif fonts; it does not redistribute
the APK font resources.

## Asset ledger

| Asset category | Product source | Status |
|---|---|---|
| Device product images | Existing repository assets | Retained under existing repository licensing |
| UI icons | Qt controls / platform text actions | No APK icons copied |
| Fonts | Operating-system sans-serif | No APK fonts copied |
| Sony logo and trademarks | None | Excluded |
| APK drawables | Local measurement only | Ignored, never packaged |

WF-1000XM6, WF-C510, and WF-L910 currently use family fallback art because no
licensed exact model image exists in the repository. Those profiles are marked
`reference-blocked` in preview mode.

## Remaining reference capture

Static layout measurements are complete enough for implementation, but dynamic
Android captures are still required for transitions, pressed/focused states,
dialogs, errors, and device-specific variations. Until each state has an
official capture, it must remain `reference-blocked` and cannot pass the visual
parity gate.
