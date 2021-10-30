#ifndef RENDERBUFFERMANAGER_H
#define RENDERBUFFERMANAGER_H

#include "Runtime/GfxDevice/GfxDeviceTypes.h"
#include "Runtime/Utilities/MemoryPool.h"
#include "Runtime/BaseClasses/BaseObject.h"

class RenderTexture;


/* Manager for getting temporary render buffers.
 * Use this instead of creating RenderTextures if you need a quick buffer.
 * This is a low-overhead class that recycles render textures.
 */
class RenderBufferManager
{
	RenderBufferManager () { m_CurrentRBMFrame = m_TempBuffers = 0; }

public:

	enum { kFullSize = -1 };
	enum {
		kRBCubemap = (1<<0),
		kRBCreatedFromScript = (1<<1),
		kRBSampleOnlyDepth = (1<<2),
	};
	enum { kKillFrames = 15 };

	static void InitRenderBufferManager ();
	static void CleanupRenderBufferManager ();

	// Get a RenderTexture with the specific sizes
	// If the width & height parameters uses autosizing, viewport size is taken from the current camera.
	RenderTexture *GetTempBuffer (int width, int height, DepthBufferFormat depthFormat, RenderTextureFormat colorFormat, UInt32 flags, RenderTextureReadWrite colorSpace, int antiAliasing = 1);

	// Release the temporary buffer.
	void ReleaseTempBuffer (RenderTexture *rTex);	
	
	void GarbageCollect (int framesDelay = kKillFrames);
	
	void Cleanup ();

private:
	
	typedef std::set<PPtr<RenderTexture>, std::less< PPtr<RenderTexture> > , memory_pool<PPtr<RenderTexture> > > TakenTextures;
	typedef std::pair <int, PPtr<RenderTexture> > IntPPtrPair;
	typedef std::list<IntPPtrPair, memory_pool<IntPPtrPair > > FreeTextures;

	FreeTextures m_FreeTextures;
	TakenTextures m_TakenTextures;
	int m_TempBuffers;
	int m_CurrentRBMFrame;
};

RenderBufferManager& GetRenderBufferManager ();
RenderBufferManager* GetRenderBufferManagerPtr ();

#endif
