#include <cstdio>

#include "platform/psvita/PsVitaBootHost.hpp"

int main() {
    std::FILE* file = std::fopen("ux0:/data/helengine_psvita_boot.log", "a");
    if (file != nullptr) {
        std::fputs("main: entered\n", file);
        std::fclose(file);
    }

    helengine::psvita::PsVitaBootHost host;
    return host.Run();
}
