# Documentation

## Device support

The current Qt Quick client targets MDR v2 and falls back to the implemented
MDR v1 operations when the modern service is unavailable. MDR v1 currently
covers Noise Cancelling and Ambient Sound Control only.

The following matrices preserve contributor reports from the upstream project
and the commit/firmware context recorded in each file. They are evidence about
the reported revision, not a guarantee that the current Qt Quick route exposes
every listed feature. Unknown entries mean that no usable report is recorded;
preview profiles and bundled artwork are never support evidence.

- [WF-1000XM5](device-support/WF-1000XM5.md)
- [WF-1000XM6](device-support/WF-1000XM6.md)
- [WF-C510](device-support/WF-C510.md)
- [WF-L910](device-support/WF-L910.md)
- [WF-LS900N / LinkBuds S](device-support/WF-LS900N.md)
- [WH-1000XM5](device-support/WH-1000XM5.md)
- [WH-1000XM6](device-support/WH-1000XM6.md)
- [WH-CH720N](device-support/WH-CH720N.md)

Status legends vary in older imported reports. For new reports use **✅** for
working, **❌** for tested and broken/unsupported, **❓** for untested, and **~**
for officially available but not implemented. Do not convert an old status
without a new real-device result.

To report a model or update a matrix, use the device-support issue template and
include the application version, operating system, firmware version, and the
features you tested.

## Developer notes

- [Qt Quick desktop client](../gui/README.md)
- [libmdr](../libmdr/README.md)
- [Code generation](../tooling/README.md)
- [Maintenance fixes in the Qt refactor](maintenance.md)
- [Sound Connect parity roadmap](official-parity-roadmap.md)
- [Sound Connect 13.2.1 clean-room reference](reference/SoundConnect-13.2.1.md)
- [Visual design QA](../design-qa.md)

## Current verification snapshot

On 2026-08-04, the existing Windows Debug build passed all six registered CTest
entries: ViewModel, backend regression, QML QuickTest, translation catalog,
application icon, and package metadata. This snapshot is a local software
baseline only; transport, reconnect, persistence, and per-model behavior still
require real hardware.
