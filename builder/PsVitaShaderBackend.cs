namespace helengine.psvita.builder;

/// <summary>
/// Exposes PS Vita shader compilation through the shared Helengine backend contract.
/// </summary>
public sealed class PsVitaShaderBackend : IShaderBackend {
    /// <summary>
    /// Default program variant name used when the request did not provide one.
    /// </summary>
    const string DefaultVariantName = "default";

    /// <summary>
    /// Stores the capability metadata exposed by the PS Vita backend.
    /// </summary>
    readonly ShaderBackendCapabilities CapabilitiesValue;

    /// <summary>
    /// Compiler wrapper that produces the PS Vita shader payload carried through the shared pipeline.
    /// </summary>
    readonly PsVitaShaderCompiler Compiler;

    /// <summary>
    /// Initializes one PS Vita shader backend.
    /// </summary>
    public PsVitaShaderBackend()
        : this(new PsVitaShaderCompiler()) {
    }

    /// <summary>
    /// Initializes one PS Vita shader backend with an explicit compiler wrapper.
    /// </summary>
    /// <param name="compiler">Compiler wrapper used to produce PS Vita shader payloads.</param>
    internal PsVitaShaderBackend(PsVitaShaderCompiler compiler) {
        Compiler = compiler ?? throw new ArgumentNullException(nameof(compiler));
        CapabilitiesValue = new ShaderBackendCapabilities(
            new ShaderModel(4, 0),
            new ShaderModel(4, 0),
            [
                ShaderStage.Vertex,
                ShaderStage.Pixel
            ],
            false);
    }

    /// <summary>
    /// Gets the backend target emitted by this shader backend.
    /// </summary>
    public ShaderCompileTarget Target {
        get {
            return ShaderCompileTarget.PsVita;
        }
    }

    /// <summary>
    /// Gets the capability metadata exposed by this shader backend.
    /// </summary>
    public ShaderBackendCapabilities Capabilities {
        get {
            return CapabilitiesValue;
        }
    }

    /// <summary>
    /// Compiles one shared shader request into the PS Vita shader payload carried by the shared editor pipeline.
    /// </summary>
    /// <param name="request">Shared shader compile request.</param>
    /// <param name="includeResolver">Resolver used for shader includes.</param>
    /// <returns>Compile result describing the compiled PS Vita shader payload.</returns>
    public ShaderCompileResult Compile(ShaderCompileRequest request, IShaderIncludeResolver includeResolver) {
        if (request == null) {
            throw new ArgumentNullException(nameof(request));
        } else if (includeResolver == null) {
            throw new ArgumentNullException(nameof(includeResolver));
        } else if (request.Target != ShaderCompileTarget.PsVita) {
            throw new InvalidOperationException("PsVitaShaderBackend only supports PS Vita shader requests.");
        }

        ValidateRequest(request);
        PsVitaCompiledShader compiledShader = Compiler.Compile(request);
        ShaderProgramDefinition programDefinition = BuildProgramDefinition(request);
        ShaderCompiledBinary binary = new ShaderCompiledBinary(
            request.Target,
            request.Stage,
            request.EntryPoint,
            request.Variant,
            compiledShader.Bytecode);
        return new ShaderCompileResult(
            request,
            programDefinition,
            binary,
            Array.Empty<ShaderCompileDiagnostic>(),
            true);
    }

    /// <summary>
    /// Builds one shared program definition from the HLSL source metadata carried by the compile request.
    /// </summary>
    /// <param name="request">Shared shader compile request.</param>
    /// <returns>Program definition describing the compiled shader stage.</returns>
    ShaderProgramDefinition BuildProgramDefinition(ShaderCompileRequest request) {
        ShaderBinding[] bindings = HlslShaderBindingParser.ParseBindings(
            request.Source.Source,
            request.Options.BindingPolicy,
            request.Defines);
        ShaderVariant[] variants = BuildVariants(request);
        return new ShaderProgramDefinition(
            request.ProgramName,
            request.Stage,
            request.EntryPoint,
            bindings,
            Array.Empty<ShaderVertexElement>(),
            Array.Empty<ShaderVertexElement>(),
            variants);
    }

    /// <summary>
    /// Builds the program variant metadata associated with the compile request.
    /// </summary>
    /// <param name="request">Shared shader compile request.</param>
    /// <returns>Variant metadata associated with the request.</returns>
    ShaderVariant[] BuildVariants(ShaderCompileRequest request) {
        string variantName = string.IsNullOrWhiteSpace(request.Variant) ? DefaultVariantName : request.Variant;
        return [
            new ShaderVariant(variantName, BuildVariantDefines(request.Defines))
        ];
    }

    /// <summary>
    /// Builds one stable define list for the emitted shader variant metadata.
    /// </summary>
    /// <param name="defines">Compile-time define set.</param>
    /// <returns>Stable define list used by the emitted shader variant metadata.</returns>
    string[] BuildVariantDefines(IReadOnlyList<ShaderDefine> defines) {
        if (defines == null) {
            throw new ArgumentNullException(nameof(defines));
        }

        if (defines.Count == 0) {
            return Array.Empty<string>();
        }

        string[] values = new string[defines.Count];
        for (int defineIndex = 0; defineIndex < defines.Count; defineIndex++) {
            ShaderDefine define = defines[defineIndex];
            values[defineIndex] = string.IsNullOrWhiteSpace(define.Value)
                ? define.Name
                : string.Concat(define.Name, "=", define.Value);
        }

        return values;
    }

    /// <summary>
    /// Validates one shared compile request against the PS Vita backend capabilities.
    /// </summary>
    /// <param name="request">Shared shader compile request.</param>
    void ValidateRequest(ShaderCompileRequest request) {
        if (!IsStageSupported(request.Stage)) {
            throw new InvalidOperationException("PS Vita shader compilation currently supports only vertex and pixel stages.");
        } else if (!IsShaderModelSupported(request.ShaderModel)) {
            throw new InvalidOperationException("PS Vita shader compilation currently supports only shader model 4.0.");
        }
    }

    /// <summary>
    /// Returns whether one shader stage is supported by the PS Vita backend.
    /// </summary>
    /// <param name="stage">Shader stage to validate.</param>
    /// <returns>True when the stage is supported by the backend.</returns>
    bool IsStageSupported(ShaderStage stage) {
        IReadOnlyList<ShaderStage> supportedStages = CapabilitiesValue.SupportedStages;
        for (int stageIndex = 0; stageIndex < supportedStages.Count; stageIndex++) {
            if (supportedStages[stageIndex] == stage) {
                return true;
            }
        }

        return false;
    }

    /// <summary>
    /// Returns whether one shader model is supported by the PS Vita backend.
    /// </summary>
    /// <param name="shaderModel">Shader model to validate.</param>
    /// <returns>True when the shader model is supported by the backend.</returns>
    bool IsShaderModelSupported(ShaderModel shaderModel) {
        if (shaderModel == null) {
            throw new ArgumentNullException(nameof(shaderModel));
        }

        return shaderModel.Major == CapabilitiesValue.MinimumShaderModel.Major
            && shaderModel.Minor == CapabilitiesValue.MinimumShaderModel.Minor;
    }
}
