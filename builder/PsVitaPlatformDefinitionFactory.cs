using helengine.baseplatform.Definitions;
using helengine.baseplatform.Profiles;

namespace helengine.psvita.builder;

/// <summary>
/// Creates the typed PS Vita platform metadata exposed to the editor.
/// </summary>
public static class PsVitaPlatformDefinitionFactory {
    /// <summary>
    /// Creates the PS Vita platform definition consumed by the editor build graph.
    /// </summary>
    /// <returns>The PS Vita platform definition.</returns>
    public static PlatformDefinition Create() {
        return new PlatformDefinition(
            "psvita",
            "PS Vita",
            [
                new PlatformBuildProfileDefinition(
                    "debug",
                    "Debug",
                    "Debug PS Vita homebrew VPK build",
                    "psvita-forward",
                    "default",
                    [
                        new PlatformSettingDefinition(
                            "texture-scale-percent",
                            "Texture Scale Percent",
                            PlatformSettingKind.Text,
                            "100",
                            true,
                            []),
                        new PlatformSettingDefinition(
                            "shader-variant-pruning",
                            "Shader Variant Pruning",
                            PlatformSettingKind.Boolean,
                            "true",
                            true,
                            [])
                    ])
            ],
            [
                new PlatformGraphicsProfileDefinition(
                    "psvita-forward",
                    "PS Vita Forward",
                    "PS Vita 960x544 forward-rendering bootstrap profile.",
                    [
                        new PlatformSettingDefinition(
                            "default-width",
                            "Default Width",
                            PlatformSettingKind.Text,
                            "960",
                            true,
                            []),
                        new PlatformSettingDefinition(
                            "default-height",
                            "Default Height",
                            PlatformSettingKind.Text,
                            "544",
                            true,
                            []),
                        new PlatformSettingDefinition(
                            "vsync-enabled",
                            "VSync Enabled",
                            PlatformSettingKind.Boolean,
                            "true",
                            true,
                            []),
                        new PlatformSettingDefinition(
                            "fullscreen-enabled",
                            "Fullscreen Enabled",
                            PlatformSettingKind.Boolean,
                            "true",
                            true,
                            [])
                    ])
            ],
            [
                new PlatformAssetRequirementDefinition(
                    "scene",
                    "Scene",
                    true,
                    ["helen"]),
                new PlatformAssetRequirementDefinition(
                    "texture",
                    "Texture",
                    false,
                    ["png", "tga", "jpg"])
            ],
            [
                new PlatformMaterialSchemaDefinition(
                    "standard-shader",
                    "Standard Shader",
                    ["psvita-forward"],
                    [
                        new PlatformMaterialFieldDefinition(
                            "base-color",
                            "Base Color",
                            PlatformMaterialFieldKind.Color,
                            "#ffffff",
                            false,
                            []),
                        new PlatformMaterialFieldDefinition(
                            "texture-id",
                            "Diffuse Texture",
                            PlatformMaterialFieldKind.AssetReference,
                            string.Empty,
                            false,
                            [])
                    ])
            ],
            [
                new PlatformComponentSupportRule(
                    "helengine.meshcomponent",
                    PlatformComponentSupportKind.Transform,
                    "Mesh components are normalized during packaging.",
                    string.Empty),
                new PlatformComponentSupportRule(
                    "helengine.cameracomponent",
                    PlatformComponentSupportKind.Transform,
                    "Camera components are normalized during packaging.",
                    string.Empty),
                new PlatformComponentSupportRule(
                    "helengine.fpscomponent",
                    PlatformComponentSupportKind.Transform,
                    "Font references are rewritten during packaging.",
                    string.Empty),
                new PlatformComponentSupportRule(
                    "helengine.textcomponent",
                    PlatformComponentSupportKind.Transform,
                    "Font references are rewritten during packaging.",
                    string.Empty),
                new PlatformComponentSupportRule(
                    "helengine.directionallightcomponent",
                    PlatformComponentSupportKind.PassThrough,
                    "Directional light payloads can deserialize while the PS Vita renderer comes online.",
                    string.Empty)
            ],
            [
                new PlatformCodegenProfileDefinition(
                    "default",
                    "Default",
                    "PS Vita C# to C++ codegen profile",
                    PlatformCodegenLanguage.Cpp,
                    PlatformSerializationEndianness.LittleEndian,
                    [
                        new PlatformSettingDefinition(
                            "write-conversion-report",
                            "Write Conversion Report",
                            PlatformSettingKind.Boolean,
                            "true",
                            true,
                            []),
                        new PlatformSettingDefinition(
                            "include-project-defined-preprocessor-symbols",
                            "Include Project Symbols",
                            PlatformSettingKind.Boolean,
                            "false",
                            true,
                            []),
                        new PlatformSettingDefinition(
                            "load-native-runtime-metadata",
                            "Load Native Runtime Metadata",
                            PlatformSettingKind.Boolean,
                            "true",
                            true,
                            []),
                        new PlatformSettingDefinition(
                            PlatformCodegenSettingIds.AppContextBaseDirectoryMode,
                            "AppContext Base Directory Mode",
                            PlatformSettingKind.Choice,
                            PlatformCodegenSettingIds.AppContextBaseDirectoryModeStaticDot,
                            true,
                            [
                                PlatformCodegenSettingIds.AppContextBaseDirectoryModeDynamicRuntime,
                                PlatformCodegenSettingIds.AppContextBaseDirectoryModeStaticDot
                            ])
                    ])
            ],
            [
                new PlatformStorageProfileDefinition(
                    "vpk-package",
                    "VPK Package",
                    PlatformStorageProfileKind.LooseFiles,
                    "psvita-vpk-package",
                    allowContainerSegmentation: false)
            ],
            [
                new PlatformMediaProfileDefinition(
                    "psvita-memory-card",
                    "PS Vita Memory Card",
                    PlatformMediaLayoutKind.InstallTree,
                    allowPhysicalDuplication: false,
                    preferLocalityOverDeduplication: true)
            ],
            new RuntimeGenerationContract(
                RuntimeMaterialResolutionMode.CookedPlatformOwned,
                true,
                PackagedPathPolicy.ContentRelativeOnly));
    }
}
