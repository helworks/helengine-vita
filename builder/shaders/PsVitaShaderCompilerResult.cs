using System.Collections.ObjectModel;

namespace helengine.psvita.builder;

/// <summary>
/// Represents one validated outbox manifest returned by the Vita compiler VPK.
/// </summary>
public sealed class PsVitaShaderCompilerResult {
    /// <summary>
    /// Current version of the Vita-to-host compiler result schema.
    /// </summary>
    public const int FormatVersion = 1;

    readonly string JobHashValue;
    readonly IReadOnlyList<PsVitaShaderCompilerStageResult> StagesValue;

    /// <summary>
    /// Initializes one validated compiler result manifest.
    /// </summary>
    /// <param name="jobHash">Stable job identity copied from the submitted inbox manifest.</param>
    /// <param name="stages">Results for every completed stage.</param>
    public PsVitaShaderCompilerResult(string jobHash, IReadOnlyList<PsVitaShaderCompilerStageResult> stages) {
        if (string.IsNullOrWhiteSpace(jobHash)) {
            throw new ArgumentException("Vita shader compiler result job hashes cannot be empty.", nameof(jobHash));
        }

        JobHashValue = jobHash;
        StagesValue = ValidateStages(stages);
    }

    /// <summary>
    /// Gets the compiler job identity that this result answers.
    /// </summary>
    public string JobHash => JobHashValue;

    /// <summary>
    /// Gets validated per-stage results in the device-provided order.
    /// </summary>
    public IReadOnlyList<PsVitaShaderCompilerStageResult> Stages => StagesValue;

    /// <summary>
    /// Validates result identity uniqueness while preserving device result ordering.
    /// </summary>
    /// <param name="stages">Candidate stage results.</param>
    /// <returns>Read-only validated stage results.</returns>
    static IReadOnlyList<PsVitaShaderCompilerStageResult> ValidateStages(IReadOnlyList<PsVitaShaderCompilerStageResult> stages) {
        if (stages == null || stages.Count == 0) {
            throw new ArgumentException("Vita shader compiler results require at least one stage.", nameof(stages));
        }

        HashSet<string> stageIds = new(StringComparer.Ordinal);
        PsVitaShaderCompilerStageResult[] copiedStages = new PsVitaShaderCompilerStageResult[stages.Count];
        for (int index = 0; index < stages.Count; index++) {
            PsVitaShaderCompilerStageResult stage = stages[index] ?? throw new ArgumentException("Vita shader compiler results cannot contain empty stages.", nameof(stages));
            if (!stageIds.Add(stage.StageId)) {
                throw new ArgumentException($"Vita shader compiler result stage id '{stage.StageId}' is duplicated.", nameof(stages));
            }

            copiedStages[index] = stage;
        }

        return new ReadOnlyCollection<PsVitaShaderCompilerStageResult>(copiedStages);
    }
}
