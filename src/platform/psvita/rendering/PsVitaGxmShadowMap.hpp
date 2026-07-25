#pragma once

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

#include <vita2d.h>

namespace helengine::psvita::rendering {
    /// Owns the Vita2D offscreen target used by the first directional shadow depth pass.
    class PsVitaGxmShadowMap final {
    public:
        /// Creates one uninitialized offscreen shadow-map owner.
        PsVitaGxmShadowMap();

        /// Allocates the fixed-resolution native render target required before drawing shadow casters.
        bool Initialize();

        /// Releases the native render target after any active shadow pass has finished.
        void Reset();

        /// Begins rendering into the offscreen Vita2D target.
        void BeginDepthPass();

        /// Ends the offscreen Vita2D target pass so the normal frame target can resume.
        void EndDepthPass();

        /// Gets the texture containing the latest completed shadow depth pass.
        vita2d_texture* GetTexture() const;

        /// Gets whether a depth pass currently owns Vita2D drawing.
        bool IsDepthPassActive() const;

    private:
        /// Stores the offscreen texture that Vita2D can both render into and expose for sampling.
        vita2d_texture* Texture;

        /// Stores whether this object currently owns an open Vita2D offscreen pass.
        bool DepthPassActive;
    };
}

#endif
