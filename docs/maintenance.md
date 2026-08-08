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
`gui/tests/BackendTests.cpp`. ViewModel capability gating and desired-state
mapping live in `gui/tests/ViewModelTests.cpp`; QML contracts live under
`gui/tests/qml/`.

With Python available, CTest also registers translation-catalog, application-
icon, and package-metadata validators. The existing Windows Debug build passed
all six registered tests on 2026-08-04.

The protocol and Windows changes are compile-checked by the normal Windows
build. Real-device verification remains required for transport, reconnect,
capability, and persistence behavior.
