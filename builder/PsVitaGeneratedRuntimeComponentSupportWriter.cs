using System.Text;
using helengine.editor;

namespace helengine.psvita.builder;

/// <summary>
/// Writes Vita-local fallback runtime component deserializer support into one generated-core workspace when the editor build graph does not emit it.
/// </summary>
public sealed class PsVitaGeneratedRuntimeComponentSupportWriter {
    /// <summary>
    /// Engine-owned component types whose automatic runtime payloads must be supported by the first Vita menu bootstrap.
    /// </summary>
    static readonly Dictionary<string, Type> SupportedEngineComponentTypesById = new(StringComparer.OrdinalIgnoreCase) {
        ["helengine.AnchorComponent"] = typeof(AnchorComponent),
        ["helengine.ClipRectComponent"] = typeof(ClipRectComponent),
        ["helengine.ReferenceCanvasFitComponent"] = typeof(ReferenceCanvasFitComponent),
        ["helengine.RoundedRectComponent"] = typeof(RoundedRectComponent),
        ["helengine.ScrollComponent"] = typeof(ScrollComponent),
        ["helengine.SpriteComponent"] = typeof(SpriteComponent),
        ["helengine.TextComponent"] = typeof(TextComponent),
        ["helengine.ViewportComponent"] = typeof(ViewportComponent)
    };

    /// <summary>
    /// Component type identifiers already covered by generated-core-owned manual runtime deserializers.
    /// </summary>
    static readonly HashSet<string> BuiltInRuntimeDeserializerComponentTypeIds = new(StringComparer.OrdinalIgnoreCase) {
        "helengine.CameraComponent",
        "helengine.FPSComponent",
        "helengine.MeshComponent",
        "helengine.SceneMapComponent"
    };

    /// <summary>
    /// Generated-core sources that should remain excluded from the unity translation unit because the host build compiles them separately.
    /// </summary>
    static readonly HashSet<string> UnityExcludedRelativeSourcePaths = new(StringComparer.OrdinalIgnoreCase) {
        "helengine_core_amalgamated.cpp",
        "helengine_core_unity.cpp",
        "runtime/runtime_code_module_manifest.cpp",
        "runtime/runtime_scene_catalog_manifest.cpp",
        "runtime/runtime_startup_manifest.cpp"
    };

    /// <summary>
    /// Ensures fallback runtime component deserializer support exists for the supplied cooked scenes.
    /// </summary>
    /// <param name="generatedCoreRootPath">Generated-core source root consumed by the Vita native build.</param>
    /// <param name="cookedSceneAssetPaths">Cooked scene assets whose serialized component identifiers should drive fallback generation.</param>
    public void EnsureGeneratedRuntimeSupport(string generatedCoreRootPath, IReadOnlyList<string> cookedSceneAssetPaths) {
        if (string.IsNullOrWhiteSpace(generatedCoreRootPath)) {
            throw new ArgumentException("Generated core root path must be provided.", nameof(generatedCoreRootPath));
        } else if (cookedSceneAssetPaths == null) {
            throw new ArgumentNullException(nameof(cookedSceneAssetPaths));
        }

        string registrationSourcePath = Path.Combine(generatedCoreRootPath, "GeneratedRuntimeComponentDeserializerRegistration.cpp");
        if (File.Exists(registrationSourcePath)) {
            return;
        }

        IReadOnlyList<string> componentTypeIds = CollectComponentTypeIds(cookedSceneAssetPaths);
        IReadOnlyList<Type> engineComponentTypes = ResolveRequiredEngineComponentTypes(componentTypeIds);
        IReadOnlyList<string> unsupportedScriptComponentTypeIds = ResolveUnsupportedScriptComponentTypeIds(componentTypeIds);
        if (engineComponentTypes.Count == 0 && unsupportedScriptComponentTypeIds.Count == 0) {
            return;
        }

        Directory.CreateDirectory(generatedCoreRootPath);

        ScriptComponentPlayerDeserializerGenerator generator = new();
        ScriptComponentReflectionSchemaBuilder schemaBuilder = new();
        List<string> generatedEngineDeserializerClassNames = WriteGeneratedEngineDeserializers(
            generatedCoreRootPath,
            engineComponentTypes,
            schemaBuilder,
            generator);

        if (unsupportedScriptComponentTypeIds.Count > 0) {
            WriteUnsupportedRuntimePlaceholderSupportFiles(generatedCoreRootPath);
        }

        WriteRegistrationFiles(
            generatedCoreRootPath,
            generatedEngineDeserializerClassNames,
            unsupportedScriptComponentTypeIds);
        RewriteUnityTranslationUnit(generatedCoreRootPath);
    }

