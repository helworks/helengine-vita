using Xunit;

namespace helengine.psvita.builder.tests;

/// <summary>
/// Audits the PS Vita generated-core compatibility shim so expanded scene builds can compile Bepu-generated narrow vector references without overriding shared generated runtime ownership.
/// </summary>
public sealed class PsVitaGeneratedCoreCompatibilitySourceAuditTests {
    /// <summary>
    /// Verifies generated-core builds force-include the Vita compatibility shim and that the shim only contributes the expected Vector2, Vector3, and Vector4 aliases for current generated cores.
    /// </summary>
    [Fact]
    public void Source_whenGeneratedCoreIncludesBepuInt2_forcesCompatibilityAliasIntoEveryCompileUnit() {
        string cmakePath = PsVitaRepositoryPathResolver.ResolvePath("CMakeLists.txt");
        string compatibilityHeaderPath = PsVitaRepositoryPathResolver.ResolvePath("src", "platform", "psvita", "PsVitaGeneratedCoreCompatibility.hpp");
        string cmakeSource = File.ReadAllText(cmakePath);
        string compatibilityHeaderSource = File.ReadAllText(compatibilityHeaderPath);

        Assert.Contains("PsVitaGeneratedCoreCompatibility.hpp", cmakeSource, StringComparison.Ordinal);
        Assert.Contains("target_compile_options(${PROJECT_NAME} PRIVATE", cmakeSource, StringComparison.Ordinal);
        Assert.Contains("-include ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/psvita/PsVitaGeneratedCoreCompatibility.hpp", cmakeSource, StringComparison.Ordinal);
        Assert.Contains("#if HELENGINE_PSVITA_HAS_GENERATED_CORE", compatibilityHeaderSource, StringComparison.Ordinal);
        Assert.DoesNotContain("class AppContext", compatibilityHeaderSource, StringComparison.Ordinal);
        Assert.Contains("#include \"float2.hpp\"", compatibilityHeaderSource, StringComparison.Ordinal);
        Assert.Contains("#include \"float3.hpp\"", compatibilityHeaderSource, StringComparison.Ordinal);
        Assert.Contains("#include \"float4.hpp\"", compatibilityHeaderSource, StringComparison.Ordinal);
        Assert.Contains("using Vector2 = ::float2;", compatibilityHeaderSource, StringComparison.Ordinal);
        Assert.Contains("using Vector3 = ::float3;", compatibilityHeaderSource, StringComparison.Ordinal);
        Assert.Contains("using Vector4 = ::float4;", compatibilityHeaderSource, StringComparison.Ordinal);
    }
}
