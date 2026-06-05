using System.Text.Json.Nodes;
using helengine.psvita.builder;
using Xunit;

namespace helengine.psvita.builder.tests;

/// <summary>
/// Verifies the checked-in PS Vita platform plugin manifest matches the builder metadata.
/// </summary>
public sealed class PsVitaPlatformPluginManifestTests {
    /// <summary>
    /// Ensures the generated manifest publishes only platform plugin discovery metadata.
    /// </summary>
    [Fact]
    public void Create_whenSerialized_contains_psvita_discovery_metadata() {
        JsonObject manifest = PsVitaPlatformPluginManifest.Create();

        Assert.Equal("psvita", manifest["platformId"]?.GetValue<string>());
        Assert.Equal("PS Vita", manifest["displayName"]?.GetValue<string>());
        Assert.Equal("builder/helengine.psvita.builder.dll", manifest["builderAssemblyPath"]?.GetValue<string>());
        Assert.Equal("helengine.psvita.builder.PsVitaPlatformDefinitionFactory", manifest["definitionFactoryType"]?.GetValue<string>());
        Assert.Null(manifest["runtimePayloadTypes"]);
        Assert.Null(manifest["serializerHooks"]);
    }

    /// <summary>
    /// Ensures the checked-in plugin manifest file matches the generated payload.
    /// </summary>
    [Fact]
    public void CheckedInPluginManifest_matchesGeneratedPayload() {
        string manifestFilePath = PsVitaRepositoryPathResolver.ResolvePath("platform-plugin.json");
        string expectedJson = PsVitaPlatformPluginManifest.Create().ToJsonString();
        string actualJson = JsonNode.Parse(File.ReadAllText(manifestFilePath))?.ToJsonString();

        Assert.Equal(expectedJson, actualJson);
    }
}