    /// <summary>
    /// Collects the distinct serialized component type identifiers referenced by the supplied cooked scenes.
    /// </summary>
    /// <param name="cookedSceneAssetPaths">Cooked scene assets that should be inspected.</param>
    /// <returns>Distinct serialized component type identifiers ordered for stable generation.</returns>
    IReadOnlyList<string> CollectComponentTypeIds(IReadOnlyList<string> cookedSceneAssetPaths) {
        SortedSet<string> componentTypeIds = new(StringComparer.OrdinalIgnoreCase);
        for (int sceneIndex = 0; sceneIndex < cookedSceneAssetPaths.Count; sceneIndex++) {
            string cookedSceneAssetPath = cookedSceneAssetPaths[sceneIndex];
            if (string.IsNullOrWhiteSpace(cookedSceneAssetPath) || !File.Exists(cookedSceneAssetPath)) {
                continue;
            }

            using FileStream stream = File.OpenRead(cookedSceneAssetPath);
            SceneAsset sceneAsset = global::helengine.files.AssetSerializer.Deserialize(stream) as SceneAsset;
            if (sceneAsset == null) {
                continue;
            }

            CollectComponentTypeIdsFromEntities(sceneAsset.RootEntities ?? Array.Empty<SceneEntityAsset>(), componentTypeIds);
        }

        return [.. componentTypeIds];
    }

    /// <summary>
    /// Collects serialized component type identifiers recursively from one scene entity tree.
    /// </summary>
    /// <param name="entities">Scene entities under inspection.</param>
    /// <param name="componentTypeIds">Set that receives discovered serialized component type identifiers.</param>
    void CollectComponentTypeIdsFromEntities(IReadOnlyList<SceneEntityAsset> entities, ISet<string> componentTypeIds) {
        if (entities == null) {
            throw new ArgumentNullException(nameof(entities));
        } else if (componentTypeIds == null) {
            throw new ArgumentNullException(nameof(componentTypeIds));
        }

        for (int entityIndex = 0; entityIndex < entities.Count; entityIndex++) {
            SceneEntityAsset entity = entities[entityIndex];
            if (entity == null) {
                continue;
            }

            SceneComponentAssetRecord[] components = entity.Components ?? Array.Empty<SceneComponentAssetRecord>();
            for (int componentIndex = 0; componentIndex < components.Length; componentIndex++) {
                SceneComponentAssetRecord component = components[componentIndex];
                if (component != null && !string.IsNullOrWhiteSpace(component.ComponentTypeId)) {
                    componentTypeIds.Add(component.ComponentTypeId);
                }
            }

            CollectComponentTypeIdsFromEntities(entity.Children ?? Array.Empty<SceneEntityAsset>(), componentTypeIds);
        }
    }

