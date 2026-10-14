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
