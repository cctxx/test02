#ifndef AVATARPLAYBACK_H
#define AVATARPLAYBACK_H

#include "Runtime/Animation/MecanimUtility.h"

namespace mecanim 
{
	namespace animation 
	{ 
		struct AvatarMemory ; 
	} 
} 

class AvatarFrame
{
public:
	AvatarFrame() : m_AvatarMemory(0), m_CurrentTime(0) {}
	mecanim::animation::AvatarMemory* m_AvatarMemory;
	float m_CurrentTime;
};

class AvatarPlayback
{
public:

	AvatarPlayback(MemLabelId label);
	
	void Clear();
	mecanim::animation::AvatarMemory* PlayFrame(float time, float& effectiveTime);
	void RecordFrame(float deltaTime, const mecanim::animation::AvatarMemory* srcMemory);
	
	void Init(int frameCount);
	
	float StartTime();
	float StopTime();
	float CursorTime();


private:
	std::vector<AvatarFrame> m_Frames;

	int m_FrameCount;
	int m_StartIndex;
	int m_StopIndex;
	int m_CursorIndex;

	int NextIndex(int index);
	mecanim::memory::MecanimAllocator m_Alloc;
};

#endif // AVATARPLAYBACK_H