    /// <summary>
    /// Resolves the engine-owned automatic component types that require Vita-local runtime deserializer generation.
    /// </summary>
    /// <param name="componentTypeIds">Serialized component identifiers referenced by the cooked scenes.</param>
    /// <returns>Distinct engine-owned runtime component types that should emit native deserializers.</returns>
    IReadOnlyList<Type> ResolveRequiredEngineComponentTypes(IReadOnlyList<string> componentTypeIds) {
        if (componentTypeIds == null) {
            throw new ArgumentNullException(nameof(componentTypeIds));
        }

        List<Type> componentTypes = [];
        HashSet<Type> seenTypes = [];
        for (int componentIndex = 0; componentIndex < componentTypeIds.Count; componentIndex++) {
            string componentTypeId = NormalizeLegacyEngineComponentTypeId(componentTypeIds[componentIndex]);
            if (string.IsNullOrWhiteSpace(componentTypeId) || BuiltInRuntimeDeserializerComponentTypeIds.Contains(componentTypeId)) {
                continue;
            }

            if (SupportedEngineComponentTypesById.TryGetValue(componentTypeId, out Type componentType) && seenTypes.Add(componentType)) {
                componentTypes.Add(componentType);
            }
        }

        componentTypes.Sort((left, right) => string.Compare(left.FullName, right.FullName, StringComparison.Ordinal));
        return componentTypes;
    }

    /// <summary>
    /// Resolves the scripted component type identifiers that should fall back to Vita-local placeholder components.
    /// </summary>
    /// <param name="componentTypeIds">Serialized component identifiers referenced by the cooked scenes.</param>
    /// <returns>Distinct scripted component type identifiers that should register placeholder deserializers.</returns>
    IReadOnlyList<string> ResolveUnsupportedScriptComponentTypeIds(IReadOnlyList<string> componentTypeIds) {
        if (componentTypeIds == null) {
            throw new ArgumentNullException(nameof(componentTypeIds));
        }

        SortedSet<string> unsupportedComponentTypeIds = new(StringComparer.OrdinalIgnoreCase);
        for (int componentIndex = 0; componentIndex < componentTypeIds.Count; componentIndex++) {
            string componentTypeId = componentTypeIds[componentIndex];
            if (string.IsNullOrWhiteSpace(componentTypeId) || !componentTypeId.Contains(',', StringComparison.Ordinal)) {
                continue;
            }

            if (componentTypeId.StartsWith("helengine.", StringComparison.OrdinalIgnoreCase)) {
                continue;
            }

            unsupportedComponentTypeIds.Add(componentTypeId);
        }

        return [.. unsupportedComponentTypeIds];
    }

    /// <summary>
    /// Writes generated native runtime deserializers for the supplied engine-owned automatic component types.
    /// </summary>
    /// <param name="generatedCoreRootPath">Generated-core source root that will receive the emitted files.</param>
    /// <param name="componentTypes">Engine-owned automatic component types that should emit native deserializers.</param>
    /// <param name="schemaBuilder">Shared reflected schema builder used to inspect component members.</param>
    /// <param name="generator">Shared native deserializer generator.</param>
    /// <returns>Stable generated runtime deserializer class names registered by the fallback writer.</returns>
    List<string> WriteGeneratedEngineDeserializers(
        string generatedCoreRootPath,
        IReadOnlyList<Type> componentTypes,
        ScriptComponentReflectionSchemaBuilder schemaBuilder,
        ScriptComponentPlayerDeserializerGenerator generator) {
        if (string.IsNullOrWhiteSpace(generatedCoreRootPath)) {
            throw new ArgumentException("Generated core root path must be provided.", nameof(generatedCoreRootPath));
        } else if (componentTypes == null) {
            throw new ArgumentNullException(nameof(componentTypes));
        } else if (schemaBuilder == null) {
            throw new ArgumentNullException(nameof(schemaBuilder));
        } else if (generator == null) {
            throw new ArgumentNullException(nameof(generator));
        }

        List<string> generatedClassNames = [];
        for (int componentIndex = 0; componentIndex < componentTypes.Count; componentIndex++) {
            Type componentType = componentTypes[componentIndex];
            ScriptComponentReflectionSchema schema = schemaBuilder.Build(componentType);
            if (!generator.CanGenerateNativeDeserializer(schema)) {
                throw new InvalidOperationException($"PS Vita fallback generation does not support automatic component type '{componentType.FullName}'.");
            }

            string className = generator.BuildNativeDeserializerClassName(schema);
            File.WriteAllText(Path.Combine(generatedCoreRootPath, className + ".hpp"), generator.GenerateNativeDeserializerHeader(schema), Encoding.UTF8);
            File.WriteAllText(Path.Combine(generatedCoreRootPath, className + ".cpp"), generator.GenerateNativeDeserializerSource(schema), Encoding.UTF8);
            generatedClassNames.Add(className);
        }

        return generatedClassNames;
    }

