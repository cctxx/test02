#pragma once

// Cleanup and recreate as separate steps
void CleanupAllGfxDeviceResources();
void RecreateAllGfxDeviceResources();

// Cleanup and recreate as a single step
void RecreateGfxDevice();

void RecreateSkinnedMeshResources();
