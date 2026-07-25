using System.Buffers;
using System.Text;
using System.Text.Json;

namespace helengine.psvita.builder;

/// <summary>
/// Serializes Vita compiler jobs into a stable JSON manifest for the fixed device inbox.
/// </summary>
public sealed class PsVitaShaderCompilerJobSerializer {
    /// <summary>
    /// Serializes one compiler job with stable property and stage ordering.
    /// </summary>
    /// <param name="job">Compiler job to write.</param>
    /// <returns>Compact deterministic JSON manifest.</returns>
    public string Serialize(PsVitaShaderCompilerJob job) {
        if (job == null) {
            throw new ArgumentNullException(nameof(job));
        }

        ArrayBufferWriter<byte> buffer = new();
        using Utf8JsonWriter writer = new(buffer, new JsonWriterOptions { Indented = false });
        writer.WriteStartObject();
        writer.WriteNumber("formatVersion", PsVitaShaderCompilerJob.FormatVersion);
        writer.WriteString("jobHash", job.JobHash);
        writer.WriteStartArray("stages");
        for (int index = 0; index < job.Stages.Count; index++) {
            WriteStage(writer, job.Stages[index]);
        }

        writer.WriteEndArray();
        writer.WriteEndObject();
        writer.Flush();
        return Encoding.UTF8.GetString(buffer.WrittenSpan);
    }

    /// <summary>
    /// Writes one compiler stage using the fixed cross-device manifest property order.
    /// </summary>
    /// <param name="writer">Destination JSON writer.</param>
    /// <param name="stage">Validated stage to serialize.</param>
    static void WriteStage(Utf8JsonWriter writer, PsVitaShaderCompilerStageRequest stage) {
        writer.WriteStartObject();
        writer.WriteString("stageId", stage.StageId);
        writer.WriteString("sourcePath", stage.SourcePath);
        writer.WriteString("entryPoint", stage.EntryPoint);
        writer.WriteString("profile", stage.Profile);
        writer.WriteString("optionsSignature", stage.OptionsSignature);
        writer.WriteEndObject();
    }
}