    /// <summary>
    /// Writes the placeholder component and placeholder deserializer sources used for unsupported scripted gameplay components.
    /// </summary>
    /// <param name="generatedCoreRootPath">Generated-core source root that will receive the emitted files.</param>
    void WriteUnsupportedRuntimePlaceholderSupportFiles(string generatedCoreRootPath) {
        if (string.IsNullOrWhiteSpace(generatedCoreRootPath)) {
            throw new ArgumentException("Generated core root path must be provided.", nameof(generatedCoreRootPath));
        }

        File.WriteAllText(Path.Combine(generatedCoreRootPath, "PsVitaUnsupportedRuntimeComponent.hpp"), BuildUnsupportedRuntimeComponentHeader(), Encoding.UTF8);
        File.WriteAllText(Path.Combine(generatedCoreRootPath, "PsVitaUnsupportedRuntimeComponent.cpp"), BuildUnsupportedRuntimeComponentSource(), Encoding.UTF8);
        File.WriteAllText(Path.Combine(generatedCoreRootPath, "PsVitaUnsupportedRuntimeComponentDeserializer.hpp"), BuildUnsupportedRuntimeComponentDeserializerHeader(), Encoding.UTF8);
        File.WriteAllText(Path.Combine(generatedCoreRootPath, "PsVitaUnsupportedRuntimeComponentDeserializer.cpp"), BuildUnsupportedRuntimeComponentDeserializerSource(), Encoding.UTF8);
    }

    /// <summary>
    /// Writes the generated runtime component registration header and source consumed by the generated runtime registry.
    /// </summary>
    /// <param name="generatedCoreRootPath">Generated-core source root that will receive the emitted registration files.</param>
    /// <param name="engineDeserializerClassNames">Generated engine-owned native deserializer class names that should register first.</param>
    /// <param name="unsupportedScriptComponentTypeIds">Unsupported scripted component identifiers that should register placeholder deserializers.</param>
    void WriteRegistrationFiles(
        string generatedCoreRootPath,
        IReadOnlyList<string> engineDeserializerClassNames,
        IReadOnlyList<string> unsupportedScriptComponentTypeIds) {
        if (string.IsNullOrWhiteSpace(generatedCoreRootPath)) {
            throw new ArgumentException("Generated core root path must be provided.", nameof(generatedCoreRootPath));
        } else if (engineDeserializerClassNames == null) {
            throw new ArgumentNullException(nameof(engineDeserializerClassNames));
        } else if (unsupportedScriptComponentTypeIds == null) {
            throw new ArgumentNullException(nameof(unsupportedScriptComponentTypeIds));
        }

        File.WriteAllText(Path.Combine(generatedCoreRootPath, "GeneratedRuntimeComponentDeserializerRegistration.hpp"), BuildRegistrationHeader(), Encoding.UTF8);
        File.WriteAllText(
            Path.Combine(generatedCoreRootPath, "GeneratedRuntimeComponentDeserializerRegistration.cpp"),
            BuildRegistrationSource(engineDeserializerClassNames, unsupportedScriptComponentTypeIds),
            Encoding.UTF8);
    }

