#pragma once

// Various limits for what we can do based on VRAM size

#define kVRAMEnoughForLargeShadowmaps 480	// VRAM MB after which we allow even higher resolution shadow maps

#define kVRAMMaxFreePortionForShadowMap 0.3f // allow single shadowmap to take 30% of possibly free VRAM
#define kVRAMMaxFreePortionForTexture 0.4f // allow single texture to take 40% of possibly free VRAM


int ChooseSuitableFSAALevel( int width, int height, int backbufferBPP, int frontbufferBPP, int depthBPP, int fsaa );
