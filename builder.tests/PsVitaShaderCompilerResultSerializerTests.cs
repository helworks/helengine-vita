using Xunit;

namespace helengine.psvita.builder.tests;

/// <summary>
/// Verifies strict validation of results returned by the Vita compiler outbox.
/// </summary>
public sealed class PsVitaShaderCompilerResultSerializerTests {
    /// <summary>
    /// Ensures a successful result cannot claim a non-canonical artifact hash.
    /// </summary>
    [Fact]
    public void Deserialize_WhenArtifactHashIsNotCanonical_RejectsTheResult() {
        const string json = "{\"formatVersion\":1,\"jobHash\":\"AABB\",\"stages\":[{\"stageId\":\"vertex\",\"success\":true,\"diagnostic\":\"\",\"artifactPath\":\"artifact/vertex.pvsa\",\"artifactHash\":\"BAD\",\"programByteCount\":1}]}";

        Assert.Throws<InvalidOperationException>(() => new PsVitaShaderCompilerResultSerializer().Deserialize(json));
    }
}
