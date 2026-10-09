namespace helengine.psvita.builder.tests;

/// <summary>Stops at the real SDK boundary after package preparation and staging, without manufacturing a native VPK artifact.</summary>
internal sealed class PsVitaStopAfterStagingNativeBuildExecutor : IPsVitaNativeBuildExecutor {
    /// <summary>Identifies this intentional boundary so tests can distinguish it from a preparation failure.</summary>
    public const string StopMessage = "Stop after real Vita texture staging";
    /// <summary>Records whether all real preparation and publication steps reached the native executor.</summary>
    public bool WasCalled { get; private set; }
    /// <summary>Records the SDK invocation boundary and stops before executing a compiler or writing native outputs.</summary>
    public string Build(string repositoryRoot, string nativeBuildRoot, string generatedCoreCppRootPath, string stagedContentRootPath,
        CancellationToken cancellationToken, string gameTitle = "", string nativeObjectCacheRoot = "") {
        WasCalled = true;
        throw new InvalidOperationException(StopMessage);
    }
}
