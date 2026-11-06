# Native presentation adapter

All native branches now build the same `SoundConnectBackend` target from `backend/`.
Configure the root with `-DMDR_BUILD_QT_FRONTEND=OFF -DMDR_ENABLE_CODEGEN=OFF`.
Follow the [shared backend build guide](../../backend/README.md) for configuration,
testing, and platform-specific executable paths, including the macOS app bundle.
See the [branch guide](../../docs/branches.md) for all native build entries.
Device validation, connection handling and protocol mapping remain on the main
branch. This branch owns only its native widgets and platform integration.

The common binding catalog and Python transport live in `clients/common/`.
The catalog now covers every device operation, including advanced timing,
NC-button mapping, adaptive sensitivity, power off and multipoint actions.
Run `python clients/common/test_catalog.py /absolute/path/to/SoundConnectBackend`.
Native widget smoke tests remain available with `--smoke-test` where supported.

Pass `--backend /absolute/path/to/SoundConnectBackend` to the native executable.
The default is isolated preview; `--live` enables Bluetooth. See
[the protocol](../../backend/README.md) and [migration gate](../../docs/backend-migration.md).
The Linux adapters on `kde`, `gnome` and `xfce` have completed functional
migration to the shared contract. The installed clients passed bilingual native
control, upgrade, reinstall and uninstall checks. Physical headset and
workstation checks are tracked in the [Linux acceptance record](../../docs/linux-acceptance-2026-11-07.md).
