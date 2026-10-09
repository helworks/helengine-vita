using Xunit;

namespace helengine.psvita.builder.tests;

/// <summary>Serializes fixtures that temporarily set the Vita repository environment and authored project working directory.</summary>
[CollectionDefinition("VitaTextureStaging", DisableParallelization = true)]
public sealed class PsVitaTextureTestCollection {
}
