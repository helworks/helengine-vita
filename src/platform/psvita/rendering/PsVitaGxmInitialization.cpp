#include <psp2/gxm.h>
#include <vita2d.h>
#include "platform/psvita/rendering/PsVitaGxmInitialization.hpp"

/// Calls the original SDK initializer after the linker redirects libvita2d's call.
extern "C" int __real_sceGxmInitialize(const SceGxmInitializeParams* params);

/// Enables extended sampler formats while retaining libvita2d's initialization settings.
/// libvita2d initializes GXM internally without exposing its flags; ETC1 requires
/// SCE_GXM_INITIALIZE_FLAG_EXTENDED_FORMAT in the real initialization request.
extern "C" int __wrap_sceGxmInitialize(const SceGxmInitializeParams* params) {
    if (params == nullptr) {
        return __real_sceGxmInitialize(params);
    }

    SceGxmInitializeParams extendedParams = *params;
    extendedParams.flags |= SCE_GXM_INITIALIZE_FLAG_EXTENDED_FORMAT;
    return __real_sceGxmInitialize(&extendedParams);
}

namespace helengine::psvita::rendering {
    bool ConfigurePsVitaNativeYuvProfiles() {
        SceGxmContext* context = vita2d_get_context();
        return context != nullptr
            && sceGxmSetYuvProfile(context, 0, SCE_GXM_YUV_PROFILE_BT601_STANDARD) >= 0
            && sceGxmSetYuvProfile(context, 1, SCE_GXM_YUV_PROFILE_BT601_STANDARD) >= 0;
    }
}
