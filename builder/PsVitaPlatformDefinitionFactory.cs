using helengine.baseplatform.Definitions;
using helengine.baseplatform.Profiles;
using helengine.editor;

namespace helengine.psvita.builder;

/// <summary>
/// Creates the typed PS Vita platform metadata exposed to the editor.
/// </summary>
public static class PsVitaPlatformDefinitionFactory {
    /// <summary>
    /// Generic native numeric type remaps required by custom C++ platforms that cannot emit System.Numerics runtime types directly.
    /// </summary>
    const string NativeNumericTypeRemaps = "System.Numerics.Vector2=helengine.float2;System.Numerics.Vector3=helengine.float3;System.Numerics.Vector4=helengine.float4;System.Numerics.Quaternion=helengine.float4";

    /// <summary>
    /// Generic generated-math-convention value that instructs the shared C++ generator to emit native column-vector math helpers.
    /// </summary>
    const string NativeColumnVectorMathConvention = "native-column-vector";

    /// <summary>
    /// Pointer-size contract forwarded to the shared C++ generator for PS Vita-native output.
    /// </summary>
    const string PointerSizeInBytes = "4";

    /// <summary>
    /// Creates the serialized default Vita texture settings contract used when assets do not provide an explicit Vita override.
    /// </summary>
    /// <returns>Serialized default Vita texture settings.</returns>
    static string CreateDefaultSerializedTextureCookSettings() {
        return PsVitaTextureCookSettingsSerializer.Serialize(new TextureAssetProcessorSettings {
            MaxResolution = 512,
            ColorFormat = TextureAssetColorFormat.Rgba32,
            AlphaPrecision = TextureAssetAlphaPrecision.A8
        });
    }

    /// <summary>
    /// Creates the serialized default Vita font-atlas texture settings contract used when fonts do not provide an explicit Vita override.
    /// </summary>
    /// <returns>Serialized default Vita font-atlas texture settings.</returns>
    static string CreateDefaultSerializedFontAtlasTextureCookSettings() {
        return PsVitaTextureCookSettingsSerializer.Serialize(new TextureAssetProcessorSettings {
            MaxResolution = 0,
            ColorFormat = TextureAssetColorFormat.Rgba32,
            AlphaPrecision = TextureAssetAlphaPrecision.A8
        });
    }

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
                            "generated-math-convention",
                            "Generated Math Convention",
                            PlatformSettingKind.Text,
                            NativeColumnVectorMathConvention,
                            true,
                            []),
                        new PlatformSettingDefinition(
                            "pointer-size-bytes",
                            "Pointer Size (Bytes)",
                            PlatformSettingKind.Text,
                            PointerSizeInBytes,
                            true,
                            []),
                        new PlatformSettingDefinition(
                            "type-remaps",
                            "Type Remaps",
                            PlatformSettingKind.Text,
                            NativeNumericTypeRemaps,
                            true,
                            [])
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
                PackagedPathPolicy.ContentRelativeOnly),
            assetCookCapabilities: [
                new PlatformAssetCookCapabilityDefinition(
                    "texture",
                    "runtime-texture",
                    PlatformAssetCookOwnershipKind.BuilderOwned,
                    "psvita-texture",
                    CreateDefaultSerializedTextureCookSettings(),
                    CreateTextureFormatCapabilities()),
                new PlatformAssetCookCapabilityDefinition(
                    "font-atlas-texture",
                    "runtime-texture",
                    PlatformAssetCookOwnershipKind.BuilderOwned,
                    "psvita-font-atlas-texture",
                    CreateDefaultSerializedFontAtlasTextureCookSettings(),
                    CreateTextureFormatCapabilities(),
                    ".hetex")
            ]);
    }

    /// <summary>
    /// Creates the generic texture format capability metadata supported by the Vita texture cooker.
    /// </summary>
    /// <returns>Texture capability metadata for Vita builder-owned texture cook contracts.</returns>
    static PlatformTextureFormatCapabilityDefinition CreateTextureFormatCapabilities() {
        return new PlatformTextureFormatCapabilityDefinition(
            [
                TextureAssetColorFormat.Rgba32.ToString()
            ],
            [
                TextureAssetAlphaPrecision.A8
            ],
            [
                new PlatformTextureFormatCombinationDefinition(TextureAssetColorFormat.Rgba32.ToString(), TextureAssetAlphaPrecision.A8)
            ]);
    }
}
