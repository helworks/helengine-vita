using System.Reflection;
using Xunit;

namespace helengine.psvita.builder.tests;

/// <summary>
/// Verifies that Vita generated runtime component support accepts only current engine component identifiers.
/// </summary>
public sealed class PsVitaGeneratedRuntimeComponentSupportWriterTests {
    /// <summary>
    /// Verifies that the writer names the current layout component and contains no deleted anchor alias or normalizer.
    /// </summary>
    [Fact]
    public void Source_usesCurrentLayoutId_withoutDeletedAnchorAliasOrNormalizer() {
        string source = File.ReadAllText(PsVitaRepositoryPathResolver.ResolvePath("builder", "PsVitaGeneratedRuntimeComponentSupportWriter.cs"));

        Assert.Contains("[\"helengine.LayoutComponent\"] = typeof(LayoutComponent)", source, StringComparison.Ordinal);
        Assert.Contains("SupportedEngineComponentTypesById = new(StringComparer.Ordinal)", source, StringComparison.Ordinal);
        Assert.DoesNotContain("helengine.AnchorComponent", source, StringComparison.Ordinal);
        Assert.DoesNotContain("AnchorComponent", source, StringComparison.Ordinal);
        Assert.DoesNotContain("NormalizeLegacyEngineComponentTypeId", source, StringComparison.Ordinal);
        Assert.DoesNotContain(", helengine.core", source, StringComparison.Ordinal);
    }

    /// <summary>
    /// Verifies that exact current component ids resolve while assembly-qualified legacy ids do not get rewritten.
    /// </summary>
    [Fact]
    public void ResolveRequiredEngineComponentTypes_acceptsExactCurrentId_withoutNormalizingLegacyId() {
        PsVitaGeneratedRuntimeComponentSupportWriter writer = new();
        MethodInfo resolver = typeof(PsVitaGeneratedRuntimeComponentSupportWriter).GetMethod(
            "ResolveRequiredEngineComponentTypes",
            BindingFlags.Instance | BindingFlags.NonPublic);

        Assert.NotNull(resolver);
        IReadOnlyList<Type> exactCurrentTypes = (IReadOnlyList<Type>)resolver.Invoke(
            writer,
            [new[] { "helengine.LayoutComponent" }]);
        IReadOnlyList<Type> legacyQualifiedTypes = (IReadOnlyList<Type>)resolver.Invoke(
            writer,
            [new[] { "helengine.LayoutComponent, helengine.core" }]);
        IReadOnlyList<Type> differentlyCasedTypes = (IReadOnlyList<Type>)resolver.Invoke(
            writer,
            [new[] { "helengine.layoutcomponent" }]);

        Assert.Contains(typeof(global::helengine.LayoutComponent), exactCurrentTypes);
        Assert.Empty(legacyQualifiedTypes);
        Assert.Empty(differentlyCasedTypes);
    }
}
