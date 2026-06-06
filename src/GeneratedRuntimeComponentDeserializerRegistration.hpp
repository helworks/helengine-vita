#pragma once

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

class RuntimeComponentRegistry;

/// Provides one PS Vita-local fallback for generated runtime component deserializer registration when the editor-generated core does not emit an explicit registration header.
inline void RegisterGeneratedRuntimeComponentDeserializers(::RuntimeComponentRegistry* registry) {
    (void)registry;
}

#endif
