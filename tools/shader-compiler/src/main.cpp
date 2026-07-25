#include "PsVitaShaderCompilerQueueProcessor.hpp"

/// Stores the only manifest path accepted by the launch-and-exit compiler VPK.
constexpr const char* InboxManifestPath = "ux0:data/helengine_shader_compiler/inbox/manifest.json";

/// Stores the only source root accepted by the launch-and-exit compiler VPK.
constexpr const char* InboxRootPath = "ux0:data/helengine_shader_compiler/inbox";

/// Stores the only result root published by the launch-and-exit compiler VPK.
constexpr const char* OutboxRootPath = "ux0:data/helengine_shader_compiler/outbox";

/// Launches one fixed-location device compiler job and exits immediately after publishing its result.
/// <returns>Zero for a successful job or a nonzero compiler status.</returns>
int main() {
    helengine::psvita::shadercompiler::PsVitaShaderCompilerQueueProcessor queueProcessor(InboxManifestPath, InboxRootPath, OutboxRootPath);
    return queueProcessor.ProcessSingleJob();
}
