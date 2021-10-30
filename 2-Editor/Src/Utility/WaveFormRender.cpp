/*
 *  WaveFormRender.h
 *  AllTargets.workspace
 *
 *  Created by Søren Christiansen on 11/3/11.
 *  Copyright 2011 Unity Technologies. All rights reserved.
 *
 */
#include "WaveFormRender.h"

#if !UNITY_FLASH
#include "Editor/Src/AssetPipeline/FMODImporter.h"
#endif
#include "Runtime/Allocator/MemoryMacros.h"

static WaveFormRender *gWaveFormRender = NULL;

WaveFormRender::~WaveFormRender()
{
	m_RenderThread.WaitForExit(true);
}

Texture2D* WaveFormRender::RenderWaveFormAsync(const PPtr<AudioClip>& clip, const UInt32 fromSample, const UInt32 toSample, const UInt32 width, const UInt32 height)
{
	AssertBreak(width);
	
	Mutex::AutoLock lock(m_RenderQueueMutex);
	const SInt32 ID = clip->GetInstanceID();
	if (m_WaveForms.find(ID) != m_WaveForms.end())
	{
		// create texture
		RenderQueueItem& item =  m_WaveForms[ID];
		if (item.texture == NULL)
		{
			item.texture = CreateObjectFromCode<Texture2D>();
			item.texture->SetHideFlags(Object::kHideAndDontSave);
			item.texture->InitTexture (width, height, kTexFormatARGB32, Texture2D::kNoMipmap, 1);
			item.texture->SetPixels32( 0, item.pixels, width * height );
			
			item.texture->UpdateImageDataDontTouchMipmap();
			delete[] item.pixels;
			item.pixels = NULL;
		}
		
		return item.texture;
	}
	
	if (!m_RenderThread.IsRunning())
		m_RenderThread.Run(RenderLoop, this);
	
	bool inQueue = InQueue(clip);
	
	if (!inQueue)
	{
		// push to queue
		RenderQueueItem item;
		item.ID = ID;
		item.channelCount = clip->GetChannelCount();
		item.sampleCount = clip->GetSampleCount();
		item.texWidth = width;
		item.texHeight = height;
		item.texBPP = 4;
		// Allocate pixels in the render-thread. This way we don't lose the memory if we remove an item (ClearWaveForm)
		// before the item is transferred to 'm_WaveForms' map.
		item.pixels = NULL;
		item.texture = NULL;

		// get importer
		AudioImporter* importer;
		importer = dynamic_pptr_cast<AudioImporter*> (FindAssetImporterForObject(clip->GetInstanceID()));

#if !UNITY_FLASH
		// handling for non-persistent assets which does not have an importer e.g. asset bundle assets
		// @TODO: Should probably be moved away from main thread as some point
		if (importer == NULL)
		{
			dynamic_array<float> data(kMemAudio);
			const char * msg;
			AudioClip* clipPtr = clip;
			if (clipPtr != NULL && clipPtr->GetFrequency() > 0 && (msg = GeneratePreview(clipPtr->GetSound(), PREVIEW_WIDTH, data)) != NULL)
			{
				ErrorString(msg);

				// Dummy entry
				item.texture = CreateObjectFromCode<Texture2D>();
				item.texture->SetHideFlags(Object::kHideAndDontSave);
				item.texture->InitTexture (1, 1, kTexFormatARGB32, Texture2D::kNoMipmap, 1);
				item.texture->UpdateImageDataDontTouchMipmap();
				m_WaveForms[ID] = item;
				return item.texture;
			}
			data.set_owns_data(false);
			item.minMaxArray = data.begin();
			item.deleteMinMaxArray = true;
		}
		else
		{
			item.minMaxArray = importer->GetPreviewMinMaxData();			
			item.deleteMinMaxArray = false;
		}
#else
		item.minMaxArray = importer->GetPreviewMinMaxData();			
		item.deleteMinMaxArray = false;
#endif
		m_RenderQueue.push_back(item);
	}
	
	return NULL;
}

void WaveFormRender::ClearWaveForm(const PPtr<AudioClip>& clip)
{
	if (clip.IsNull())
		return;
	
	Mutex::AutoLock lock(m_RenderQueueMutex);
	if (InQueue(clip))
		RemoveFromQueue (clip);
	
	const SInt32 ID = clip->GetInstanceID();
	if (m_WaveForms.find(ID) != m_WaveForms.end())
	{
		RenderQueueItem& item = m_WaveForms[ID];
		if (item.texture)
			DestroySingleObject(item.texture);
		if (item.pixels)
			delete[] item.pixels;
		m_WaveForms.erase(ID);
	}
}

