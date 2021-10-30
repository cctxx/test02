#include "UnityPrefix.h"

#include "AvatarPlayback.h"

#include "Runtime/mecanim/animation/avatar.h"
#include "Runtime/Serialize/Blobification/BlobWrite.h"
#include "Runtime/Animation/MecanimUtility.h"
#include "Runtime/Utilities/LogAssert.h"

enum  { kMaxFrameCount = 10000};

AvatarPlayback::AvatarPlayback(MemLabelId label) 
:	m_Alloc(label),
	m_FrameCount(-1),
	m_CursorIndex(-1),
	m_StartIndex(-1),
	m_StopIndex(-1)
{
}

void AvatarPlayback::Init(int frameCount)
{
	Clear();
	if(frameCount > 0 )
	{
		if(frameCount > kMaxFrameCount )
			WarningString("Could not allocate requested frameCount for Animator Recording. 10000 frames where allocated.");
		
		m_Frames.resize(min<unsigned int>(frameCount,kMaxFrameCount));	
		m_FrameCount = m_Frames.size();		
	}
	else 
	{
		m_FrameCount = 0 ;
	}
	
	m_CursorIndex = -1;
	m_StartIndex = -1 ;
	m_StopIndex = -1;
}

void AvatarPlayback::Clear()
{
	for(int i = 0 ; i < m_Frames.size() ; i++)
	{
		mecanim::animation::DestroyAvatarMemory(m_Frames[i].m_AvatarMemory, m_Alloc);
	}
	m_Frames.clear();
}

int AvatarPlayback::NextIndex(int index)
{
	return m_FrameCount > 0 ? (index+1)%m_FrameCount : index+1;
}

mecanim::animation::AvatarMemory* AvatarPlayback::PlayFrame(float time, float &effectiveTime)
{	
	int frameIndex = m_StopIndex;
	bool  found = false;

	if(m_StartIndex == -1)
		return 0;

	int i = m_StartIndex;
	int prevIndex = i;

	int endIndex = NextIndex(m_StopIndex);
	do // at this point, we have a least one frame recorded
	{
		if(m_Frames[i].m_CurrentTime > time)
		{
			frameIndex = prevIndex;
			found = true;
		}

		prevIndex = i;
		i = NextIndex(i);
	} while(i != endIndex && !found);
	

	effectiveTime = m_Frames[frameIndex].m_CurrentTime;
	m_CursorIndex = frameIndex;
	
	return m_Frames[frameIndex].m_AvatarMemory;	
}

void AvatarPlayback::RecordFrame(float deltaTime, const mecanim::animation::AvatarMemory* srcMemory)
{
	if(m_FrameCount == -1)
	{
		WarningString("Could not record Animator. Frame allocation has failed.");
		return;
	}

	AvatarFrame newFrame;
	if(m_StartIndex != -1)
		newFrame.m_CurrentTime = m_Frames[m_CursorIndex].m_CurrentTime + deltaTime;

	size_t size=0;
	newFrame.m_AvatarMemory = CopyBlob( *srcMemory, m_Alloc, size);
	
	m_CursorIndex = NextIndex(m_CursorIndex);
	if(m_StartIndex == m_CursorIndex|| m_StartIndex == -1) // increment startIndex when the cursor is writing on it (the buffer is full)
	{
		if(m_StartIndex != -1)
			mecanim::animation::DestroyAvatarMemory(m_Frames[m_CursorIndex].m_AvatarMemory, m_Alloc);
		m_StartIndex = NextIndex(m_StartIndex);
	}
	m_StopIndex = m_CursorIndex;
	if(m_FrameCount > 0)
		m_Frames[m_CursorIndex] = newFrame;
	else
		m_Frames.push_back(newFrame);
}

float AvatarPlayback::CursorTime() {return m_CursorIndex != -1 ? m_Frames[m_CursorIndex].m_CurrentTime : -1;}
float AvatarPlayback::StartTime() {return m_CursorIndex != -1 ? m_Frames[m_StartIndex].m_CurrentTime  : -1;}
float AvatarPlayback::StopTime() {return m_CursorIndex != -1 ? m_Frames[m_StopIndex].m_CurrentTime  : -1;}
