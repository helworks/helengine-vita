using System.Security.Cryptography;
using System.Text;

namespace helengine.psvita.builder;

/// <summary>
/// Produces the PS Vita shader payload currently carried through the shared Helengine shader pipeline.
/// </summary>
public sealed class PsVitaShaderCompiler {
    /// <summary>
    /// Stable built-in shader file name currently supported by the PS Vita shader pipeline.
    /// </summary>
    const string SupportedShaderFileName = "ForwardSolidColorShader.hlsl";

    /// <summary>
    /// Stable vertex entry point expected by the shared built-in solid-color shader.
    /// </summary>
    const string VertexEntryPoint = "VS";

    /// <summary>
    /// Stable pixel entry point expected by the shared built-in solid-color shader.
    /// </summary>
    const string PixelEntryPoint = "PS";

    /// <summary>
    /// Stable bytecode header used to mark PS Vita shader payloads produced by this compiler wrapper.
    /// </summary>
    static readonly byte[] BytecodeHeader = Encoding.ASCII.GetBytes("HELENGINE_PSVITA_SHADER");

    /// <summary>
    /// Compiles one shared shader request into the PS Vita bytecode payload currently carried by the editor pipeline.
    /// </summary>
    /// <param name="request">Shared shader compile request.</param>
    /// <returns>Compiled PS Vita shader payload.</returns>
    public PsVitaCompiledShader Compile(ShaderCompileRequest request) {
        if (request == null) {
            throw new ArgumentNullException(nameof(request));
        }

        ValidateRequest(request);
        return new PsVitaCompiledShader(BuildBytecodePayload(request));
    }

    /// <summary>
    /// Validates one shared shader compile request against the current PS Vita shader compiler constraints.
    /// </summary>
    /// <param name="request">Shared shader compile request.</param>
    void ValidateRequest(ShaderCompileRequest request) {
        if (request.Target != ShaderCompileTarget.PsVita) {
            throw new InvalidOperationException("PsVitaShaderCompiler only supports PS Vita shader requests.");
        } else if (request.Source == null) {
            throw new InvalidOperationException("Shader source information must be provided.");
        } else if (string.IsNullOrWhiteSpace(request.Source.Path)) {
            throw new InvalidOperationException("Shader source path must be provided.");
        } else if (string.IsNullOrWhiteSpace(request.Source.Source)) {
            throw new InvalidOperationException("Shader source text must be provided.");
        }

        ValidateSupportedShader(request);
        ValidateSupportedStage(request);
        ValidateSourceShape(request);
    }

    /// <summary>
    /// Validates the built-in shader identity expected by the current PS Vita pipeline.
    /// </summary>
    /// <param name="request">Shared shader compile request.</param>
    void ValidateSupportedShader(ShaderCompileRequest request) {
        string shaderFileName = Path.GetFileName(request.Source.Path);
        if (!string.Equals(shaderFileName, SupportedShaderFileName, StringComparison.OrdinalIgnoreCase)) {
            throw new InvalidOperationException($"PS Vita shader compilation currently supports only '{SupportedShaderFileName}'.");
        }
    }

    /// <summary>
    /// Validates the requested shader stage and entry point.
    /// </summary>
    /// <param name="request">Shared shader compile request.</param>
    void ValidateSupportedStage(ShaderCompileRequest request) {
        if (request.Stage == ShaderStage.Vertex) {
            if (!string.Equals(request.EntryPoint, VertexEntryPoint, StringComparison.Ordinal)) {
                throw new InvalidOperationException($"PS Vita vertex compilation requires the '{VertexEntryPoint}' entry point.");
            }

            return;
        } else if (request.Stage == ShaderStage.Pixel) {
            if (!string.Equals(request.EntryPoint, PixelEntryPoint, StringComparison.Ordinal)) {
                throw new InvalidOperationException($"PS Vita pixel compilation requires the '{PixelEntryPoint}' entry point.");
            }

            return;
        }

        throw new InvalidOperationException("PS Vita shader compilation currently supports only vertex and pixel stages.");
    }

    /// <summary>
    /// Validates the current shared shader source shape supported by the PS Vita pipeline wrapper.
    /// </summary>
    /// <param name="request">Shared shader compile request.</param>
    void ValidateSourceShape(ShaderCompileRequest request) {
        if (request.Source.Source.Contains("#include", StringComparison.Ordinal)) {
            throw new InvalidOperationException("PS Vita shader compilation does not currently support shader includes.");
        }

        if (!request.Source.Source.Contains("HelengineWorldViewProjection", StringComparison.Ordinal)
            || !request.Source.Source.Contains("HelengineBaseColor", StringComparison.Ordinal)) {
            throw new InvalidOperationException("PS Vita shader compilation requires the shared solid-color shader contract.");
        }
    }

    /// <summary>
    /// Builds the deterministic PS Vita bytecode payload carried by the shared shader asset.
    /// </summary>
    /// <param name="request">Shared shader compile request.</param>
    /// <returns>Bytecode payload associated with the request.</returns>
    byte[] BuildBytecodePayload(ShaderCompileRequest request) {
        string signatureText = BuildSignatureText(request);
        byte[] signatureBytes = Encoding.UTF8.GetBytes(signatureText);
        byte[] hashBytes = SHA256.HashData(signatureBytes);
        byte[] bytecode = new byte[BytecodeHeader.Length + hashBytes.Length + signatureBytes.Length];
        Buffer.BlockCopy(BytecodeHeader, 0, bytecode, 0, BytecodeHeader.Length);
        Buffer.BlockCopy(hashBytes, 0, bytecode, BytecodeHeader.Length, hashBytes.Length);
        Buffer.BlockCopy(signatureBytes, 0, bytecode, BytecodeHeader.Length + hashBytes.Length, signatureBytes.Length);
        return bytecode;
    }

    /// <summary>
    /// Builds one stable request signature used to derive the PS Vita bytecode payload.
    /// </summary>
    /// <param name="request">Shared shader compile request.</param>
    /// <returns>Stable request signature.</returns>
    string BuildSignatureText(ShaderCompileRequest request) {
        StringBuilder builder = new StringBuilder();
        builder.Append("Target=");
        builder.Append(ShaderTargetNames.GetTargetName(request.Target));
        builder.Append(";Stage=");
        builder.Append(request.Stage.ToString());
        builder.Append(";Program=");
        builder.Append(request.ProgramName);
        builder.Append(";Entry=");
        builder.Append(request.EntryPoint);
        builder.Append(";Variant=");
        builder.Append(request.Variant);
        builder.Append(";Model=");
        builder.Append(request.ShaderModel.ToString());
        builder.Append(";Defines=");
        builder.Append(BuildDefinesSignature(request.Defines));
        builder.Append(";Source=");
        builder.Append(request.Source.Source);
        return builder.ToString();
    }

    /// <summary>
    /// Builds one stable define signature for the compile request.
    /// </summary>
    /// <param name="defines">Compile-time define set.</param>
    /// <returns>Stable define signature.</returns>
    string BuildDefinesSignature(IReadOnlyList<ShaderDefine> defines) {
        if (defines == null) {
            throw new ArgumentNullException(nameof(defines));
        }

        if (defines.Count == 0) {
            return string.Empty;
        }

        StringBuilder builder = new StringBuilder();
        for (int defineIndex = 0; defineIndex < defines.Count; defineIndex++) {
            ShaderDefine define = defines[defineIndex];
            builder.Append(define.Name);
            builder.Append('=');
            builder.Append(define.Value);
            builder.Append(';');
        }

        return builder.ToString();
    }
}
