#pragma once
#include <psp2/gxm.h>
struct vita2d_texture {
    SceGxmTexture gxm_tex;
    SceUID data_UID;
    SceUID palette_UID;
    SceGxmRenderTarget* gxm_rtgt;
    SceGxmColorSurface gxm_sfc;
    SceGxmDepthStencilSurface gxm_sfd;
    SceUID depth_UID;
};
void vita2d_free_texture(vita2d_texture*);
void* vita2d_texture_get_datap(const vita2d_texture*);
unsigned vita2d_texture_get_stride(const vita2d_texture*);
void vita2d_wait_rendering_done();
SceGxmContext* vita2d_get_context();
void vita2d_end_drawing();
void vita2d_start_drawing_advanced(vita2d_texture*, unsigned);
