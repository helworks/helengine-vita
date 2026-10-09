namespace helengine.psvita.builder;

/// <summary>Validates texture and manifest destinations before any Vita staging output is replaced.</summary>
internal static class PsVitaTextureCookPath {
    /// <summary>Rejects a staging root or ancestor that is a file or filesystem link before a workspace can reset the tree.</summary>
    public static void ValidateRoot(string root) {
        string current = Path.GetFullPath(root);
        while (!string.IsNullOrEmpty(current)) {
            if (File.Exists(current)) throw new InvalidOperationException("Vita staging root ancestor is a file.");
            if (Directory.Exists(current) && (File.GetAttributes(current) & FileAttributes.ReparsePoint) != 0) throw new InvalidOperationException("Vita staging root crosses a filesystem link.");
            current = Path.GetDirectoryName(current);
        }
    }
    /// <summary>Rejects rooted paths, traversal, Windows stream/device aliases and invalid filename characters.</summary>
    public static string Normalize(string path) {
        if (string.IsNullOrWhiteSpace(path)) throw new InvalidOperationException("Vita cooked path is required.");
        string normalized = path.Replace('\\', '/');
        if (Path.IsPathRooted(path) || normalized.StartsWith('/')) throw new InvalidOperationException("Vita cooked path must be runtime-relative.");
        string[] segments = normalized.Split('/', StringSplitOptions.RemoveEmptyEntries);
        foreach (string segment in segments) {
            string stem = segment.Split('.')[0];
            bool device = stem.Equals("CON", StringComparison.OrdinalIgnoreCase) || stem.Equals("PRN", StringComparison.OrdinalIgnoreCase) ||
                stem.Equals("AUX", StringComparison.OrdinalIgnoreCase) || stem.Equals("NUL", StringComparison.OrdinalIgnoreCase) ||
                (stem.Length == 4 && (stem.StartsWith("COM", StringComparison.OrdinalIgnoreCase) || stem.StartsWith("LPT", StringComparison.OrdinalIgnoreCase)) && stem[3] >= '1' && stem[3] <= '9');
            if (segment == "." || segment == ".." || segment.IndexOfAny(Path.GetInvalidFileNameChars()) >= 0 || device || segment.EndsWith('.') || segment.EndsWith(' ')) throw new InvalidOperationException($"Unsafe Vita cooked path '{path}'.");
        }
        if (segments.Length == 0) throw new InvalidOperationException("Vita cooked path is empty.");
        return string.Join('/', segments);
    }

    /// <summary>Resolves a contained file path and excludes filesystem links and existing file/directory conflicts.</summary>
    public static string Resolve(string root, string relativePath) {
        string fullRoot = Path.TrimEndingDirectorySeparator(Path.GetFullPath(root));
        string destination = Path.GetFullPath(Path.Combine(fullRoot, Normalize(relativePath).Replace('/', Path.DirectorySeparatorChar)));
        if (!destination.StartsWith(fullRoot + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase)) throw new InvalidOperationException("Vita cooked path escaped its root.");
        string current = destination;
        while (!string.IsNullOrEmpty(current)) {
            bool exists = File.Exists(current) || Directory.Exists(current);
            if (exists && (File.GetAttributes(current) & FileAttributes.ReparsePoint) != 0) throw new InvalidOperationException($"Vita cooked path crosses a filesystem link at '{current}'.");
            if (current == destination && Directory.Exists(current)) throw new InvalidOperationException("Vita cooked file destination is already a directory.");
            if (current != destination && File.Exists(current)) throw new InvalidOperationException("Vita cooked output ancestor is already a file.");
            current = Path.GetDirectoryName(current);
        }
        return destination;
    }

    /// <summary>Reserves case-insensitive runtime package paths against exact or parent/child collisions.</summary>
    public static void Reserve(HashSet<string> paths, string relativePath) {
        string normalized = Normalize(relativePath);
        string physical = normalized;
        foreach (string existing in paths) {
            if (physical.Equals(existing, StringComparison.OrdinalIgnoreCase) || physical.StartsWith(existing + "/", StringComparison.OrdinalIgnoreCase) || existing.StartsWith(physical + "/", StringComparison.OrdinalIgnoreCase)) {
                throw new InvalidOperationException($"Vita cooked path '{relativePath}' conflicts with '{existing}'.");
            }
        }
        paths.Add(physical);
    }
}
