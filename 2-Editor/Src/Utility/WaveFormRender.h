/*
 *  WaveFormRender.h
 *  AllTargets.workspace
 *
 *  Created by Søren Christiansen on 11/3/11.
 *  Copyright 2011 Unity Technologies. All rights reserved.
 *
 */
#pragma once

#if UNITY_EDITOR

#include "Runtime/Audio/AudioClip.h"
#include "Runtime/Graphics/Texture2D.h"
#include "Runtime/Threads/Thread.h"
#include "Runtime/Threads/Mutex.h"

#include "Editor/Src/AssetPipeline/AudioImporter.h"

#include <map>
#include <deque>

class WaveFormRender {
public:
	WaveFormRender() {}
	~WaveFormRender();
	
	Texture2D* RenderWaveFormAsync(const PPtr<AudioClip>& clip, const UInt32 fromSample, const UInt32 toSample, const UInt32 width, const UInt32 height);	
	void ClearWaveForm(const PPtr<AudioClip>& clip);
	
private:
	struct RenderQueueItem {
		SInt32 ID;
 		
		UInt16       channelCount;
		UInt32       sampleCount;
		const float* minMaxArray;	
		bool         deleteMinMaxArray;
		UInt32       texWidth;
		UInt32       texHeight;
		UInt16       texBPP;
		
		ColorRGBA32* pixels;
		Texture2D*   texture;
	};
	
private:
	std::map<SInt32, RenderQueueItem> m_WaveForms;
	
	mutable Mutex m_RenderQueueMutex;
	typedef std::deque<RenderQueueItem> TRenderQueue;
	TRenderQueue m_RenderQueue;
		
	Thread m_RenderThread;	
private:
	inline bool InQueue(const PPtr<AudioClip>& clip) const;
	void RemoveFromQueue(const PPtr<AudioClip>& clip);
	static inline void GetMinMax(UInt32 fromSample, UInt32 toSample, UInt16 channel, UInt16 channels, UInt32 totalSamples, const float* previewData, float* min, float* max);	
	static void* RenderLoop(void* parameters);
	static void RenderWaveForm(RenderQueueItem& item, const UInt32 fromSample, const UInt32 toSample);
};

WaveFormRender& GetWaveFormRender ();

inline bool WaveFormRender::InQueue(const PPtr<AudioClip>& clip) const
{
	bool inQueue = false;
	
	TRenderQueue::const_iterator it = m_RenderQueue.begin();
	
	for (; it != m_RenderQueue.end(); ++it)
	{
		if ((*it).ID == clip.GetInstanceID())
		{
			inQueue = true;
			break;
		}
	}
	
	return inQueue;
}

inline void WaveFormRender::GetMinMax(UInt32 fromSample, UInt32 toSample, UInt16 channel, UInt16 channels, UInt32 totalSamples, const float* previewData, float* min, float* max)	
{
	UInt32 fromIdx = fromSample * (PREVIEW_WIDTH / (float)totalSamples);
	UInt32 toIdx = toSample * (PREVIEW_WIDTH / (float)totalSamples);
	
	*max = (previewData[(fromIdx * channels * 2) + (channel * 2) ] + previewData[(toIdx * channels * 2) + (channel * 2) ] ) / 2;				   
	*min = (previewData[(fromIdx * channels * 2) + (channel * 2) + 1] + previewData[(toIdx * channels * 2) + (channel * 2) + 1] ) / 2;
	
	return;	
}

#endif // UNITY_EDITOR