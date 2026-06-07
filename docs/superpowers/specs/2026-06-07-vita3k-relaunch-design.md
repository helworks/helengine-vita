# PS Vita Vita3K Relaunch Design

## Goal

Update the PS Vita Vita3K launcher script so every new launch starts from a clean emulator process state without requiring manual cleanup.

## Approved Behavior

Before the script deletes the default installed title or launches the requested VPK, it must force-stop every running `Vita3K` process on the machine.

## Scope

- Change `tools/launch-vita3k.ps1`
- Extend `builder.tests/Vita3KLauncherScriptAuditTests.cs`

## Design

The launcher keeps its existing validation and launch contract:

- validate `-VpkPath`
- validate the target is a `.vpk` file
- validate the configured `Vita3K.exe` path
- optionally delete installed title `HLEN00001`
- launch the requested VPK

The new behavior is inserted after path validation and before title deletion:

1. Query all running `Vita3K` processes with `Get-Process`.
2. If any are found, force-stop them with `Stop-Process -Force`.
3. Continue with the existing delete-and-launch flow.

This design intentionally kills all running Vita3K instances, not only instances that match the configured executable path, because the approved requirement is deterministic force-stop behavior rather than selective matching.

## Error Handling

- If no `Vita3K` processes are running, the script continues silently.
- If stopping a running Vita3K process fails, the script should fail because the current launcher already uses strict-stop behavior and should not proceed in a partially cleaned state.

## Test Coverage

The source audit must verify that the launcher:

- references `Get-Process`
- references `Stop-Process`
- uses `-Force`
- performs that process cleanup before launch

## Non-Goals

- waiting for graceful Vita3K shutdown
- preserving manually running Vita3K sessions
- differentiating between multiple Vita3K installs
