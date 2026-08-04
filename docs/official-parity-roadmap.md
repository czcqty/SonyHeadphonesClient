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
| Connection and capability discovery | Implemented for MDR v2; partial for MDR v1 | The desktop client tries the modern RFCOMM service first and automatically falls back to the legacy service. |
| Battery, codec, firmware, model information | Implemented | Includes separate left, right, and case battery values when advertised. |
| Noise cancelling and Ambient Sound Control | Implemented | Includes Focus on Voice and supported XM6 noise-adaptation controls. |
| Equalizer and Clear Bass | Implemented | Supports device-reported five-band and ten-band layouts. |
| Playback, track metadata, and volume | Implemented | Uses the device protocol and platform metadata support. |
| DSEE | Implemented | Capability gated. |
| Bluetooth connection quality | Implemented | Sound-quality and stable-connection modes for the currently supported MDR v2 command variant. |
| Speak-to-Chat | Implemented | Includes sensitivity and mode-out time where available. |
| Listening mode | Implemented | BGM room size and Cinema upmix are mutually exclusive. |
| Automatic power off and wearing pause | Implemented | Capability gated. |
| Remote headphone power off | Implemented | Shown separately from disconnect and only when advertised by the device. |
| Touch/button assignment and head gesture | Implemented | Covers the assignment forms currently decoded by `libmdr`. |
| Notification/voice guidance | Partial | On/off and volume are implemented; language-package transfer is not. |
| Pairing and multipoint device management | Implemented | Includes connect, disconnect, unpair, pairing mode, and playback-device switch. |
| Safe Listening | Partial | Current sound pressure is displayed; history, dose calculations, and account sync are not implemented. |
| Simplified Chinese | Implemented for current QML routes | English and Simplified Chinese strings are provided by `AppController`; new routes must add both variants. |
| Legacy MDR v1 devices | Partial | MIT-licensed SonyBridge/Plutoberth behavior provides service fallback plus read/write support for Noise Cancelling and Ambient Sound Control. Battery, legacy EQ, VPT, and sound-position support remain to be integrated and hardware-verified. |
| LE Audio and low-latency connection mode | Missing | Newer MDR v2 connection-mode variants still need protocol integration and hardware verification. |
| Touch sensor master switch and sidetone | Missing | Protocol support has not yet been integrated. |
| Battery Care | Missing | Protocol support and real-device captures are required. |
| Quick Access service assignment | Missing | Service-side identifiers and device protocol support are required. |
| Firmware update | Blocked | Do not implement until a legally redistributable, signed update source and a recovery-safe transport are available. |
| Find Your Equalizer | Missing | Can be implemented as a local guided UI after the basic device matrix is stable. |
| Adaptive Sound Control scenes | Partial/blocked | Device-local adaptive controls exist; phone motion/location learning and cloud-linked scenes do not. |
| 360 Reality Audio, ear analysis, service integrations | Out of desktop-protocol scope | These depend on Sony services, accounts, phone sensors, or licensed content. |

## Reuse policy

Code may be copied or adapted only after its license and provenance are recorded.
The preferred sources are:

| Source | License | Intended use |
| --- | --- | --- |
| `mos9527/SonyHeadphonesClient` / local `libmdr` | MIT | Primary MDR v2 protocol and platform transports. |
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
2. Import a tested MDR v1 backend from the MIT-licensed SonyBridge/Plutoberth
   lineage behind the same capability-driven UI.
3. Add newer MDR v2 connection-mode variants, Safe Listening controls, sidetone,
   Battery Care, and remaining local device settings only when packet layouts
   and real-device reports exist.
4. Add optional desktop-native features such as tray controls, hotkeys, and
   standards-based head tracking as separate modules.
5. Treat firmware, accounts, cloud services, and phone-sensor workflows as
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
