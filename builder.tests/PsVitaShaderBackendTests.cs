using Xunit;

namespace helengine.psvita.builder.tests;

/// <summary>
/// Verifies the host backend that submits shader compilation to the Vita compiler VPK.
/// </summary>
public sealed class PsVitaShaderBackendTests {
    /// <summary>
    /// Ensures a matching successful outbox result returns its validated PVSA artifact unchanged.
    /// </summary>
    [Fact]
    public void Compile_WhenMatchingSuccessfulOutboxResultExists_ReturnsValidatedStageBinary() {
        byte[] artifactBytes = CreateValidVertexArtifactBytes();
        PsVitaShaderBackendTestExchange exchange = new(artifactBytes, false);
        PsVitaShaderBackend backend = new(exchange);
        ShaderCompileRequest request = CreateVertexRequest();

        ShaderCompileResult result = backend.Compile(request, new PsVitaShaderBackendTestIncludeResolver());

        Assert.Equal(ShaderCompileTarget.PsVita, backend.Target);
        Assert.Equal(artifactBytes, result.Binary.Bytecode);
        Assert.Equal(CreateExpectedDeviceJobHash(request), exchange.SubmittedJob.JobHash);
    }

    /// <summary>
    /// Ensures the backend rejects an outbox result whose job identity does not match the submitted request.
    /// </summary>
    [Fact]
    public void Compile_WhenOutboxJobHashIsStale_ThrowsInsteadOfUsingTheArtifact() {
        PsVitaShaderBackendTestExchange exchange = new(CreateValidVertexArtifactBytes(), true);
        PsVitaShaderBackend backend = new(exchange);

        Assert.Throws<InvalidOperationException>(() => backend.Compile(CreateVertexRequest(), new PsVitaShaderBackendTestIncludeResolver()));
    }

    /// <summary>
    /// Ensures Forward Standard Shader requests submit the supported Vita texture-Lambert source profile.
    /// </summary>
    [Fact]
    public void Compile_WhenForwardStandardShaderIsRequested_SubmitsLoweredVitaCgSource() {
        PsVitaShaderBackendTestExchange exchange = new(CreateValidVertexArtifactBytes(), false);
        PsVitaShaderBackend backend = new(exchange);
        ShaderCompileRequest request = CreateVertexRequest("ForwardStandardShader.vs");

        backend.Compile(request, new PsVitaShaderBackendTestIncludeResolver());

        Assert.Contains("HelengineWorldViewProjection", exchange.SubmittedSourceFiles[0].SourceText, StringComparison.Ordinal);
        Assert.Contains("TEXCOORD0", exchange.SubmittedSourceFiles[0].SourceText, StringComparison.Ordinal);
    }

    /// <summary>
    /// Creates a vertex compilation request compatible with the first Vita shader compiler backend.
    /// </summary>
    /// <returns>Validated vertex shader compile request.</returns>
    static ShaderCompileRequest CreateVertexRequest(string programName = "ForwardStandard") {
        return new ShaderCompileRequest(
            new ShaderSourceInfo("shaders/standard.cg", "float4 VS(float4 position : POSITION) : POSITION { return position; }"),
            programName,
            "VS",
            ShaderStage.Vertex,
            ShaderCompileTarget.PsVita,
            new ShaderModel(4, 0),
            "default",
            [],
            new ShaderCompileOptions(new ShaderBindingPolicy(0, 0, 0, 0, 0), false, true, true));
    }

    /// <summary>
    /// Creates the expected Vita-owned device job hash from the shared cache-key contract.
    /// </summary>
    /// <param name="request">Shader compile request submitted by the backend.</param>
    /// <returns>Filesystem-safe upper-case SHA-256 job identity.</returns>
    static string CreateExpectedDeviceJobHash(ShaderCompileRequest request) {
        string cacheKey = ShaderCompileRequestIdentity.CreateCacheKey(request, new ShaderSourceHasher()).ToString();
        return Convert.ToHexString(System.Security.Cryptography.SHA256.HashData(System.Text.Encoding.UTF8.GetBytes(cacheKey)));
    }

    /// <summary>
    /// Creates a valid serialized PVSA vertex artifact used as a device outbox fixture.
    /// </summary>
    /// <returns>Serialized valid PVSA artifact bytes.</returns>
    static byte[] CreateValidVertexArtifactBytes() {
        PsVitaShaderArtifact artifact = new(
            "VP",
            "libshacccg-test",
            Convert.ToHexString(System.Security.Cryptography.SHA256.HashData("source"u8)),
            "VS",
            "O3-W4",
            [1, 2, 3]);
        return new PsVitaShaderArtifactBinarySerializer().Serialize(artifact);
    }
}
