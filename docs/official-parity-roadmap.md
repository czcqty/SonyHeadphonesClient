# Sound Connect parity roadmap

This document tracks practical parity with Sony Sound Connect. "Parity" means
that the desktop client exposes a setting only when the connected device reports
the corresponding capability and that the setting has been verified against a
real device. It does not mean copying Sony application code, artwork, cloud
services, or branding.

Official references:

- <https://www.sony.com/electronics/support/software/00269130>
- <https://helpguide.sony.net/mdr/hpc/v1/en/print.pdf>
- <https://helpguide.sony.net/mdr/2998/v1/en/contents/H002_Using_the_Sound_Connect_app.html>

## Current feature matrix

| Area | Status | Notes |
| --- | --- | --- |
| Connection and capability discovery | Implemented for MDR v2; partial for MDR v1 | The desktop client scans the platform Bluetooth backend, tries the modern RFCOMM service first, falls back to the legacy service during initialization failure, and resynchronizes every 60 seconds. |
| Battery, codec, firmware, model information | Partial in the GUI | Model and available left/right/case battery values are displayed. Additional decoded protocol metadata is not yet presented consistently in QML. |
| Noise cancelling and Ambient Sound Control | Core controls implemented | Off/NC/Ambient, ambient level, and Focus on Voice are wired. XM6 automatic ambient/noise-adaptation state exists in `libmdr` but has no current ViewModel or QML control. |
| Equalizer and Clear Bass | Partial | Device-reported band arrays and Clear Bass can be edited. The current GUI offers Flat and Custom only, not the full device preset catalog. |
| Playback, track metadata, and volume | Implemented | Title, artist, previous/play-pause/next, and volume are exposed; availability still depends on device and host metadata support. |
| DSEE | Implemented | Capability gated. |
| Bluetooth connection quality | Implemented | Sound-quality and stable-connection modes for the currently supported MDR v2 command variant. |
| Speak-to-Chat | Partial | On/off is exposed. Sensitivity and mode-out time are not exposed by the current ViewModel. |
| Listening mode | Partial | The GUI toggles BGM mode and clears Cinema when enabling it. Room size and Cinema selection are not exposed. |
| Automatic power off and wearing pause | Partial | Wearing pause is an on/off switch. Auto power-off currently maps to Disabled or 15 minutes rather than exposing every device-reported duration. |
| Remote headphone power off | Implemented | Shown separately from disconnect and only when advertised by the device. |
| Touch/button assignment and head gesture | Partial | Left/right assignment, the NC/ASM button variants, and head-gesture on/off are exposed. Only the preset values accepted by the current ViewModel can be selected. |
| Notification/voice guidance | Partial | On/off and volume are implemented; language-package transfer is not. |
| Pairing and multipoint device management | Prototype/partial | Pairing-mode and playback-target writes exist. The current Devices page shows deterministic placeholder rows; the decoded live paired-device list, connect/disconnect, and unpair flows are not wired to QML. |
| Safe Listening | Backend only | `libmdr` decodes current sound pressure and supports preview-mode writes, but the Qt ViewModels and QML do not expose them. History, dose calculations, and account sync are not implemented. |
| Simplified Chinese | Implemented for current QML routes | English source strings live in `I18n.qml`; the Simplified Chinese Qt Linguist catalog is applied at runtime. New UI-owned strings must update both. |
| Legacy MDR v1 devices | Partial | MIT-licensed SonyBridge/Plutoberth behavior provides service fallback plus read/write support for Noise Cancelling and Ambient Sound Control. Battery, legacy EQ, VPT, and sound-position support remain to be integrated and hardware-verified. |
| LE Audio and low-latency connection mode | Missing | Newer MDR v2 connection-mode variants still need protocol integration and hardware verification. |
| Touch sensor master switch and sidetone | Missing | Protocol support has not yet been integrated. |
| Battery Care | Missing | Protocol support and real-device captures are required. |
| Quick Access service assignment | Missing | Service-side identifiers and device protocol support are required. |
| Firmware update | Blocked | Do not implement until a legally redistributable, signed update source and a recovery-safe transport are available. |
| Find Your Equalizer | Missing | Can be implemented as a local guided UI after the basic device matrix is stable. |
| Adaptive Sound Control scenes | Backend subset/blocked | Some device-local adaptive fields are decoded, but the current GUI does not expose them. Phone motion/location learning and cloud-linked scenes remain out of scope. |
| 360 Reality Audio, ear analysis, service integrations | Out of desktop-protocol scope | These depend on Sony services, accounts, phone sensors, or licensed content. |

The status above describes the current repository wiring, not model support.
An item can be implemented in the generic MDR v2 path and still remain unverified
or unsupported on a specific device. See the individual device reports for
firmware- and revision-specific evidence.

## Reuse policy

Code may be copied or adapted only after its license and provenance are recorded.
The preferred sources are:

| Source | License | Intended use |
| --- | --- | --- |
| Historical upstream `libmdr` implementation | MIT | MDR v2 protocol and platform-transport lineage; legal provenance is retained in the applicable source notices. |
| `AmitRajput-Dev/SonyBridge` | MIT | Legacy MDR v1 behavior, dual-protocol discovery, and credited device artwork. |
| `Plutoberth/SonyHeadphonesClient` | MIT | Legacy MDR v1 protocol, VPT, sound position, and older-device reports. |
| `sevsev9/sonyctl` | MIT | Linux/BlueZ operational behavior where it improves the existing backend. |
| `NicholasSlattery/sony-head-tracker` | Check before integration | Optional standards-based Android Head Tracker support, isolated from core settings. |
| Gadgetbridge | GPL | Behavioral and packet-layout reference only; do not copy GPL implementation into this MIT repository. |

When importing code, preserve the upstream copyright notice, add a local notice
beside the imported files, and keep protocol-specific code out of the Qt pages.

## Delivery order

1. Stabilize the current Qt Quick application: regression tests,
   reconnect/error handling, bilingual string coverage, packaging, and
   device-report templates.
2. Replace current frontend placeholders with decoded backend state: live
   paired devices, Safe Listening, XM6 noise adaptation, and complete preset or
   duration choices, each behind capability tests.
3. Extend the imported MDR v1 backend beyond NC/ASM only when packet fixtures
   and hardware reports cover read, write, reconnect, and persistence behavior.
4. Add newer MDR v2 connection-mode variants, sidetone, Battery Care, and
   remaining local settings only when packet layouts and real-device reports
   exist.
5. Add optional desktop-native features such as tray controls, hotkeys, and
   standards-based head tracking as separate modules.
6. Treat firmware, accounts, cloud services, and phone-sensor workflows as
   separate projects with explicit safety and legal review.

## Verification requirements

- Every setting must have a capability-gating regression test.
- Protocol payload changes must follow `AGENTS.md` serialization and validation
  rules.
- A feature is not marked "implemented" for a model until a real-device report
  confirms read, write, reconnect, and persistence behavior.
- Unsupported controls must stay hidden or disabled and must never enqueue a
  command.
- Windows, Linux, and macOS builds must remain green before a release.
