using System.Text.Json.Nodes;

namespace helengine.psvita.builder;

/// <summary>
/// Creates the metadata-only platform plugin manifest used by editor platform discovery.
/// </summary>
public static class PsVitaPlatformPluginManifest {
    /// <summary>
    /// Creates the JSON payload for the PS Vita platform plugin manifest.
    /// </summary>
    /// <returns>Platform plugin manifest JSON object.</returns>
    public static JsonObject Create() {
        return new JsonObject {
            ["platformId"] = "psvita",
            ["displayName"] = "PS Vita",
            ["builderAssemblyPath"] = "builder/helengine.psvita.builder.dll",
            ["definitionFactoryType"] = "helengine.psvita.builder.PsVitaPlatformDefinitionFactory"
        };
    }
}
