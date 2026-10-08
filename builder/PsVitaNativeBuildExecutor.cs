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
    /// <param name="nativeObjectCacheRoot">Persistent project/profile native build cache root; an empty value uses the invocation scratch area.</param>
    /// <returns>Absolute path to the produced VPK.</returns>
    public string Build(string repositoryRoot, string nativeBuildRoot, string generatedCoreCppRootPath, string stagedContentRootPath, CancellationToken cancellationToken, string gameTitle = "", string nativeObjectCacheRoot = "") {
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
        string resolvedNativeObjectCacheRoot = string.IsNullOrWhiteSpace(nativeObjectCacheRoot)
            ? Path.Combine(nativeBuildRoot, "native-cache")
            : Path.GetFullPath(nativeObjectCacheRoot);
        string resolvedNativeBuildRoot = Path.GetFullPath(nativeBuildRoot);
        string packageRoot = Path.GetFullPath(Path.Combine(resolvedNativeBuildRoot, "package-output"));
        string nativeBuildRootPrefix = resolvedNativeBuildRoot.TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar)
            + Path.DirectorySeparatorChar;
        if (!packageRoot.StartsWith(nativeBuildRootPrefix, StringComparison.OrdinalIgnoreCase)) {
            throw new InvalidOperationException($"PS Vita package staging path escaped the native build root: '{packageRoot}'.");
        }
        Directory.CreateDirectory(resolvedNativeObjectCacheRoot);
        if (Directory.Exists(packageRoot)) {
            Directory.Delete(packageRoot, true);
        }
        Directory.CreateDirectory(packageRoot);

        RunProcess(
            "docker",
            ["build", "-t", DockerImageTag, "."],
            repositoryRoot,
            Path.Combine(nativeBuildRoot, "docker-build.log"),
            cancellationToken);

        RunProcess(
            "docker",
            CreateNativeBuildArguments(repositoryRoot, generatedCoreCppRootPath, stagedContentRootPath, resolvedNativeObjectCacheRoot, packageRoot, gameTitle),
            repositoryRoot,
            Path.Combine(nativeBuildRoot, "docker-run.log"),
            cancellationToken);

        string sourceVpkPath = Path.Combine(packageRoot, "helengine_psvita.vpk");
        if (!File.Exists(sourceVpkPath)) {
            throw new InvalidOperationException($"Native PS Vita build completed, but no VPK was produced at '{sourceVpkPath}'.");
        }

        string destinationVpkPath = Path.Combine(nativeBuildRoot, "helengine_psvita.vpk");
        File.Copy(sourceVpkPath, destinationVpkPath, true);
        return destinationVpkPath;
    }

    /// <summary>
    /// Creates the Docker command arguments for one cached native build with separate package staging.
    /// </summary>
    /// <param name="repositoryRoot">Absolute PS Vita repository root.</param>
    /// <param name="generatedCoreCppRootPath">Stable generated-core source root.</param>
    /// <param name="stagedContentRootPath">Fresh cooked-content root for the current build.</param>
    /// <param name="nativeObjectCacheRoot">Persistent project/profile CMake and object root.</param>
    /// <param name="packageRoot">Fresh package-output directory for this build.</param>
    /// <param name="gameTitle">Optional editor-authored VPK title.</param>
    /// <returns>Ordered Docker command arguments.</returns>
    public static IReadOnlyList<string> CreateNativeBuildArguments(
        string repositoryRoot,
        string generatedCoreCppRootPath,
        string stagedContentRootPath,
        string nativeObjectCacheRoot,
        string packageRoot,
        string gameTitle) {
        return [
            "run",
            "--rm",
            "-v",
            $"{repositoryRoot}:/workspace",
            "-v",
            $"{generatedCoreCppRootPath}:/generated-core",
            "-v",
            $"{stagedContentRootPath}:/workspace/cooked",
            "-v",
            $"{nativeObjectCacheRoot}:/native-cache",
            "-v",
            $"{packageRoot}:/package-output",
            "-w",
            "/workspace",
            "-e",
            "HELENGINE_CORE_CPP_ROOT=/generated-core",
            DockerImageTag,
            "make",
            "all",
            "NATIVE_OBJECT_CACHE_ROOT=/native-cache",
            "BUILD_DIR=/native-cache",
            "PACKAGE_DIR=/package-output",
            "SOURCE_DIR=/workspace",
            "HELENGINE_PSVITA_GAME_TITLE=" + (string.IsNullOrWhiteSpace(gameTitle) ? string.Empty : gameTitle.Replace("\"", string.Empty).Trim())
        ];
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
