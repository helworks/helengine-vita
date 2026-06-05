using helengine.psvita.builder;

/// <summary>
/// Exposes the PS Vita builder command-line surface used by editor discovery and smoke checks.
/// </summary>
public static class Program {
    /// <summary>
    /// Runs the PS Vita builder command-line surface.
    /// </summary>
    /// <param name="args">Command-line arguments.</param>
    /// <returns>Process exit code.</returns>
    public static int Main(string[] args) {
        if (args.Length > 0 && string.Equals(args[0], "--describe", StringComparison.OrdinalIgnoreCase)) {
            PsVitaPlatformAssetBuilder builder = new();
            Console.WriteLine(builder.Descriptor.BuilderId);
            Console.WriteLine(builder.Descriptor.TargetPlatformId);
            Console.WriteLine(builder.Definition.DisplayName);
            Console.WriteLine(builder.Definition.BuildProfiles.Length);
            Console.WriteLine(builder.Definition.GraphicsProfiles.Length);
            return 0;
        }

        if (args.Length > 0 && string.Equals(args[0], "--smoke-test", StringComparison.OrdinalIgnoreCase)) {
            PsVitaPlatformAssetBuilder builder = new();
            Console.WriteLine(builder.Descriptor.BuilderId);
            Console.WriteLine("ok");
            return 0;
        }

        Console.WriteLine("helengine.psvita.builder --describe|--smoke-test");
        return 0;
    }
}
