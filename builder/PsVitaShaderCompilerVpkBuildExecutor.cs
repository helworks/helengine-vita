using System.Diagnostics;
using helengine.baseplatform.Builders;

namespace helengine.psvita.builder;

/// <summary>
/// Builds the standalone Helengine Vita Shader Compiler VPK through the repository VitaSDK Docker image.
/// </summary>
public sealed class PsVitaShaderCompilerVpkBuildExecutor {
    /// <summary>
    /// Docker image tag shared with the native PS Vita player build.
    /// </summary>
    const string DockerImageTag = "helengine-psvita";

    /// <summary>
    /// Builds the standalone compiler VPK and copies it into the explicit output directory.
    /// </summary>
    /// <param name="repositoryRoot">Absolute PS Vita repository root containing the compiler CMake project.</param>
    /// <param name="outputRoot">Absolute or relative output directory for the resulting compiler VPK.</param>
    /// <param name="cancellationToken">Cancellation token that can stop Docker processes.</param>
    /// <returns>Absolute path to the copied compiler VPK.</returns>
    public string Build(string repositoryRoot, string outputRoot, CancellationToken cancellationToken) {
        if (string.IsNullOrWhiteSpace(repositoryRoot)) {
            throw new ArgumentException("Repository root must be provided.", nameof(repositoryRoot));
        } else if (string.IsNullOrWhiteSpace(outputRoot)) {
            throw new ArgumentException("Output root must be provided.", nameof(outputRoot));
        }

        string fullRepositoryRoot = Path.GetFullPath(repositoryRoot);
        if (!File.Exists(Path.Combine(fullRepositoryRoot, "tools", "shader-compiler", "CMakeLists.txt"))) {
            throw new DirectoryNotFoundException($"PS Vita shader compiler CMake project was not found beneath '{fullRepositoryRoot}'.");
        }

        string fullOutputRoot = Path.GetFullPath(outputRoot);
        Directory.CreateDirectory(fullOutputRoot);
        RunProcess(
            "docker",
            ["build", "-t", DockerImageTag, "."],
            fullRepositoryRoot,
            Path.Combine(fullOutputRoot, "shader-compiler-docker-build.log"),
            cancellationToken);
        RunProcess(
            "docker",
            [
                "run",
                "--rm",
                "-v",
                $"{fullRepositoryRoot}:/workspace",
                "-w",
                "/workspace",
                DockerImageTag,
                "bash",
                "-lc",
                "cmake -S tools/shader-compiler -B build/shader-compiler && cmake --build build/shader-compiler"
            ],
            fullRepositoryRoot,
            Path.Combine(fullOutputRoot, "shader-compiler-docker-run.log"),
            cancellationToken);

        string sourceVpkPath = Path.Combine(fullRepositoryRoot, "build", "shader-compiler", "helengine_psvita_shader_compiler.vpk");
        if (!File.Exists(sourceVpkPath)) {
            throw new InvalidOperationException($"PS Vita shader compiler build completed, but no VPK was produced at '{sourceVpkPath}'.");
        }

        string destinationVpkPath = Path.Combine(fullOutputRoot, "helengine_psvita_shader_compiler.vpk");
        File.Copy(sourceVpkPath, destinationVpkPath, true);
        return destinationVpkPath;
    }

    /// <summary>
    /// Runs one Docker process and writes its combined output into the supplied log file.
    /// </summary>
    /// <param name="fileName">Executable name.</param>
    /// <param name="arguments">Ordered process arguments.</param>
    /// <param name="workingDirectory">Working directory for the process.</param>
    /// <param name="logPath">Path receiving standard output and error.</param>
    /// <param name="cancellationToken">Cancellation token that can stop the process.</param>
    static void RunProcess(string fileName, IReadOnlyList<string> arguments, string workingDirectory, string logPath, CancellationToken cancellationToken) {
        ProcessStartInfo startInfo = new() {
            FileName = fileName,
            WorkingDirectory = workingDirectory,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            UseShellExecute = false,
            CreateNoWindow = true
        };
        for (int index = 0; index < arguments.Count; index++) {
            startInfo.ArgumentList.Add(arguments[index]);
        }

        NativeProcessRunResult result = new NativeProcessRunner().Run(startInfo, cancellationToken);
        Directory.CreateDirectory(Path.GetDirectoryName(logPath) ?? workingDirectory);
        File.WriteAllText(logPath, result.StandardOutput + result.StandardError);
        if (result.ExitCode != 0) {
            throw new InvalidOperationException($"Process '{fileName}' failed with exit code {result.ExitCode}. See '{logPath}'.");
        }
    }
}
