namespace helengine.psvita.builder.tests;

/// <summary>
/// Resolves repository-root-relative paths for PS Vita builder tests even when the test assembly is emitted into a temporary output directory.
/// </summary>
public static class PsVitaRepositoryPathResolver {
    /// <summary>
    /// Resolves the absolute repository root for the current <c>helengine-psvita</c> checkout.
    /// </summary>
    /// <returns>Absolute path to the repository root.</returns>
    /// <exception cref="DirectoryNotFoundException">Thrown when the repository root cannot be discovered from the environment or process directories.</exception>
    public static string ResolveRepositoryRoot() {
        string repositoryRootPath = Environment.GetEnvironmentVariable("HELENGINE_PSVITA_REPOSITORY_ROOT");
        if (string.IsNullOrWhiteSpace(repositoryRootPath)) {
            repositoryRootPath = Path.GetFullPath(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", ".."));
        }

        string repositoryMarkerPath = Path.Combine("src", "platform", "psvita", "rendering", "PsVitaRenderManager2D.hpp");
        DirectoryInfo candidateDirectory = new DirectoryInfo(repositoryRootPath);
        while (candidateDirectory != null && !File.Exists(Path.Combine(candidateDirectory.FullName, repositoryMarkerPath))) {
            candidateDirectory = candidateDirectory.Parent;
        }

        if (candidateDirectory == null) {
            candidateDirectory = new DirectoryInfo(Directory.GetCurrentDirectory());
            while (candidateDirectory != null && !File.Exists(Path.Combine(candidateDirectory.FullName, repositoryMarkerPath))) {
                candidateDirectory = candidateDirectory.Parent;
            }
        }

        if (candidateDirectory == null) {
            throw new DirectoryNotFoundException("Could not locate the helengine-psvita repository root from the test process.");
        }

        return candidateDirectory.FullName;
    }

    /// <summary>
    /// Resolves one repository-relative path into an absolute filesystem path.
    /// </summary>
    /// <param name="pathSegments">Path segments relative to the repository root.</param>
    /// <returns>Absolute path rooted at the current <c>helengine-psvita</c> checkout.</returns>
    public static string ResolvePath(params string[] pathSegments) {
        return Path.Combine(new[] { ResolveRepositoryRoot() }.Concat(pathSegments).ToArray());
    }
}
