#pragma once

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

#include <string>

namespace helengine::psvita::rendering {
    class PsVitaRuntimeModel;

    /// <summary>
    /// Reads PS Vita packed model payloads whose triangle lists were resolved during staging so runtime loading can skip generic model reconstruction work.
    /// </summary>
    class PsVitaPackedModelReader final {
    public:
        /// <summary>
        /// Attempts to read one packed PS Vita model payload and returns <c>nullptr</c> when the file uses a different asset format.
        /// </summary>
        /// <param name="cookedAssetPath">Cooked asset path to inspect.</param>
        /// <returns>Concrete Vita runtime model when the payload uses the packed Vita model format; otherwise <c>nullptr</c>.</returns>
        static PsVitaRuntimeModel* TryRead(const std::string& cookedAssetPath);
    };
}

#endif
