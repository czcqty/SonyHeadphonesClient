# Branch guide

## Ownership and entry points

The target architecture is a backend-only `main` retaining Qt Core. During the
migration, `main` still builds Qt Quick by default. Use
`-DMDR_BUILD_QT_FRONTEND=OFF` for the standalone backend. Removal of the
transitional frontend requires the [migration gates](backend-migration.md).

| Branch | Responsibility | Build and run documentation |
| --- | --- | --- |
| `main` | Shared Qt Core backend, private process, protocol library, common contract, transitional Qt Quick frontend | [Backend](../backend/README.md), [Qt Quick](../gui/README.md) |
| `codex/winui` | Windows WinUI 3 / .NET presentation | [WinUI README](https://github.com/czcqty/SonyHeadphonesClient/blob/codex/winui/gui/winui/README.md) |
| `codex/macos-swift` | macOS Swift / AppKit presentation | [AppKit README](https://github.com/czcqty/SonyHeadphonesClient/blob/codex/macos-swift/gui/macos/README.md) |
| `codex/kde` | KDE Qt Widgets / KDE Frameworks presentation | [KDE README](https://github.com/czcqty/SonyHeadphonesClient/blob/codex/kde/gui/kde/README.md) |
| `codex/gnome` | GTK 4 / libadwaita presentation | [GNOME README](https://github.com/czcqty/SonyHeadphonesClient/blob/codex/gnome/gui/gnome/README.md) |
| `codex/xfce` | GTK 3 / libxfce4ui presentation | [Xfce README](https://github.com/czcqty/SonyHeadphonesClient/blob/codex/xfce/gui/xfce/README.md) |

Native source paths exist only on their respective branches. Each adapter uses
`SoundConnectBackend` and `clients/common/catalog.json`; device logic and backend
hosts must not be copied into a frontend. KDE retains its own Qt/KDE presentation
dependencies independently of the shared Qt Core backend.

The native default launch is isolated simulation. Use explicit `--live` for
Bluetooth and persisted preferences. Quick controls and full settings are
separate native layouts. Their implementation is not proof of real-device or
desktop acceptance; see the dated evidence and remaining checks in
[backend-migration.md](backend-migration.md).

## Working with the branches

From a fresh clone, create a tracking checkout for the desired adapter:

```sh
git fetch origin --prune
git switch --track origin/codex/winui
```

If a local branch already exists, use `git switch codex/winui`. If it is checked
out in another worktree, use the path from `git worktree list` instead. Check
`git status` in each worktree before switching, merging, or cleaning files.

Make shared backend/contract changes on `main`, then merge `main` into each
native branch and run the affected contract and toolkit checks. Keep native
presentation changes on their own branches. Build and test instructions are in
the linked READMEs. The main CMake build does not automatically build all five
native adapters.

The former `rebuild` branch ended at `bfba8aa` (compact Qt tray refactor), which
is already included in `main`. It is retired; continue shared development on
`main`. Deleting its branch reference does not delete that history.

## CI and publishing

- `Build and test` is currently manual (`workflow_dispatch`) and covers the
  Qt frontend ON/OFF matrix on Windows, Linux, and macOS. A branch push alone
  does not run that workflow or establish native frontend parity.
- `Package release` runs manually or for `vX.Y.Z` tags. Its desktop installers
  package the transitional Qt Quick frontend. Backend-only CPack builds produce
  ZIP on Windows and TGZ elsewhere; native application packaging must include
  the shared backend/runtime explicitly.
- The AppKit branch additionally provides its native macOS build workflow.
  A configured workflow is not evidence that its run has passed.

Push branch updates normally, without rewriting shared history. Verify that
remote branch tips match the intended local commits after publication.
