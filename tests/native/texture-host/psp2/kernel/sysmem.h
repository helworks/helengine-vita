#pragma once
#include <psp2/gxm.h>
constexpr unsigned SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE = 1;
int sceKernelAllocMemBlock(const char*, unsigned, SceSize, const void*);
int sceKernelGetMemBlockBase(SceUID, void**);
int sceKernelFreeMemBlock(SceUID);
