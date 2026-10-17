#pragma once

namespace helengine::psvita::rendering {
    /// Rejects a failed real GXM initialization that libvita2d would otherwise report as successful.
    void ValidatePsVitaGxmInitialization();

    /// Reports successful explicit extended-API initialization; ordinary application initialization does not enable ETC1.
    bool ArePsVitaExtendedTextureFormatsEnabled();

    /// Configures both GXM YUV selector banks for the canonical BT.601 limited-range native payload contract.
    bool ConfigurePsVitaNativeYuvProfiles();
}
