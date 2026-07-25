using System.Text.Json;

namespace helengine.psvita.builder;

/// <summary>
/// Strictly parses Vita compiler outbox manifests before the host accepts any device-produced artifact.
/// </summary>
public sealed class PsVitaShaderCompilerResultSerializer {
    /// <summary>
    /// Deserializes and validates one device compiler result manifest.
    /// </summary>
    /// <param name="json">Result manifest text read from the fixed outbox location.</param>
    /// <returns>Validated compiler result.</returns>
    public PsVitaShaderCompilerResult Deserialize(string json) {
        if (string.IsNullOrWhiteSpace(json)) {
            throw new ArgumentException("Vita shader compiler result JSON cannot be empty.", nameof(json));
        }

        using JsonDocument document = JsonDocument.Parse(json);
        JsonElement root = document.RootElement;
        if (root.ValueKind != JsonValueKind.Object) {
            throw new InvalidOperationException("Vita shader compiler result JSON must contain an object.");
        }
        if (GetRequiredInt32(root, "formatVersion") != PsVitaShaderCompilerResult.FormatVersion) {
            throw new InvalidOperationException("Vita shader compiler result format version is unsupported.");
        }

        JsonElement stagesElement = GetRequiredArray(root, "stages");
        PsVitaShaderCompilerStageResult[] stages = new PsVitaShaderCompilerStageResult[stagesElement.GetArrayLength()];
        for (int index = 0; index < stages.Length; index++) {
            stages[index] = ReadStage(stagesElement[index]);
        }

        return new PsVitaShaderCompilerResult(GetRequiredString(root, "jobHash"), stages);
    }

    /// <summary>
    /// Parses one required stage result object from the outbox manifest.
    /// </summary>
    /// <param name="element">JSON element representing one stage result.</param>
    /// <returns>Validated stage result.</returns>
    static PsVitaShaderCompilerStageResult ReadStage(JsonElement element) {
        if (element.ValueKind != JsonValueKind.Object) {
            throw new InvalidOperationException("Vita shader compiler result stages must be objects.");
        }

        try {
            return new PsVitaShaderCompilerStageResult(
                GetRequiredString(element, "stageId"),
                GetRequiredBoolean(element, "success"),
                GetRequiredString(element, "diagnostic"),
                GetRequiredString(element, "artifactPath"),
                GetRequiredString(element, "artifactHash"),
                GetRequiredInt32(element, "programByteCount"));
        } catch (ArgumentException exception) {
            throw new InvalidOperationException("Vita shader compiler result stage metadata is invalid.", exception);
        }
    }

    /// <summary>
    /// Reads one required string property from an object.
    /// </summary>
    /// <param name="element">Source JSON object.</param>
    /// <param name="propertyName">Required property name.</param>
    /// <returns>The property string value.</returns>
    static string GetRequiredString(JsonElement element, string propertyName) {
        JsonElement property = GetRequiredProperty(element, propertyName);
        if (property.ValueKind != JsonValueKind.String) {
            throw new InvalidOperationException($"Vita shader compiler result property '{propertyName}' must be a string.");
        }

        return property.GetString() ?? throw new InvalidOperationException($"Vita shader compiler result property '{propertyName}' cannot be null.");
    }

    /// <summary>
    /// Reads one required Boolean property from an object.
    /// </summary>
    /// <param name="element">Source JSON object.</param>
    /// <param name="propertyName">Required property name.</param>
    /// <returns>The property Boolean value.</returns>
    static bool GetRequiredBoolean(JsonElement element, string propertyName) {
        JsonElement property = GetRequiredProperty(element, propertyName);
        if (property.ValueKind != JsonValueKind.True && property.ValueKind != JsonValueKind.False) {
            throw new InvalidOperationException($"Vita shader compiler result property '{propertyName}' must be Boolean.");
        }

        return property.GetBoolean();
    }

    /// <summary>
    /// Reads one required Int32 property from an object.
    /// </summary>
    /// <param name="element">Source JSON object.</param>
    /// <param name="propertyName">Required property name.</param>
    /// <returns>The property Int32 value.</returns>
    static int GetRequiredInt32(JsonElement element, string propertyName) {
        JsonElement property = GetRequiredProperty(element, propertyName);
        if (property.ValueKind != JsonValueKind.Number || !property.TryGetInt32(out int value)) {
            throw new InvalidOperationException($"Vita shader compiler result property '{propertyName}' must be an Int32.");
        }

        return value;
    }

    /// <summary>
    /// Reads one required array property from an object.
    /// </summary>
    /// <param name="element">Source JSON object.</param>
    /// <param name="propertyName">Required property name.</param>
    /// <returns>The property array value.</returns>
    static JsonElement GetRequiredArray(JsonElement element, string propertyName) {
        JsonElement property = GetRequiredProperty(element, propertyName);
        if (property.ValueKind != JsonValueKind.Array) {
            throw new InvalidOperationException($"Vita shader compiler result property '{propertyName}' must be an array.");
        }

        return property;
    }

    /// <summary>
    /// Reads one required property from an object while preserving a field-specific diagnostic.
    /// </summary>
    /// <param name="element">Source JSON object.</param>
    /// <param name="propertyName">Required property name.</param>
    /// <returns>The requested JSON property.</returns>
    static JsonElement GetRequiredProperty(JsonElement element, string propertyName) {
        if (!element.TryGetProperty(propertyName, out JsonElement property)) {
            throw new InvalidOperationException($"Vita shader compiler result is missing required property '{propertyName}'.");
        }

        return property;
    }
}
