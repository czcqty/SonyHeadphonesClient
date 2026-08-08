# Contributing

## Before opening a pull request

- Keep changes focused and preserve the existing MIT-compatible licensing.
- Credit copied or adapted work in the source and documentation.
- Do not introduce code from incompatible sources such as AGPL projects.
- Avoid device-specific workarounds unless they are backed by protocol evidence
  or behavior from Sony's official implementation.
- Update device matrices and user-facing documentation when behavior changes.

Configure without code generation unless your change intentionally updates
generated protocol files:

```bash
cmake -S . -B build -DMDR_ENABLE_CODEGEN=OFF
cmake --build build --config RelWithDebInfo --parallel
ctest --test-dir build -C RelWithDebInfo --output-on-failure
```

With `BUILD_TESTING=ON` (the default), the registered suite covers ViewModels,
backend regressions, QML navigation, translations, icons, and package metadata.
Python-based validators are registered only when a Python interpreter is found.

If you do modify protocol payload declarations, follow
[`AGENTS.md`](../AGENTS.md), run the appropriate code-generation targets, and
include the generated outputs.

Do not mark a feature implemented for a device from a preview profile or bundled
artwork. Update `docs/device-support/` only from a real-device report that names
the application commit/version, firmware, operating system, and tested behavior.

## Licensing and credits

This repository is MIT licensed, but that does not make code from every source
compatible. Confirm the license before adapting external implementations and
retain all required notices. Device artwork credits are stored alongside the
assets.

## AI-assisted contributions

Disclose material use of coding agents in the pull-request description or with
an appropriate `Co-Authored-By` trailer. Contributors must understand, test, and
be able to explain every submitted change; generated output is reviewed to the
same standard as handwritten code.

Current maintainers:

- [czcqty](https://github.com/czcqty/)