    /// <summary>
    /// Rewrites the generated-core unity translation unit so the Vita fallback sources participate in the native build.
    /// </summary>
    /// <param name="generatedCoreRootPath">Generated-core source root whose unity translation unit should be rewritten.</param>
    void RewriteUnityTranslationUnit(string generatedCoreRootPath) {
        if (string.IsNullOrWhiteSpace(generatedCoreRootPath)) {
            throw new ArgumentException("Generated core root path must be provided.", nameof(generatedCoreRootPath));
        }

        string[] sourcePaths = Directory.GetFiles(generatedCoreRootPath, "*.cpp", SearchOption.AllDirectories);
        List<string> relativeSourcePaths = [];
        for (int sourceIndex = 0; sourceIndex < sourcePaths.Length; sourceIndex++) {
            string relativeSourcePath = Path.GetRelativePath(generatedCoreRootPath, sourcePaths[sourceIndex]).Replace('\\', '/');
            if (!UnityExcludedRelativeSourcePaths.Contains(relativeSourcePath)) {
                relativeSourcePaths.Add(relativeSourcePath);
            }
        }

        relativeSourcePaths.Sort(StringComparer.OrdinalIgnoreCase);
        StringBuilder builder = new();
        builder.AppendLine("#ifdef DrawText");
        builder.AppendLine("#undef DrawText");
        builder.AppendLine("#endif");
        for (int sourceIndex = 0; sourceIndex < relativeSourcePaths.Count; sourceIndex++) {
            builder.AppendLine("#include \"" + relativeSourcePaths[sourceIndex] + "\"");
        }

        File.WriteAllText(Path.Combine(generatedCoreRootPath, "helengine_core_unity.cpp"), builder.ToString(), Encoding.UTF8);
    }

    /// <summary>
    /// Normalizes one legacy assembly-qualified engine component identifier to the short runtime form used by generated-core component classes.
    /// </summary>
    /// <param name="componentTypeId">Serialized component identifier under evaluation.</param>
    /// <returns>Short engine component identifier when the supplied identifier uses the legacy engine assembly-qualified form; otherwise the original identifier.</returns>
    static string NormalizeLegacyEngineComponentTypeId(string componentTypeId) {
        if (string.IsNullOrWhiteSpace(componentTypeId)) {
            return string.Empty;
        }

        return componentTypeId switch {
            "helengine.AnchorComponent, helengine.core" => "helengine.AnchorComponent",
            "helengine.ClipRectComponent, helengine.core" => "helengine.ClipRectComponent",
            "helengine.ReferenceCanvasFitComponent, helengine.core" => "helengine.ReferenceCanvasFitComponent",
            "helengine.RoundedRectComponent, helengine.core" => "helengine.RoundedRectComponent",
            "helengine.ScrollComponent, helengine.core" => "helengine.ScrollComponent",
            "helengine.SpriteComponent, helengine.core" => "helengine.SpriteComponent",
            "helengine.TextComponent, helengine.core" => "helengine.TextComponent",
            "helengine.ViewportComponent, helengine.core" => "helengine.ViewportComponent",
            _ => componentTypeId
        };
    }

    /// <summary>
    /// Builds the generated runtime component registration header text.
    /// </summary>
    /// <returns>Generated registration header source.</returns>
    static string BuildRegistrationHeader() {
        return "#pragma once\n"
            + "#ifdef DrawText\n"
            + "#undef DrawText\n"
            + "#endif\n"
            + "class RuntimeComponentRegistry;\n\n"
            + "void RegisterGeneratedRuntimeComponentDeserializers(::RuntimeComponentRegistry* registry);\n";
    }

