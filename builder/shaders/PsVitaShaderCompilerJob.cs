using System.Collections.ObjectModel;

namespace helengine.psvita.builder;

/// <summary>
/// Represents one immutable, deterministic request submitted to the Vita shader compiler VPK.
/// </summary>
public sealed class PsVitaShaderCompilerJob {
    /// <summary>
    /// Current version of the host-to-Vita compiler job schema.
    /// </summary>
    public const int FormatVersion = 1;

    readonly string JobHashValue;
    readonly IReadOnlyList<PsVitaShaderCompilerStageRequest> StagesValue;

    /// <summary>
    /// Initializes one validated compiler job.
    /// </summary>
    /// <param name="jobHash">Stable host-side compilation identity.</param>
    /// <param name="stages">Ordered stage requests compiled as part of the job.</param>
    PsVitaShaderCompilerJob(string jobHash, IReadOnlyList<PsVitaShaderCompilerStageRequest> stages) {
        JobHashValue = RequireText(jobHash, nameof(jobHash));
        StagesValue = ValidateStages(stages);
    }

    /// <summary>
    /// Gets the deterministic job identity expected in the corresponding outbox result.
    /// </summary>
    public string JobHash => JobHashValue;

    /// <summary>
    /// Gets stage requests in the source order that must be preserved by serialization.
    /// </summary>
    public IReadOnlyList<PsVitaShaderCompilerStageRequest> Stages => StagesValue;

    /// <summary>
    /// Creates one immutable compiler job after validating its stable identity and ordered stages.
    /// </summary>
    /// <param name="jobHash">Stable host-side compilation identity.</param>
    /// <param name="stages">Ordered stage requests compiled as part of the job.</param>
    /// <returns>Validated compiler job.</returns>
    public static PsVitaShaderCompilerJob Create(string jobHash, IReadOnlyList<PsVitaShaderCompilerStageRequest> stages) {
        return new PsVitaShaderCompilerJob(jobHash, stages);
    }

    /// <summary>
    /// Requires a non-empty job identity.
    /// </summary>
    /// <param name="value">Candidate job identity.</param>
    /// <param name="parameterName">Parameter name used for diagnostics.</param>
    /// <returns>The validated job identity.</returns>
    static string RequireText(string value, string parameterName) {
        if (string.IsNullOrWhiteSpace(value)) {
            throw new ArgumentException("Vita shader compiler job hashes cannot be empty.", parameterName);
        }

        return value;
    }

    /// <summary>
    /// Validates stage identity uniqueness while preserving the supplied source order.
    /// </summary>
    /// <param name="stages">Candidate compiler stage requests.</param>
    /// <returns>Read-only validated stages.</returns>
    static IReadOnlyList<PsVitaShaderCompilerStageRequest> ValidateStages(IReadOnlyList<PsVitaShaderCompilerStageRequest> stages) {
        if (stages == null || stages.Count == 0) {
            throw new ArgumentException("Vita shader compiler jobs require at least one stage.", nameof(stages));
        }

        HashSet<string> stageIds = new(StringComparer.Ordinal);
        PsVitaShaderCompilerStageRequest[] copiedStages = new PsVitaShaderCompilerStageRequest[stages.Count];
        for (int index = 0; index < stages.Count; index++) {
            PsVitaShaderCompilerStageRequest stage = stages[index] ?? throw new ArgumentException("Vita shader compiler jobs cannot contain empty stages.", nameof(stages));
            if (!stageIds.Add(stage.StageId)) {
                throw new ArgumentException($"Vita shader compiler stage id '{stage.StageId}' is duplicated.", nameof(stages));
            }

            copiedStages[index] = stage;
        }

        return new ReadOnlyCollection<PsVitaShaderCompilerStageRequest>(copiedStages);
    }
}
