#include "platform/psvita/PsVitaBootHost.hpp"
#include <cstring>

#if HELENGINE_PSVITA_HAS_GENERATED_CORE
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>

#include "platform/psvita/shaders/PsVitaShaderArtifactExportCommand.hpp"
#endif

int main(int argc, char** argv) {
#if HELENGINE_PSVITA_HAS_GENERATED_CORE
    SceIoStat exportRequestStat;
    if (sceIoGetstat("ux0:data/helengine/export-forward-lambert.flag", &exportRequestStat) >= 0
        || (argc == 2 && std::strcmp(argv[1], "--export-forward-lambert") == 0)) {
        helengine::psvita::shaders::PsVitaShaderArtifactExportCommand command;
        int result = command.Run();
        if (result == 0) {
            sceIoRemove("ux0:data/helengine/export-forward-lambert.flag");
        }
        return result;
    }
#else
    (void)argc;
    (void)argv;
#endif

    helengine::psvita::PsVitaBootHost host;
    return host.Run();
}
