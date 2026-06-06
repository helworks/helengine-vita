#pragma once

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

class RuntimeSceneCatalog;

namespace helengine::psvita {
    /// Builds a generated-core runtime scene catalog from the packaged native scene manifest when one is available.
    class PsVitaRuntimeSceneCatalogFactory {
    public:
        /// Builds the runtime scene catalog instance consumed by generated core.
        RuntimeSceneCatalog* Build() const;
    };
}

#endif
