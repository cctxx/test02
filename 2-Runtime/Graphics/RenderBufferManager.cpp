#include "UnityPrefix.h"
#include "RenderBufferManager.h"
#include "Runtime/Camera/Camera.h"
#include "Runtime/Camera/CameraUtil.h"
#include "Runtime/Camera/RenderManager.h"
#include "Runtime/GfxDevice/GfxDevice.h"
#include "RenderTexture.h"
#include "Runtime/Utilities/BitUtility.h"
#include "Runtime/Profiler/Profiler.h"
#if UNITY_XENON
	#include "PlatformDependent/Xbox360/Source/GfxDevice/TexturesXenon.h"
#elif UNITY_WII
	#include "Runtime/Graphics/ScreenManager.h"	
#endif

#ifndef DEBUG_RB_MANAGER
#define DEBUG_RB_MANAGER 0
#endif

using namespace std;

static RenderBufferManager* gRenderBufferManager = NULL;

void RenderBufferManager::InitRenderBufferManager ()
{
	Assert(gRenderBufferManager == NULL);
	gRenderBufferManager = new RenderBufferManager();
}

void RenderBufferManager::CleanupRenderBufferManager ()
{
	Assert(gRenderBufferManager != NULL);
	delete gRenderBufferManager;
	gRenderBufferManager = NULL;
}

RenderBufferManager& GetRenderBufferManager ()
{
	Assert(gRenderBufferManager != NULL);
	return *gRenderBufferManager;
}

RenderBufferManager* GetRenderBufferManagerPtr ()
{
	return gRenderBufferManager;
}


static int CalcSize( int size, int parentSize )
{
	switch (size) {
	case RenderBufferManager::kFullSize:
		return parentSize;
	default:
		#if DEBUGMODE
		if( size <= 0 ) {
			AssertString ("Invalid Temp Buffer size");
			return 128;
		}
		#endif
		return size;
	}
}


RenderTexture *RenderBufferManager::GetTempBuffer (int width, int height, DepthBufferFormat depthFormat, RenderTextureFormat colorFormat, UInt32 flags, RenderTextureReadWrite colorSpace, int antiAliasing)
{
	if( colorFormat == kRTFormatDefault )
		colorFormat = GetGfxDevice().GetDefaultRTFormat();

	if( colorFormat == kRTFormatDefaultHDR )
		colorFormat = GetGfxDevice().GetDefaultHDRRTFormat();

	bool sRGB = colorSpace == kRTReadWriteSRGB;
	const bool createdFromScript = flags & kRBCreatedFromScript;
	const bool sampleOnlyDepth = flags & kRBSampleOnlyDepth;
	const TextureDimension dim = (flags & kRBCubemap) ? kTexDimCUBE : kTexDim2D;

	if (colorSpace == kRTReadWriteDefault)
		sRGB = GetActiveColorSpace() == kLinearColorSpace;

	// only SRGB where it makes sense
	sRGB = sRGB && (colorFormat != GetGfxDevice().GetDefaultHDRRTFormat());

	if( width <= 0 || height <= 0 )
	{
		if (dim != kTexDim2D) {
			AssertString( "Trying to get a relatively sized RenderBuffer cubemap" );
			return NULL;
		}
		Camera *cam = GetCurrentCameraPtr();
		if (cam == NULL) {
			AssertString ("Trying to get a relatively sized RenderBuffer without an active camera.");
			return NULL;
		}
		Rectf r = cam->GetScreenViewportRect();
#if UNITY_WII
		GetScreenManager().ScaleViewportToFrameBuffer(r);
#endif
		// Figure out pixel size. Get screen extents as ints so we do the rounding correctly.
		int viewport[4];
		RectfToViewport(r, viewport);
		width = viewport[2];
		height = viewport[3];
	}
	
	if (dim == kTexDimCUBE && (!IsPowerOfTwo(width) || width != height))
	{
		AssertString( "Trying to get a non square or non power of two RenderBuffer cubemap" );
		return NULL;
	}

	if (antiAliasing < 1 || antiAliasing > 8 || !IsPowerOfTwo(antiAliasing))
	{
		AssertString( "Trying to get RenderBuffer with invalid antiAliasing (must be 1, 2, 4 or 8)" );
		return NULL;
	}
	
	// TODO: actually set & check depth

	// Go over free textures & find the one that matches in parameters.
	// The main point is: If we used a texture of same dims last frame we'll get that.
	FreeTextures::iterator found = m_FreeTextures.end();
	for( FreeTextures::iterator i = m_FreeTextures.begin(); i != m_FreeTextures.end(); ++i )
	{
		RenderTexture* rt = i->second;
		if( !rt 
		   || rt->GetDepthFormat() != depthFormat 
		   || rt->GetColorFormat() != colorFormat 
		   || rt->GetDimension() != dim
		   //@TODO: Only matters on OSX as DX can just set sampler state...
		   || rt->GetSRGBReadWrite() != sRGB
		   || rt->GetAntiAliasing() != antiAliasing
		   || rt->GetSampleOnlyDepth() != sampleOnlyDepth )
			continue;
		int tw = rt->GetWidth();
		int th = rt->GetHeight();
		
		if (tw == width && th == height) { 	// If the texture is same size
			found = i;
			break;
		}
	}
	
	// We didn't find any.
	if (found == m_FreeTextures.end() || !found->second)
	{
		m_TempBuffers++;
		RenderTexture *tex = NEW_OBJECT (RenderTexture);
		tex->Reset();

		tex->SetHideFlags(Object::kDontSave);
		tex->SetName (Format ("TempBuffer %d", m_TempBuffers).c_str());
		tex->SetWidth(width);
		tex->SetHeight(height);
		tex->SetColorFormat( colorFormat );
		tex->SetDepthFormat( depthFormat );
		tex->SetDimension (dim);
		tex->SetSRGBReadWrite (sRGB);
		tex->SetAntiAliasing (antiAliasing);
		tex->SetSampleOnlyDepth (sampleOnlyDepth);
		tex->AwakeFromLoad(kInstantiateOrCreateFromCodeAwakeFromLoad);
		m_TakenTextures.insert (tex);
		#if DEBUG_RB_MANAGER
		printf_console ("RBM: new texture %ix%i fmt=%i\n", width, height, colorFormat);
		#endif
		return tex;
	}

	// We found one. Move it from free to taken
	RenderTexture *tex = found->second;
	Assert (tex->GetWidth() == width && tex->GetHeight() == height);
	m_TakenTextures.insert (tex);
	m_FreeTextures.erase (found);

	// Set it's parameters (filtering etc.) as if it was newly created
	tex->GetSettings().Reset();
	tex->GetSettings().m_WrapMode = kTexWrapClamp;
	tex->ApplySettings();

	tex->SetCreatedFromScript (createdFromScript);

	// Automatically DiscardContents when createdFromScript - user can't expect any valid content.
	if (createdFromScript)
		tex->DiscardContents();

	return tex;
}

