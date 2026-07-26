using System;
using System.IO;
using Xunit;

namespace helengine.psvita.builder.tests;

/// <summary>
/// Audits the PS Vita input backend source so runtime input support cannot regress back to the empty bootstrap stub.
/// </summary>
public sealed class PsVitaInputBackendSourceAuditTests {
    /// <summary>
    /// Verifies the backend no longer returns an empty default frame and instead references native controller polling.
    /// </summary>
    [Fact]
    public void Source_whenCapturingInput_usesNativeControllerPolling() {
        string sourcePath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "PsVitaInputBackend.cpp");
        string sourceCode = File.ReadAllText(sourcePath);

        Assert.DoesNotContain("return ::InputFrameState();", sourceCode, StringComparison.Ordinal);
        Assert.Contains("sceCtrl", sourceCode, StringComparison.Ordinal);
        Assert.DoesNotContain("sceTouch", sourceCode, StringComparison.Ordinal);
        Assert.Contains("CaptureFrame()", sourceCode, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies the backend owns persistent input storage instead of allocating new frame-owned gamepad arrays every frame.
    /// </summary>
    [Fact]
    public void Source_whenCapturingInput_reusesPersistentGamepadStorageWithoutPerFrameAllocation() {
        string headerPath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "PsVitaInputBackend.hpp");
        string sourcePath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "PsVitaInputBackend.cpp");
        string headerSource = File.ReadAllText(headerPath);
        string sourceCode = File.ReadAllText(sourcePath);

        Assert.Contains("Array<InputGamepadState>* GamepadBuffers[2]", headerSource, StringComparison.Ordinal);
        Assert.Contains("int ActiveGamepadBufferIndex;", headerSource, StringComparison.Ordinal);
        Assert.DoesNotContain("::InputFrameState CachedFrame;", headerSource, StringComparison.Ordinal);
        Assert.DoesNotContain("PersistentGamepads", sourceCode, StringComparison.Ordinal);
        Assert.Contains("GamepadBuffers[ActiveGamepadBufferIndex]", sourceCode, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies each captured Vita frame selects the alternate gamepad buffer so previous-frame button and stick transitions remain observable.
    /// </summary>
    [Fact]
    public void Source_whenCapturingConsecutiveFrames_alternatesGamepadBuffers() {
        string headerPath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "PsVitaInputBackend.hpp");
        string sourcePath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "PsVitaInputBackend.cpp");
        string headerSource = File.ReadAllText(headerPath);
        string sourceCode = File.ReadAllText(sourcePath);

        Assert.Contains("GamepadBuffers[2]", headerSource, StringComparison.Ordinal);
        Assert.Contains("ActiveGamepadBufferIndex = (ActiveGamepadBufferIndex + 1) % 2;", sourceCode, StringComparison.Ordinal);
        Assert.Contains("frameState.set_Gamepads(gamepads);", sourceCode, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies the backend maps Vita pad buttons into the shared gamepad contract without importing desktop mouse or keyboard state.
    /// </summary>
    [Fact]
    public void Source_whenCapturingInput_mapsSharedButtonsWithoutDesktopMouseOrKeyboardState() {
        string headerPath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "PsVitaInputBackend.hpp");
        string sourcePath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "PsVitaInputBackend.cpp");
        string headerSource = File.ReadAllText(headerPath);
        string sourceCode = File.ReadAllText(sourcePath);

        Assert.Contains("InputGamepadButton::DPadUp", sourceCode, StringComparison.Ordinal);
        Assert.Contains("InputGamepadButton::South", sourceCode, StringComparison.Ordinal);
        Assert.Contains("InputGamepadButton::East", sourceCode, StringComparison.Ordinal);
        Assert.Contains("InputGamepadButton::Start", sourceCode, StringComparison.Ordinal);
        Assert.Contains("set_LeftStickX", sourceCode, StringComparison.Ordinal);
        Assert.Contains("set_RightStickX", sourceCode, StringComparison.Ordinal);
        Assert.DoesNotContain("MouseState", headerSource, StringComparison.Ordinal);
        Assert.DoesNotContain("KeyboardState", headerSource, StringComparison.Ordinal);
        Assert.DoesNotContain("MouseState", sourceCode, StringComparison.Ordinal);
        Assert.DoesNotContain("KeyboardState", sourceCode, StringComparison.Ordinal);
    }
}