    /// <summary>
    /// Builds the generated runtime component registration source text.
    /// </summary>
    /// <param name="engineDeserializerClassNames">Generated engine-owned runtime deserializer class names that should register.</param>
    /// <param name="unsupportedScriptComponentTypeIds">Unsupported scripted component identifiers that should register placeholder deserializers.</param>
    /// <returns>Generated registration source.</returns>
    static string BuildRegistrationSource(
        IReadOnlyList<string> engineDeserializerClassNames,
        IReadOnlyList<string> unsupportedScriptComponentTypeIds) {
        StringBuilder builder = new();
        builder.AppendLine("#ifdef DrawText");
        builder.AppendLine("#undef DrawText");
        builder.AppendLine("#endif");
        builder.AppendLine("#include \"GeneratedRuntimeComponentDeserializerRegistration.hpp\"");
        builder.AppendLine("#include \"RuntimeComponentRegistry.hpp\"");
        builder.AppendLine("#include \"runtime/native_exceptions.hpp\"");
        for (int classIndex = 0; classIndex < engineDeserializerClassNames.Count; classIndex++) {
            builder.AppendLine("#include \"" + engineDeserializerClassNames[classIndex] + ".hpp\"");
        }
        if (unsupportedScriptComponentTypeIds.Count > 0) {
            builder.AppendLine("#include \"PsVitaUnsupportedRuntimeComponentDeserializer.hpp\"");
        }

        builder.AppendLine();
        builder.AppendLine("void RegisterGeneratedRuntimeComponentDeserializers(::RuntimeComponentRegistry* registry)");
        builder.AppendLine("{");
        builder.AppendLine("    if (registry == nullptr)");
        builder.AppendLine("    {");
        builder.AppendLine("throw new ArgumentNullException(\"registry\");");
        builder.AppendLine("    }");
        for (int classIndex = 0; classIndex < engineDeserializerClassNames.Count; classIndex++) {
            builder.AppendLine("registry->Register(new ::" + engineDeserializerClassNames[classIndex] + "());");
        }
        for (int componentIndex = 0; componentIndex < unsupportedScriptComponentTypeIds.Count; componentIndex++) {
            builder.AppendLine("registry->Register(new ::PsVitaUnsupportedRuntimeComponentDeserializer(\"" + EscapeForCppString(unsupportedScriptComponentTypeIds[componentIndex]) + "\"));");
        }
        builder.AppendLine("}");
        return builder.ToString();
    }

    /// <summary>
    /// Builds the placeholder unsupported runtime component header text.
    /// </summary>
    /// <returns>Generated placeholder component header source.</returns>
    static string BuildUnsupportedRuntimeComponentHeader() {
        return "#pragma once\n"
            + "#ifdef DrawText\n"
            + "#undef DrawText\n"
            + "#endif\n"
            + "#include \"runtime/native_string.hpp\"\n"
            + "#include \"Component.hpp\"\n\n"
            + "class PsVitaUnsupportedRuntimeComponent : public Component\n"
            + "{\n"
            + "public:\n"
            + "    virtual ~PsVitaUnsupportedRuntimeComponent() = default;\n\n"
            + "    PsVitaUnsupportedRuntimeComponent(std::string serializedComponentTypeId);\n\n"
            + "    const std::string& get_SerializedComponentTypeId();\n\n"
            + "private:\n"
            + "    std::string SerializedComponentTypeId;\n"
            + "};\n";
    }

    /// <summary>
    /// Builds the placeholder unsupported runtime component source text.
    /// </summary>
    /// <returns>Generated placeholder component source.</returns>
    static string BuildUnsupportedRuntimeComponentSource() {
        return "#ifdef DrawText\n"
            + "#undef DrawText\n"
            + "#endif\n"
            + "#include \"PsVitaUnsupportedRuntimeComponent.hpp\"\n"
            + "#include \"runtime/native_exceptions.hpp\"\n\n"
            + "PsVitaUnsupportedRuntimeComponent::PsVitaUnsupportedRuntimeComponent(std::string serializedComponentTypeId) : SerializedComponentTypeId()\n"
            + "{\n"
            + "    if (String::IsNullOrWhiteSpace(serializedComponentTypeId))\n"
            + "    {\n"
            + "throw new ArgumentException(\"Serialized component type id is required.\", \"serializedComponentTypeId\");\n"
            + "    }\n"
            + "this->SerializedComponentTypeId = serializedComponentTypeId;\n"
            + "}\n\n"
            + "const std::string& PsVitaUnsupportedRuntimeComponent::get_SerializedComponentTypeId()\n"
            + "{\n"
            + "return this->SerializedComponentTypeId;\n"
            + "}\n";
    }

