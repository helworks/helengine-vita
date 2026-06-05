using System.Diagnostics;
using System.Text;

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
    /// <returns>Absolute path to the produced VPK.</returns>
    public string Build(string repositoryRoot, string nativeBuildRoot, string generatedCoreCppRootPath, string stagedContentRootPath, CancellationToken cancellationToken) {
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
                "all"
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
        StringBuilder logBuilder = new();
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

        using Process process = Process.Start(startInfo) ?? throw new InvalidOperationException($"Failed to start '{fileName}'.");
        using CancellationTokenRegistration cancellationRegistration = cancellationToken.Register(() => TryKillProcess(process));
        process.OutputDataReceived += (_, eventArgs) => {
            if (!string.IsNullOrEmpty(eventArgs.Data)) {
                logBuilder.AppendLine(eventArgs.Data);
            }
        };
        process.ErrorDataReceived += (_, eventArgs) => {
            if (!string.IsNullOrEmpty(eventArgs.Data)) {
                logBuilder.AppendLine(eventArgs.Data);
            }
        };
        process.BeginOutputReadLine();
        process.BeginErrorReadLine();

        while (!process.HasExited) {
            cancellationToken.ThrowIfCancellationRequested();
            process.WaitForExit(100);
        }

        process.WaitForExit();
        Directory.CreateDirectory(Path.GetDirectoryName(logPath) ?? workingDirectory);
        File.WriteAllText(logPath, logBuilder.ToString());

        if (process.ExitCode != 0) {
            throw new InvalidOperationException($"Process '{fileName}' failed with exit code {process.ExitCode}. See '{logPath}'.");
        }
    }

    /// <summary>
    /// Attempts to stop one process tree during cancellation.
    /// </summary>
    /// <param name="process">Process to stop.</param>
    static void TryKillProcess(Process process) {
        try {
            if (process != null && !process.HasExited) {
                process.Kill(entireProcessTree: true);
            }
        } catch {
        }
    }
}
