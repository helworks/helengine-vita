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

    /// <summary>
    /// Verifies that a prior builder-owned deserializer is removed when it is no longer in the generated set.
    /// </summary>
    [Fact]
    public void PruneStaleGeneratedDeserializers_removesOnlyPreviousGeneratedFiles() {
        string testRoot = Path.Combine(PsVitaRepositoryPathResolver.ResolvePath("builder.tests"), ".test-output", Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(testRoot);
        try {
            string manifestPath = Path.Combine(testRoot, "psvita-generated-runtime-deserializers.manifest");
            File.WriteAllLines(manifestPath, ["GeneratedRuntimeRemovedComponentDeserializer"]);
            string staleCppPath = Path.Combine(testRoot, "GeneratedRuntimeRemovedComponentDeserializer.cpp");
            string staleHppPath = Path.Combine(testRoot, "GeneratedRuntimeRemovedComponentDeserializer.hpp");
            string currentCppPath = Path.Combine(testRoot, "GeneratedRuntimeCurrentComponentDeserializer.cpp");
            File.WriteAllText(staleCppPath, "old");
            File.WriteAllText(staleHppPath, "old");
            File.WriteAllText(currentCppPath, "current");

            MethodInfo prune = typeof(PsVitaGeneratedRuntimeComponentSupportWriter).GetMethod(
                "PruneStaleGeneratedDeserializers",
                BindingFlags.Static | BindingFlags.NonPublic);
            Assert.NotNull(prune);
            prune.Invoke(null, [testRoot, manifestPath, new[] { "GeneratedRuntimeCurrentComponentDeserializer" }, Array.Empty<string>()]);

            Assert.False(File.Exists(staleCppPath));
            Assert.False(File.Exists(staleHppPath));
            Assert.True(File.Exists(currentCppPath));
            Assert.Equal("GeneratedRuntimeCurrentComponentDeserializer", File.ReadAllText(manifestPath).Trim());
        } finally {
            Directory.Delete(testRoot, true);
        }
    }

    /// <summary>
    /// Verifies an editor-owned registration is left alone even when an old fallback manifest remains beside it.
    /// </summary>
    [Fact]
    public void EnsureGeneratedRuntimeSupport_preservesEditorRegistration_withLeftoverManifest() {
        string testRoot = Path.Combine(PsVitaRepositoryPathResolver.ResolvePath("builder.tests"), ".test-output", Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(testRoot);
        try {
            string registrationPath = Path.Combine(testRoot, "GeneratedRuntimeComponentDeserializerRegistration.cpp");
            string editorRegistration = "// EditorGeneratedCoreRegenerationService output\nvoid RegisterGeneratedRuntimeComponentDeserializers() {}\n";
            File.WriteAllText(registrationPath, editorRegistration);
            File.WriteAllText(Path.Combine(testRoot, "psvita-generated-runtime-deserializers.manifest"), "GeneratedRuntimeOldComponentDeserializer");
            string oldDeserializerPath = Path.Combine(testRoot, "GeneratedRuntimeOldComponentDeserializer.cpp");
            File.WriteAllText(oldDeserializerPath, "editor-owned or shared stale output");

            new PsVitaGeneratedRuntimeComponentSupportWriter().EnsureGeneratedRuntimeSupport(testRoot, Array.Empty<string>());

            Assert.Equal(editorRegistration, File.ReadAllText(registrationPath));
            Assert.True(File.Exists(oldDeserializerPath));
        } finally {
            Directory.Delete(testRoot, true);
        }
    }
}
