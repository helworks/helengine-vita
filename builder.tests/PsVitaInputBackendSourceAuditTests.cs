using System;
using System.IO;
using Xunit;

namespace helengine.psvita.builder.tests;

/// <summary>
/// Audits the PS Vita input backend source so runtime input support cannot regress back to the empty bootstrap stub.
/// </summary>
public sealed class PsVitaInputBackendSourceAuditTests {
    /// <summary>
    /// Verifies the backend no longer returns an empty default frame and instead references native control and touch polling.
    /// </summary>
    [Fact]
    public void Source_whenCapturingInput_usesNativeControllerAndFrontTouchPolling() {
        string sourcePath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "PsVitaInputBackend.cpp");
        string sourceCode = File.ReadAllText(sourcePath);

        Assert.DoesNotContain("return ::InputFrameState();", sourceCode, StringComparison.Ordinal);
        Assert.Contains("sceCtrl", sourceCode, StringComparison.Ordinal);
        Assert.Contains("sceTouch", sourceCode, StringComparison.Ordinal);
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

        Assert.Contains("Array<InputGamepadState>* PersistentGamepads;", headerSource, StringComparison.Ordinal);
        Assert.Contains("::InputFrameState CachedFrame;", headerSource, StringComparison.Ordinal);
        Assert.DoesNotContain("new Array<InputGamepadState>(1)", sourceCode, StringComparison.Ordinal);
        Assert.Contains("PersistentGamepads", sourceCode, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies the backend maps Vita pad buttons into the shared gamepad contract and translates front touch into the mouse-backed pointer path.
    /// </summary>
    [Fact]
    public void Source_whenCapturingInput_mapsSharedButtonsAndTranslatesTouchIntoMouseState() {
        string sourcePath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "PsVitaInputBackend.cpp");
        string sourceCode = File.ReadAllText(sourcePath);

        Assert.Contains("InputGamepadButton::DPadUp", sourceCode, StringComparison.Ordinal);
        Assert.Contains("InputGamepadButton::South", sourceCode, StringComparison.Ordinal);
        Assert.Contains("InputGamepadButton::East", sourceCode, StringComparison.Ordinal);
        Assert.Contains("InputGamepadButton::Start", sourceCode, StringComparison.Ordinal);
        Assert.Contains("set_LeftStickX", sourceCode, StringComparison.Ordinal);
        Assert.Contains("set_RightStickX", sourceCode, StringComparison.Ordinal);
        Assert.Contains("MouseState(", sourceCode, StringComparison.Ordinal);
        Assert.Contains("ButtonState::Pressed", sourceCode, StringComparison.Ordinal);
        Assert.Contains("ButtonState::Released", sourceCode, StringComparison.Ordinal);
    }
}
