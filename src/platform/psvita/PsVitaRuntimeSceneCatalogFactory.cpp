#include "platform/psvita/PsVitaRuntimeSceneCatalogFactory.hpp"

#include <stdexcept>

#if HELENGINE_PSVITA_HAS_GENERATED_CORE && __has_include("RuntimeSceneCatalog.hpp") && __has_include("RuntimeSceneCatalogEntry.hpp") && __has_include("runtime/runtime_scene_catalog_manifest.hpp")
#define HELENGINE_PSVITA_HAS_RUNTIME_MANIFESTS 1
#include "RuntimeSceneCatalog.hpp"
#include "RuntimeSceneCatalogEntry.hpp"
#include "runtime/array.hpp"
#include "runtime/runtime_scene_catalog_manifest.hpp"
#else
#define HELENGINE_PSVITA_HAS_RUNTIME_MANIFESTS 0
#endif

namespace helengine::psvita {
    /// Builds the runtime scene catalog instance consumed by generated core.
    RuntimeSceneCatalog* PsVitaRuntimeSceneCatalogFactory::Build() const {
#if HELENGINE_PSVITA_HAS_RUNTIME_MANIFESTS
        std::size_t nativeEntryCount = 0;
        const HERuntimeSceneCatalogEntry* nativeEntries = he_runtime_scene_catalog_entries(&nativeEntryCount);
        if (nativeEntries == nullptr || nativeEntryCount == 0) {
            throw std::runtime_error("PS Vita runtime scene catalog manifest did not contain any entries.");
        }

        Array<::RuntimeSceneCatalogEntry*>* sceneEntries = new Array<::RuntimeSceneCatalogEntry*>(static_cast<int32_t>(nativeEntryCount));
        for (std::size_t index = 0; index < nativeEntryCount; index++) {
            const HERuntimeSceneCatalogEntry& nativeEntry = nativeEntries[index];
            (*sceneEntries)[static_cast<int32_t>(index)] = new ::RuntimeSceneCatalogEntry(nativeEntry.SceneId, nativeEntry.CookedRelativePath);
        }

        return new RuntimeSceneCatalog(sceneEntries);
#else
        return nullptr;
#endif
    }
}