void* WaveFormRender::RenderLoop(void* parameters)
{
	WaveFormRender& waveformrender = *static_cast<WaveFormRender*> (parameters);
	while (true)
	{
		waveformrender.m_RenderQueueMutex.Lock();
		if (waveformrender.m_RenderQueue.empty())
		{
			waveformrender.m_RenderQueueMutex.Unlock();
			Thread::Sleep(0.1f);
		}
		else 
		{
			// Do not remove the item from queue here. We want to know if ClearWaveForm was called
			// while processing this waveform. See below
			RenderQueueItem& itemTemp = waveformrender.m_RenderQueue.front();
			itemTemp.pixels = new ColorRGBA32[itemTemp.texWidth * itemTemp.texHeight * itemTemp.texBPP];

			// Need to make a local copy as the item can be removed from the queue by the main thread.
			RenderQueueItem item = waveformrender.m_RenderQueue.front();

			// The main thread must not delete the minMaxArray now that we have begun processing it.
			itemTemp.deleteMinMaxArray = false;

			waveformrender.m_RenderQueueMutex.Unlock();
			
			WaveFormRender::RenderWaveForm(item, 0, item.sampleCount);
			
			// Free source data if needed
			if (item.deleteMinMaxArray && item.minMaxArray)
			{
				UNITY_FREE (kMemAudio, const_cast<float*>(item.minMaxArray));
				item.deleteMinMaxArray = false;
				item.minMaxArray = 0;
			}
			
			// The item may be removed from the queue while still rendering.
			// We could not release the 'pixels' memory while removing, so we're doing it here, if
			// the item isn't in the queue anymore.
			Mutex::AutoLock lock(waveformrender.m_RenderQueueMutex);

			if (waveformrender.InQueue (PPtr<AudioClip> (item.ID)))
			{
				// Also need to clean an item in m_WaveForms if its pixels data hasn't been used up by the main thread yet.
				// This probably never happens as waveforms are quickly removed from the queue after clip selection changes.
				RenderQueueItem& itemPlace = waveformrender.m_WaveForms[item.ID];
				delete []itemPlace.pixels;
				
				itemPlace = item;
				if(!waveformrender.m_RenderQueue.empty())
					waveformrender.m_RenderQueue.pop_front();
			}
			else
				delete[] item.pixels;
		}
	}
	
	return 0;
}


void WaveFormRender::RenderWaveForm(RenderQueueItem& item, const UInt32 fromSample, const UInt32 toSample)
{
	Assert (toSample > fromSample);
	
	UInt8* p = (UInt8*)item.pixels;
	
	const UInt32 sampleCount = toSample - fromSample;
	const UInt32 samplesPerPixel = sampleCount / item.texWidth;	
	
	UInt32 startSample = fromSample;
	for (UInt32 x = 0; x < item.texWidth; ++x)
	{	
		const float channelHeight = item.texHeight / (float)item.channelCount;
		
		for (int c = 0; c < item.channelCount; c++)
		{
			float min, max;				
			
			GetMinMax(startSample, startSample + samplesPerPixel, c, item.channelCount, sampleCount, item.minMaxArray, &min, &max);
			
			const float y = item.texHeight - channelHeight - c * channelHeight;
			const SInt32 y1 = std::max((SInt32)0, (SInt32)floorf(y));
			const SInt32 y2 = std::min((SInt32)item.texHeight, (SInt32)ceilf(y1 + channelHeight));
			const SInt32 ystart = (SInt32)floorf(y + channelHeight * (min * 0.5f + 0.5f));
			const SInt32 yend = (SInt32)ceilf(y + channelHeight * (max * 0.5f + 0.5f));		

			for (UInt32 channelY = y1; channelY < y2; ++channelY)
			{
				UInt32 off = channelY * (item.texWidth * item.texBPP);				
				
				if (channelY < ystart || channelY > yend)
				{
					p[off] =  p[off+1] = p[off+2] = p[off+3] = 0;
				}
				else
				{
					const UInt32 middleY = (y + channelHeight * 0.5f);
					const float fade = (abs((int)(channelY - middleY)) / channelHeight);
					
					p[off] = 255.f;
					p[off+1] = Lerp(140.f, 255.f, fade);		 
					p[off+2] = 0.f;
					p[off+3] = 255;	 
				}
			}
		}
		
		startSample += samplesPerPixel;
		p += item.texBPP;		
	}
	
	return;
}

void WaveFormRender::RemoveFromQueue(const PPtr<AudioClip>& clip)
{
	if (clip.IsNull())
		return;

	TRenderQueue::iterator it = m_RenderQueue.begin();

	for (; it != m_RenderQueue.end();)
	{
		if ((*it).ID == clip.GetInstanceID())
		{
			// Free source data if needed
			if (it->deleteMinMaxArray && it->minMaxArray)
			{
				UNITY_FREE (kMemAudio, const_cast<float*>(it->minMaxArray));
				it->deleteMinMaxArray = false;
				it->minMaxArray = 0;
			}

			it = m_RenderQueue.erase(it);
		}
		else {
			++it;
		}
	}
}

WaveFormRender &GetWaveFormRender ()
{
	if (gWaveFormRender == NULL)
		gWaveFormRender = new WaveFormRender ();
	return *gWaveFormRender;
}
