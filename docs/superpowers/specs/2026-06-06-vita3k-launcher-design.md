# Vita3K Launcher Design

## Goal

Standardize PS Vita emulator launch for this repository with one explicit PowerShell entrypoint that always runs a specific `.vpk` build artifact.

This launcher must:

- take an explicit VPK path
- validate inputs before touching Vita3K
- surface real launch and deletion errors clearly
- support the normal "fresh install from this exact build artifact" workflow

## Scope

Included:

- one repo-local PowerShell launcher script
- required `-VpkPath` input
- upfront validation of the VPK path and Vita3K executable path
- default deletion of installed title `HLEN00001` before launch
- optional bypass for title deletion
- clear failure behavior for invalid inputs and real process failures

Excluded from this milestone:

- automatic build discovery
- screenshot capture
- log scraping
- automated input
- project-specific hardcoded output paths

## Current Workflow Problem

PS Vita launch is currently driven manually and inconsistently.

Problems with the current state:

- fresh build validation depends on ad hoc commands
- failure behavior is not standardized
- missing-file and wrong-path mistakes are easy to make
- the workflow is harder to reuse across future PS Vita testing passes

The repository already has a standardized editor build command. Emulator launch should match that level of consistency.

## Recommended Approach

Add one explicit launcher script at `tools/launch-vita3k.ps1`.

Why this approach:

- it standardizes the emulator entrypoint
- it keeps the contract simple: build a `.vpk`, then launch that exact `.vpk`
- it avoids hidden "latest build" heuristics
- it surfaces path errors before any emulator work starts
- it supports both clean reinstall loops and faster keep-installed loops

Rejected alternatives:

1. Hardcode one City build output path
   - rejected because the launcher should remain reusable across projects and outputs
2. Launch the installed title only
   - rejected because the requested workflow is to run a specific VPK path each time
3. Search for the latest VPK automatically
   - rejected because it introduces ambiguity and weakens reproducibility

## Script Contract

The script should expose this primary interface:

- required `-VpkPath`
- optional `-KeepInstalledTitle`

Behavior:

- `-VpkPath` identifies the exact build artifact to launch
- if `-KeepInstalledTitle` is not provided, the script deletes installed title `HLEN00001` before launch
- if `-KeepInstalledTitle` is provided, the script skips deletion and launches directly

The script should resolve Vita3K from this fixed path:

- `C:\dev\helworks\emus\vita-3k\Vita3K.exe`

## Launch Flow

The launcher should follow this deterministic sequence:

1. Enable strict PowerShell behavior
2. Resolve and validate `-VpkPath`
3. Resolve and validate the Vita3K executable path
4. If `-KeepInstalledTitle` is not set:
   - run `Vita3K.exe -d HLEN00001`
   - tolerate the known "title not installed" case
   - surface any other real delete failure
5. Launch Vita3K with the provided VPK path
6. Return control immediately without waiting for emulator exit
7. Print the launched VPK path and whether deletion was performed

This makes the launcher predictable and suitable as the standard PS Vita run path.

## Validation Rules

Validation must happen before any emulator side effects.

Rules:

- if `-VpkPath` is missing, throw with usage guidance
- if the resolved VPK path does not exist, throw
- if the resolved path is not a file, throw
- if the file extension is not `.vpk`, throw
- if `C:\dev\helworks\emus\vita-3k\Vita3K.exe` does not exist, throw and print that exact path

The script should not attempt partial recovery from bad inputs. This is a standardization tool, so failing fast is preferable to guessing.

## Error Handling

Error forwarding is a core requirement for this script.

Rules:

- use `Set-StrictMode -Version Latest`
- set `$ErrorActionPreference = 'Stop'`
- use explicit path checks with `Test-Path -LiteralPath`
- run the delete step as a blocking process invocation so delete failures are observable
- only suppress the known "not installed" delete case
- rethrow launch failures instead of swallowing them

This ensures the launcher becomes trustworthy infrastructure instead of a best-effort helper.

## Output Expectations

On success, the script should print concise operational feedback:

- resolved VPK path
- resolved Vita3K path
- whether `HLEN00001` was deleted or preserved
- confirmation that Vita3K was launched

On failure, the script should stop with a specific error describing the invalid path or failed process.

## Documentation

The repository README should include one short usage example that matches the standard build flow:

1. build with `C:\dev\helworks\helengine\artifacts\build-platform.ps1`
2. launch with `tools/launch-vita3k.ps1 -VpkPath ...`

This keeps the build and run loop explicit and repeatable.

## Testing Strategy

Start with failing source-audit or script-audit coverage where appropriate.

Required verification:

1. script exists at `tools/launch-vita3k.ps1`
2. script requires `-VpkPath`
3. script validates the VPK path before launch
4. script validates the Vita3K executable path before launch
5. script supports skipping delete with `-KeepInstalledTitle`
6. script uses the configured `HLEN00001` delete path by default

After those checks:

- run the launcher against a known-good PS Vita VPK
- confirm Vita3K starts from the specified artifact
- confirm invalid paths fail before emulator work begins

## Success Criteria

The milestone is complete when all of the following are true:

- there is one standardized launcher script for Vita3K
- the script launches a specific `.vpk` path each time
- invalid file paths fail immediately and clearly
- Vita3K executable lookup is validated explicitly
- default behavior refreshes installed title `HLEN00001`
- an optional fast path exists to keep the installed title
- README usage stays aligned with the standard PS Vita build-and-run workflow
