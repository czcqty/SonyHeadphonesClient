# Documentation

## Device support

The current Qt Quick client targets MDR v2 and falls back to the implemented
MDR v1 operations when the modern service is unavailable. The following
matrices are based on contributor testing and may contain unknown entries where
a feature has not yet been verified.

- [WF-1000XM5](device-support/WF-1000XM5.md)
- [WF-1000XM6](device-support/WF-1000XM6.md)
- [WF-C510](device-support/WF-C510.md)
- [WF-L910](device-support/WF-L910.md)
- [WF-LS900N / LinkBuds S](device-support/WF-LS900N.md)
- [WH-1000XM5](device-support/WH-1000XM5.md)
- [WH-1000XM6](device-support/WH-1000XM6.md)
- [WH-CH720N](device-support/WH-CH720N.md)

Bundled device artwork and preview profiles are not support guarantees. MDR v1
support is currently limited to the operations documented in the parity
roadmap and still requires model-specific hardware verification.

To report a model or update a matrix, use the device-support issue template and
include the application version, operating system, firmware version, and the
features you tested.

## Developer notes

- [Qt Quick desktop client](../gui/README.md)
- [libmdr](../libmdr/README.md)
- [Code generation](../tooling/README.md)
- [Maintenance fixes in the Qt refactor](maintenance.md)
