namespace helengine.psvita.builder;

/// <summary>
/// Holds the PS Vita shader bytecode payload produced for one compiled shader stage.
/// </summary>
public sealed class PsVitaCompiledShader {
    /// <summary>
    /// Stores the compiled bytecode payload.
    /// </summary>
    readonly byte[] BytecodeValue;

    /// <summary>
    /// Initializes one compiled PS Vita shader payload.
    /// </summary>
    /// <param name="bytecode">Compiled bytecode payload for one shader stage.</param>
    public PsVitaCompiledShader(byte[] bytecode) {
        if (bytecode == null) {
            throw new ArgumentNullException(nameof(bytecode));
        } else if (bytecode.Length == 0) {
            throw new ArgumentException("Compiled shader bytecode must be provided.", nameof(bytecode));
        }

        BytecodeValue = bytecode;
    }

    /// <summary>
    /// Gets the compiled bytecode payload.
    /// </summary>
    public byte[] Bytecode {
        get {
            return BytecodeValue;
        }
    }
}