PROFILER_INFORMATION(gRenderBufferCollect, "RenderTexture.GarbageCollectTemporary", kProfilerRender)

void RenderBufferManager::GarbageCollect (int framesDelay)
{
	++m_CurrentRBMFrame;
	
	for (FreeTextures::iterator i = m_FreeTextures.begin(); i != m_FreeTextures.end();)
	{
		// Should only ever compare the difference since frame wraps around
		int frameDiff = m_CurrentRBMFrame - i->first;
		if( frameDiff > framesDelay || frameDiff < 0 )
		{
			PROFILER_AUTO(gRenderBufferCollect, NULL);
			#if DEBUG_RB_MANAGER
			printf_console ("RBM: kill unused texture (currframe=%i usedframe=%i)\n", m_CurrentRBMFrame, i->first);
			#endif

			FreeTextures::iterator j = i;
			i++;
			DestroySingleObject(j->second);
			m_FreeTextures.erase (j);
		}
		else
		{
			i++;
		}
	}
}


void RenderBufferManager::Cleanup ()
{
	for (TakenTextures::iterator i=m_TakenTextures.begin();i != m_TakenTextures.end();i++)
	{
		DestroySingleObject(*i);
	}
	m_TakenTextures.clear();

	for (FreeTextures::iterator i=m_FreeTextures.begin();i != m_FreeTextures.end();i++)
	{
		DestroySingleObject(i->second);
	}
	m_FreeTextures.clear();
	#if DEBUG_RB_MANAGER
	printf_console( "RBM: destroy all textures\n" );
	#endif
}
	
void RenderBufferManager::ReleaseTempBuffer (RenderTexture *rTex)
{
	if (!rTex)
		return;
	
	if (!m_TakenTextures.count (PPtr<RenderTexture> (rTex)))
	{
		ErrorStringObject ("Attempting to release RenderTexture that were not gotten as a temp buffer", rTex);
		return;
	}

	m_TakenTextures.erase (PPtr<RenderTexture> (rTex));
	m_FreeTextures.push_back (make_pair (m_CurrentRBMFrame, PPtr<RenderTexture> (rTex)));
}
