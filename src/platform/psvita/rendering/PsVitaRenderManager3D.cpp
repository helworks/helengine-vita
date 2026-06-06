#include "platform/psvita/rendering/PsVitaRenderManager3D.hpp"

#if HELENGINE_PSVITA_HAS_GENERATED_CORE

#include "Core.hpp"
#include "ICamera.hpp"
#include "ObjectManager.hpp"
#include "platform/psvita/rendering/PsVitaRenderManager2D.hpp"

namespace {
    constexpr int PsVitaScreenWidth = 960;
    constexpr int PsVitaScreenHeight = 544;
}

namespace helengine::psvita {
    /// Creates the temporary PS Vita 3D renderer with the Vita display size.
    PsVitaRenderManager3D::PsVitaRenderManager3D() {
        set_MainWindowSize(::int2(PsVitaScreenWidth, PsVitaScreenHeight));
    }

    /// Dispatches camera-owned 2D queues into the Vita 2D renderer until native 3D rendering is implemented.
    void PsVitaRenderManager3D::Draw() {
        if (Core::Instance == nullptr || Core::Instance->ObjectManager == nullptr || Core::Instance->RenderManager2D == nullptr) {
            return;
        }

        PsVitaRenderManager2D* renderManager2D = dynamic_cast<PsVitaRenderManager2D*>(Core::Instance->RenderManager2D);
        if (renderManager2D == nullptr) {
            return;
        }

        List<::ICamera*>* cameras = Core::Instance->ObjectManager->get_Cameras();
        if (cameras == nullptr) {
            return;
        }

        for (int32_t cameraIndex = 0; cameraIndex < cameras->get_Count(); cameraIndex++) {
            ::ICamera* camera = (*cameras).get_Item(static_cast<int32_t>(cameraIndex));
            if (camera == nullptr) {
                continue;
            }

            renderManager2D->DrawCamera(camera);
        }
    }

    /// Builds a runtime model from raw data.
    ::RuntimeModel* PsVitaRenderManager3D::BuildModelFromRaw(::ModelAsset* data) {
        return nullptr;
    }
}

#endif
