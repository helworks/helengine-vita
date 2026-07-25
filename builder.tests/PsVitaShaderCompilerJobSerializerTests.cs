using Xunit;

namespace helengine.psvita.builder.tests;

/// <summary>
/// Verifies deterministic serialization of jobs sent to the fixed Vita compiler inbox.
/// </summary>
public sealed class PsVitaShaderCompilerJobSerializerTests {
    /// <summary>
    /// Ensures ordered stage requests retain their compiler identity in the JSON inbox manifest.
    /// </summary>
    [Fact]
    public void Serialize_WhenStagesAreSupplied_WritesStableJobHashAndOrderedStageRequests() {
        PsVitaShaderCompilerJob job = PsVitaShaderCompilerJob.Create(
            "AABB",
            [new PsVitaShaderCompilerStageRequest("vertex", "source/standard.cg", "VS", "VP", "O3-W4")]);

        string json = new PsVitaShaderCompilerJobSerializer().Serialize(job);

        Assert.Contains("\"jobHash\":\"AABB\"", json, StringComparison.Ordinal);
        Assert.Contains("\"profile\":\"VP\"", json, StringComparison.Ordinal);
    }
}