    /// <summary>
    /// Builds the placeholder unsupported runtime component deserializer header text.
    /// </summary>
    /// <returns>Generated placeholder deserializer header source.</returns>
    static string BuildUnsupportedRuntimeComponentDeserializerHeader() {
        return "#pragma once\n"
            + "#ifdef DrawText\n"
            + "#undef DrawText\n"
            + "#endif\n"
            + "#include \"IRuntimeComponentDeserializer.hpp\"\n"
            + "#include \"runtime/native_string.hpp\"\n"
            + "#include \"Component.hpp\"\n"
            + "#include \"SceneComponentAssetRecord.hpp\"\n"
            + "#include \"RuntimeSceneAssetReferenceResolver.hpp\"\n\n"
            + "class PsVitaUnsupportedRuntimeComponentDeserializer : public IRuntimeComponentDeserializer\n"
            + "{\n"
            + "public:\n"
            + "    virtual ~PsVitaUnsupportedRuntimeComponentDeserializer() = default;\n\n"
            + "    PsVitaUnsupportedRuntimeComponentDeserializer(std::string componentTypeId);\n\n"
            + "    const std::string& get_ComponentTypeId();\n\n"
            + "    ::Component* Deserialize(::SceneComponentAssetRecord* record, ::RuntimeSceneAssetReferenceResolver* referenceResolver);\n\n"
            + "private:\n"
            + "    std::string ComponentTypeId;\n"
            + "};\n";
    }

    /// <summary>
    /// Builds the placeholder unsupported runtime component deserializer source text.
    /// </summary>
    /// <returns>Generated placeholder deserializer source.</returns>
    static string BuildUnsupportedRuntimeComponentDeserializerSource() {
        return "#ifdef DrawText\n"
            + "#undef DrawText\n"
            + "#endif\n"
            + "#include \"PsVitaUnsupportedRuntimeComponentDeserializer.hpp\"\n"
            + "#include \"PsVitaUnsupportedRuntimeComponent.hpp\"\n"
            + "#include \"runtime/native_exceptions.hpp\"\n\n"
            + "PsVitaUnsupportedRuntimeComponentDeserializer::PsVitaUnsupportedRuntimeComponentDeserializer(std::string componentTypeId) : ComponentTypeId()\n"
            + "{\n"
            + "    if (String::IsNullOrWhiteSpace(componentTypeId))\n"
            + "    {\n"
            + "throw new ArgumentException(\"Component type id is required.\", \"componentTypeId\");\n"
            + "    }\n"
            + "this->ComponentTypeId = componentTypeId;\n"
            + "}\n\n"
            + "const std::string& PsVitaUnsupportedRuntimeComponentDeserializer::get_ComponentTypeId()\n"
            + "{\n"
            + "return this->ComponentTypeId;\n"
            + "}\n\n"
            + "::Component* PsVitaUnsupportedRuntimeComponentDeserializer::Deserialize(::SceneComponentAssetRecord* record, ::RuntimeSceneAssetReferenceResolver* referenceResolver)\n"
            + "{\n"
            + "    if (record == nullptr)\n"
            + "    {\n"
            + "throw new ArgumentNullException(\"record\");\n"
            + "    }\n"
            + "    if (referenceResolver == nullptr)\n"
            + "    {\n"
            + "throw new ArgumentNullException(\"referenceResolver\");\n"
            + "    }\n"
            + "    if (!String::Equals(record->get_ComponentTypeId(), this->ComponentTypeId, StringComparison::OrdinalIgnoreCase))\n"
            + "    {\n"
            + "throw new InvalidOperationException(std::string(\"Unsupported runtime placeholder deserializer cannot deserialize '\") + record->get_ComponentTypeId() + std::string(\"'.\"));\n"
            + "    }\n"
            + "return new ::PsVitaUnsupportedRuntimeComponent(this->ComponentTypeId);\n"
            + "}\n";
    }

    /// <summary>
    /// Escapes one string literal for inclusion inside generated C++ source.
    /// </summary>
    /// <param name="value">String literal text that should be escaped.</param>
    /// <returns>Escaped C++ string literal content.</returns>
    static string EscapeForCppString(string value) {
        if (value == null) {
            return string.Empty;
        }

        return value
            .Replace("\\", "\\\\", StringComparison.Ordinal)
            .Replace("\"", "\\\"", StringComparison.Ordinal)
            .Replace("\r", "\\r", StringComparison.Ordinal)
            .Replace("\n", "\\n", StringComparison.Ordinal);
    }
}
