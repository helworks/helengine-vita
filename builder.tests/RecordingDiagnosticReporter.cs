using helengine.baseplatform.Builders;
using helengine.baseplatform.Reporting;

namespace helengine.psvita.builder.tests;

/// <summary>
/// Records diagnostics produced by the builder during tests.
/// </summary>
sealed class RecordingDiagnosticReporter : IPlatformBuildDiagnosticReporter {
    /// <summary>
    /// Gets the diagnostics emitted during the test.
    /// </summary>
    public List<PlatformBuildDiagnostic> Diagnostics { get; } = [];

    /// <summary>
    /// Records one diagnostic.
    /// </summary>
    /// <param name="diagnostic">Diagnostic to record.</param>
    public void Report(PlatformBuildDiagnostic diagnostic) {
        Diagnostics.Add(diagnostic);
    }
}
