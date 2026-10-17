#include <psp2/gxm.h>
#include <vita2d.h>
#include <stdexcept>
#include "platform/psvita/rendering/PsVitaGxmInitialization.hpp"

/// Calls the original SDK initializer after the linker redirects libvita2d's call.
extern "C" int __real_sceGxmInitialize(const SceGxmInitializeParams* params);

namespace {
    /// Records the real SDK result because libvita2d does not propagate initialization errors.
    int GxmInitializationResult = -1;
    /// Marks whether the current successful initialization explicitly requested extended texture formats.
    bool ExtendedTextureFormatsEnabled = false;
}

/// Preserves ordinary libvita2d initialization and routes explicitly requested extended flags to the SDK's extended API.
/// The ordinary sceGxmInitialize API rejects EXTENDED_FORMAT; enabling it automatically prevents display-queue creation.
extern "C" int __wrap_sceGxmInitialize(const SceGxmInitializeParams* params) {
    const bool extended = params != nullptr
        && (params->flags & SCE_GXM_INITIALIZE_FLAG_EXTENDED_FORMAT) != 0u;
    GxmInitializationResult = extended ? sceGxmVshInitialize(params) : __real_sceGxmInitialize(params);
    ExtendedTextureFormatsEnabled = extended && GxmInitializationResult >= 0;
    return GxmInitializationResult;
}

namespace helengine::psvita::rendering {
    void ValidatePsVitaGxmInitialization() {
        if (GxmInitializationResult < 0) {
            throw std::runtime_error("PS Vita GXM initialization failed before the display queue was created.");
        }
    }

    bool ArePsVitaExtendedTextureFormatsEnabled() {
        return ExtendedTextureFormatsEnabled;
    }

    bool ConfigurePsVitaNativeYuvProfiles() {
        SceGxmContext* context = vita2d_get_context();
        return context != nullptr
            && sceGxmSetYuvProfile(context, 0, SCE_GXM_YUV_PROFILE_BT601_STANDARD) >= 0
            && sceGxmSetYuvProfile(context, 1, SCE_GXM_YUV_PROFILE_BT601_STANDARD) >= 0;
    }
}
