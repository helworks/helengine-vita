using Xunit;

namespace helengine.psvita.builder.tests;

/// <summary>
/// Verifies the host folder protocol used to transfer jobs and artifacts to the Vita compiler VPK.
/// </summary>
public sealed class PsVitaShaderCompilerExchangeTests {
    /// <summary>
    /// Ensures submitting a job writes its declared source before publishing the deterministic manifest.
    /// </summary>
    [Fact]
    public void Submit_WhenJobIsValid_WritesTheManifestAndDeclaredSourceIntoTheInbox() {
        string rootPath = Path.Combine(Path.GetTempPath(), "helengine-psvita-exchange-tests", Guid.NewGuid().ToString("N"));
        try {
            PsVitaShaderCompilerJob job = PsVitaShaderCompilerJob.Create(
                "AABB",
                [new PsVitaShaderCompilerStageRequest("vertex", "source/AABB/vertex.cg", "VS", "VP", "O3-W4")]);
            PsVitaShaderCompilerExchange exchange = new(rootPath);

            exchange.Submit(job, [new PsVitaShaderCompilerSourceFile("source/AABB/vertex.cg", "void VS() { }")]);

            Assert.Equal("void VS() { }", File.ReadAllText(Path.Combine(rootPath, "inbox", "source", "AABB", "vertex.cg")));
            Assert.Contains("\"jobHash\":\"AABB\"", File.ReadAllText(Path.Combine(rootPath, "inbox", "manifest.json")), StringComparison.Ordinal);
        } finally {
            if (Directory.Exists(rootPath)) {
                Directory.Delete(rootPath, true);
            }
        }
    }
}
