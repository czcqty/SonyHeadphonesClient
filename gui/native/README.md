# Native presentation adapter

All native branches now build the same `SoundConnectBackend` target from `backend/`.
Configure the root with `-DMDR_BUILD_QT_FRONTEND=OFF -DMDR_ENABLE_CODEGEN=OFF`.
The process is `build/backend/SoundConnectBackend` (add `.exe` on Windows).
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
Full production parity, persistent preferences/localization, desktop behavior
and real-device acceptance are still tracked separately from command coverage.
