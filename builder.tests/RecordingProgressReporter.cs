using helengine.baseplatform.Builders;
using helengine.baseplatform.Reporting;

namespace helengine.psvita.builder.tests;

/// <summary>
/// Records progress updates produced by the builder during tests.
/// </summary>
sealed class RecordingProgressReporter : IPlatformBuildProgressReporter {
    /// <summary>
    /// Gets the progress updates emitted during the test.
    /// </summary>
    public List<PlatformBuildProgressUpdate> Updates { get; } = [];

    /// <summary>
    /// Records one progress update.
    /// </summary>
    /// <param name="update">Progress update to record.</param>
    public void Report(PlatformBuildProgressUpdate update) {
        Updates.Add(update);
    }
}
