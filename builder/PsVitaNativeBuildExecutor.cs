using System.Diagnostics;
using helengine.baseplatform.Builders;

namespace helengine.psvita.builder;

/// <summary>
/// Executes the Docker-backed VitaSDK native build for the PS Vita player.
/// </summary>
public sealed class PsVitaNativeBuildExecutor : IPsVitaNativeBuildExecutor {
    /// <summary>
    /// Docker image tag used for the local PS Vita build image.
    /// </summary>
    const string DockerImageTag = "helengine-psvita";

    /// <summary>
    /// Builds the native PS Vita player and returns the produced VPK path.
    /// </summary>
    /// <param name="repositoryRoot">Absolute PS Vita repository root.</param>
    /// <param name="nativeBuildRoot">Absolute scratch directory for native build artifacts.</param>
    /// <param name="generatedCoreCppRootPath">Absolute generated core C++ root supplied by the editor.</param>
    /// <param name="stagedContentRootPath">Absolute staged cooked-content root supplied by the builder.</param>
    /// <param name="cancellationToken">Cancellation token that can stop the native build.</param>
    /// <param name="gameTitle">Editor-authored app name stamped into the VPK metadata; empty keeps the toolchain default.</param>
    /// <returns>Absolute path to the produced VPK.</returns>
    public string Build(string repositoryRoot, string nativeBuildRoot, string generatedCoreCppRootPath, string stagedContentRootPath, CancellationToken cancellationToken, string gameTitle = "") {
        if (string.IsNullOrWhiteSpace(repositoryRoot)) {
            throw new ArgumentException("Repository root must be provided.", nameof(repositoryRoot));
        } else if (string.IsNullOrWhiteSpace(nativeBuildRoot)) {
            throw new ArgumentException("Native build root must be provided.", nameof(nativeBuildRoot));
        } else if (string.IsNullOrWhiteSpace(generatedCoreCppRootPath)) {
            throw new ArgumentException("Generated core root must be provided.", nameof(generatedCoreCppRootPath));
        } else if (string.IsNullOrWhiteSpace(stagedContentRootPath)) {
            throw new ArgumentException("Staged content root must be provided.", nameof(stagedContentRootPath));
        }

        Directory.CreateDirectory(nativeBuildRoot);
        Directory.CreateDirectory(generatedCoreCppRootPath);
        Directory.CreateDirectory(stagedContentRootPath);

        RunProcess(
            "docker",
            ["build", "-t", DockerImageTag, "."],
            repositoryRoot,
            Path.Combine(nativeBuildRoot, "docker-build.log"),
            cancellationToken);

        RunProcess(
            "docker",
            [
                "run",
                "--rm",
                "-v",
                $"{repositoryRoot}:/workspace",
                "-v",
                $"{generatedCoreCppRootPath}:/generated-core",
                "-v",
                $"{stagedContentRootPath}:/workspace/cooked",
                "-w",
                "/workspace",
                "-e",
                "HELENGINE_CORE_CPP_ROOT=/generated-core",
                DockerImageTag,
                "make",
                "clean",
                "all",
                "HELENGINE_PSVITA_GAME_TITLE=" + (string.IsNullOrWhiteSpace(gameTitle) ? string.Empty : gameTitle.Replace("\"", string.Empty).Trim())
            ],
            repositoryRoot,
            Path.Combine(nativeBuildRoot, "docker-run.log"),
            cancellationToken);

        string sourceVpkPath = Path.Combine(repositoryRoot, "build", "helengine_psvita.vpk");
        if (!File.Exists(sourceVpkPath)) {
            throw new InvalidOperationException($"Native PS Vita build completed, but no VPK was produced at '{sourceVpkPath}'.");
        }

        string destinationVpkPath = Path.Combine(nativeBuildRoot, "helengine_psvita.vpk");
        File.Copy(sourceVpkPath, destinationVpkPath, true);
        return destinationVpkPath;
    }

    /// <summary>
    /// Runs one process and writes combined output to a log file.
    /// </summary>
    /// <param name="fileName">Executable name.</param>
    /// <param name="arguments">Ordered process arguments.</param>
    /// <param name="workingDirectory">Process working directory.</param>
    /// <param name="logPath">Log path for combined standard output and error.</param>
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
