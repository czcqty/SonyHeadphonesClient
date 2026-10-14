# Maintenance notes

The Qt Quick migration retained the existing `libmdr` implementation and fixed
several issues found while integrating it:

- Protocol initialization now reads Table 2 support from
  `supportTable2Value`.
- Head-gesture commits are guarded by the head-gesture capability instead of
  the DSEE capability.
- Voice-guidance enable commits now clear the correct dirty property.
- `MDRTask` move assignment destroys an already-owned coroutine frame before
  taking ownership of another frame.
- The Windows RFCOMM backend validates `socket()` before calling
  `ioctlsocket()`, reports Winsock errors consistently, and returns the
  documented network error code.
- Windows error-message buffer sizes are passed as character counts and trailing
  line endings are trimmed safely.
- Classic Bluetooth device names and addresses are copied with explicit bounds
  and null termination.

The former Qt Widgets frontend, its custom widgets, Linguist catalog, and
widget-specific tests have been removed. `SonyHeadphonesClient` now names the
Qt Quick executable directly.

The coroutine ownership and MDR v1 packet regressions live in
`backend/tests/BackendTests.cpp`. ViewModel capability gating and desired-state
mapping live in `gui/tests/ViewModelTests.cpp`; QML contracts live under
`tests/qml/`.

CTest also registers translation-catalog, application-icon, and package-metadata
validators. The six-test Windows Debug result from 2026-08-04 is historical;
the expanded backend/frontend checks are recorded in
[backend-migration.md](backend-migration.md).

The protocol and Windows changes are compile-checked by the normal Windows
build. Real-device verification remains required for transport, reconnect,
capability, and persistence behavior.

The shared-backend migration now uses `backend/` for the device service and
private process host. Native adapters consume that host and the common binding
catalog; the transitional Qt Quick application links the same business library.
Backend-only builds use `MDR_BUILD_QT_FRONTEND=OFF`. Current checks and the
conditions for removing Qt Quick are in [backend-migration.md](backend-migration.md).

## Repository organization (2026-10-15)

- Shared backend, contract, and common documentation are maintained on `main`.
  Five native branches contain toolkit-specific presentation adapters; see the
  [branch guide](branches.md) for build entries and synchronization rules.
- The former `rebuild` tip, `bfba8aa`, is an ancestor of `main`. Its compact Qt
  tray frontend is retained in history and in the transitional build; retiring
  that branch does not remove the frontend.
- Build outputs and local worktrees under `cmake-build*/` are ignored. They can
  contain active branch checkouts: inspect `git worktree list` before cleaning
  build directories, and never recursively delete an active worktree as an
  ordinary build artifact.
