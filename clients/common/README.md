# Shared native adapter contract

`catalog.json` contains labels and bindings for the complete device command
surface. It contains no painted controls, styles, geometry, Bluetooth commands
or business validation. Each native toolkit owns its controls and interaction.
Run `python clients/common/generate_catalog.py` after editing the catalog source.

`client.py` provides the common GTK pipe transport, with UI-thread dispatch
supplied by each presentation. Other languages use their native process APIs
against the same [protocol](../../backend/README.md). No frontend builds a
private copy of the backend host.

`test_catalog.py` compares bindings against the running host's full command
contract and exercises available controls on representative device profiles.
This proves binding coverage, not visual or desktop integration parity.

`messages.json` is the shared English/Chinese presentation dictionary, including
all reported equalizer IDs, touch assignments, connection error codes, protocol
errors, and parameterized messages. User/device names, track metadata and opaque
OS diagnostics stay verbatim. The generator embeds this dictionary in catalog.json;
frontends need only that catalog at runtime. Run `test_localization.py` to check
coverage against the current backend sources.

The `application` page binds to the shared preference target. Default preview
preferences are English/system and session-only. Live defaults come from migrated
Qt settings or the system locale. Native widgets own theme observation and window
lifetime. Shortcut parity is Ctrl+Alt+P (play/pause), N (next), M (quick controls):
global on Windows, application-local on Linux and Ctrl+Option on macOS, matching
the previous non-Windows Qt shortcut scope. Shortcut registration failures must
be visible and must not disable the application's local fallback.
