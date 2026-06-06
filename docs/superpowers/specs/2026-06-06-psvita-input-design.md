# PS Vita Input Design

## Goal

Implement shared-runtime PS Vita input support for:

- buttons
- both analog sticks
- front touch

This milestone must support both the main menu and broader runtime input immediately. It must not introduce per-frame heap allocation or leak-prone frame-owned input buffers.

## Scope

Included:

- PS Vita gamepad button mapping through the shared `InputGamepadState`
- left and right analog stick capture
- front touch mapped into the shared pointer pipeline
- zero-garbage per-frame backend behavior

Excluded from this milestone:

- rear touch
- gyro or motion
- Vita-specific gesture abstractions
- text input
- keyboard emulation

## Existing Runtime Contract

The shared engine contract is `IInputBackend::CaptureFrame() -> InputFrameState`.

`InputFrameState` currently contains:

- `Keyboard`
- `Mouse`
- `Pointer`
- `Gamepads`
- `GamepadCount`
- `Text`

The shared `InputSystem` does not trust a platform backend to author the final pointer state directly. Instead, after `CaptureFrame()` it rebuilds `CurrentFrame.Pointer` from `CurrentFrame.Mouse`. That means Vita front touch must be translated into `MouseState` if it is expected to participate in the existing interactable and pointer-hit systems.

## Recommended Approach

Use one PS Vita backend that fills both gamepad and pointer carrier state inside the shared frame contract.

Why this approach:

- it matches the existing engine architecture
- it avoids Vita-only runtime abstractions
- it makes menu and gameplay input available immediately
- it lets front touch reuse the existing pointer interaction system without further core changes

Rejected alternatives:

1. Gamepad-only first, touch later
   - rejected because the requested scope is broader immediately
2. Vita-specific custom touch channel
   - rejected because the current shared input frame does not expose a separate multi-touch runtime model and the result would be platform-specific behavior

## Architecture

`PsVitaInputBackend` remains the single PS Vita platform backend.

It will own persistent storage for:

- one `Array<InputGamepadState>` with length `1`
- one reusable `InputFrameState`
- one reusable `MouseState`
- previous front-touch position
- previous front-touch active state

`CaptureFrame()` mutates those members in place each frame and returns the updated frame snapshot.

This is a hard requirement:

- no per-frame `new Array<InputGamepadState>(1)`
- no per-frame heap allocation of backend-owned input buffers

## Gamepad Mapping

The Vita controller will be exposed as `Gamepads[0]`.

Mapped buttons:

- D-pad up/down/left/right
- South
- East
- West
- North
- LeftShoulder
- RightShoulder
- Start
- Select

Recommended face-button mapping:

- `Cross -> South`
- `Circle -> East`
- `Square -> West`
- `Triangle -> North`

Recommended shoulder and system mapping:

- `L -> LeftShoulder`
- `R -> RightShoulder`
- `Start -> Start`
- `Select -> Select`

Analog mapping:

- left stick -> `LeftStickX`, `LeftStickY`
- right stick -> `RightStickX`, `RightStickY`

Trigger mapping:

- `LeftTrigger = 0`
- `RightTrigger = 0`

because Vita does not expose analog triggers through this controller path.

## Analog Conversion

Vita analog values arrive in an unsigned centered range. The backend should convert them into the signed engine stick range centered at zero.

Rules:

- center at the Vita neutral midpoint
- preserve full directional sign
- clamp to the target `short` range expected by `InputGamepadState`
- preserve both sticks independently

The implementation should keep this conversion in one explicit helper path inside the backend so the mapping is auditable and consistent.

## Front Touch Mapping

Front touch will be represented through the shared pointer path by authoring `MouseState`.

Behavior:

- touch available and finger down:
  - `MouseState.X/Y` become the touch coordinates mapped into engine window space
  - `LeftButton = Pressed`
- no active front touch:
  - `LeftButton = Released`
  - last known coordinates remain valid or are updated according to the backend policy, but no press is emitted

Because `InputSystem` rebuilds `Pointer` from `MouseState`, this automatically enables:

- hover
- press
- release
- pointer hit testing
- existing menu interactables

Pointer delta:

- backend computes touch deltas from previous captured touch position
- those deltas are represented by the `MouseState` position change
- shared `InputSystem` computes final pointer delta from successive mouse snapshots

## Native Vita APIs

The backend should use non-blocking native reads:

- controller polling through the PS Vita control API
- front-touch polling through the PS Vita touch API

The backend should capture current state only. It should not implement its own press/release history beyond what is needed for stable touch coordinate tracking, because the shared `InputSystem` already computes transitions from successive frames.

## Memory and Lifetime Rules

The backend must not allocate frame-owned state repeatedly.

Rules:

- allocate persistent gamepad storage once
- reuse the same storage every frame
- mutate frame data in place
- do not hand ownership of transient heap buffers to the input system on each capture

This is specifically intended to avoid the leak pattern seen in other platform implementations where each frame creates a fresh `Gamepads` array that is never reclaimed by the native runtime path.

## Error Handling

Input capture should be resilient rather than exception-heavy.

Rules:

- unavailable touch data should fall back to no active touch
- unavailable controller data should fall back to disconnected or zeroed control state for the frame
- input polling failures should not crash the runtime loop unless initialization is fundamentally impossible

This backend is part of the live frame loop, so the failure mode should be safe and deterministic.

## Testing Strategy

Start with failing source-audit tests.

Required audits:

1. `PsVitaInputBackend::CaptureFrame()` is no longer an empty default return
2. Vita backend source references both control and touch polling
3. backend owns persistent gamepad storage
4. `CaptureFrame()` does not allocate a new gamepad array each frame
5. source shows the shared gamepad button mapping for core Vita buttons

After source audits pass:

- run `dotnet test builder.tests\helengine.psvita.builder.tests.csproj`
- rebuild via the real editor CLI `psvita` path
- verify in Vita3K that menu interaction works with buttons, analog, and front touch

## Success Criteria

The milestone is complete when all of the following are true:

- Vita menu can be navigated by controller buttons
- Vita menu can be navigated by analog input where bindings support it
- front touch participates in existing pointer interaction
- backend introduces no per-frame heap allocation for gamepad frame storage
- builder tests pass
- editor CLI `--build psvita` succeeds
- Vita3K boot and interaction remain stable
