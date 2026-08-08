# libmdr

`libmdr` is the C++20 MDR protocol and transport library used by the Qt Quick
application. It exposes both the C++ `mdr::MDRHeadphones` API and the `mdr-c`
wrapper, and selects the Windows, Linux, macOS, or Emscripten platform backend
at configure time.

## Current protocol scope

- MDR v2 initialization, synchronization, capability tables, events, and dirty-
  property commits cover the local controls used by the desktop client.
- MDR v1 can initialize, synchronize, and commit Noise Cancelling/Ambient Sound
  Control state. Other legacy features such as battery, equalizer, VPT, and
  sound position are not integrated.
- Generated serialization, validation, enum, and trait sources are committed.
  Normal application builds should configure with `MDR_ENABLE_CODEGEN=OFF`.

The Qt-facing session is implemented in `gui/quick/MdrSession.*`; UI code must
not send protocol packets directly. Capability mapping is kept in
`gui/DeviceCapabilities.*`.

## Development

Protocol payload declarations must follow the packing, serialization, and
validation rules in [`../AGENTS.md`](../AGENTS.md). If a declaration changes,
configure with LLVM/libclang available, build the affected code-generation
target, and commit the generated output. See
[`../tooling/README.md`](../tooling/README.md) for the codegen workflow.

The protocol library is built through the repository's top-level CMake project;
it is not currently documented as a standalone installable package. MDR v1
packet fixtures and coroutine ownership regressions live in
`gui/tests/BackendTests.cpp`.

## Credits

Naming inspired by <https://github.com/AndreasOlofsson/libmdr>.

The following third-party libraries are used in the implementation.

- <https://github.com/fmtlib/fmt>
- SonyBridge / Plutoberth legacy MDR v1 behavior; see
  [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)
