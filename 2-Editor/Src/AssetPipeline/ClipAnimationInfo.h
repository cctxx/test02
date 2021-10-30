#pragma once

#include "Editor/Src/Animation/AvatarMask.h"
#include "Runtime/Math/AnimationCurve.h"
#include "Editor/Src/AssetPipeline/ModelImporter.h" 
#include "Runtime/Animation/AnimationClipSettings.h" 

struct ClipAnimationInfoCurve
{
	UnityStr name;
	AnimationCurve curve;

	DEFINE_GET_TYPESTRING(ClipAnimationInfoCurve)
		
	template<class TransferFunction>
	void Transfer (TransferFunction& transfer)
	{
		transfer.Transfer (name, "name");
		TRANSFER(curve);
	}
};


struct ClipAnimationInfo
{
	enum MaskType
	{
		kCreateFromThisModel = 0,
		kCopyFromOther = 1,
		kCreateHumanoidFromThisModel = 2 // This one is deprecated
	};

	
	UnityStr name;
	UnityStr takeName;
	float  firstFrame;
	float  lastFrame;
	int    wrapMode;
	bool   loop;
	
	float	orientationOffsetY;
	float	level;
	float	cycleOffset;
		
	bool	loopTime;
	bool	loopBlend;
	bool	loopBlendOrientation;
	bool	loopBlendPositionY;
	bool	loopBlendPositionXZ;
	bool	keepOriginalOrientation;
	bool	keepOriginalPositionY;
	bool	keepOriginalPositionXZ;
	bool	heightFromFeet;
	bool	mirror;

	MaskType			maskType;
	PPtr<AvatarMask>	maskSource;

	std::vector<UInt32> bodyMask;
	std::vector<ClipAnimationInfoCurve> curves;

	std::vector<AnimationEvent> events;

	AvatarMask::ElementList transformMask;
	
	DEFINE_GET_TYPESTRING (ClipAnimationInfo)
	
	ClipAnimationInfo ()
	{
		firstFrame = 0;
		lastFrame = 0;
		wrapMode = 0;
		loop = false;
		
		orientationOffsetY = 0;
		level = 0;
		cycleOffset = 0;
		
		loopTime = false;
		loopBlend = false;
		loopBlendOrientation = false;
		loopBlendPositionY = false;
		loopBlendPositionXZ = false;
		keepOriginalOrientation = false;
		keepOriginalPositionY = true;
		keepOriginalPositionXZ = false;
		heightFromFeet = false;
		mirror = false;
		maskType = kCreateFromThisModel;
		
		for(int i = 0; i < kLastMaskBodyPart; i++) bodyMask.push_back(1);
	}
	
	bool operator==(ClipAnimationInfo const& other)const
	{
		return name == other.name && takeName == other.takeName && 
				firstFrame == other.firstFrame &&
				lastFrame == other.lastFrame &&
				wrapMode == other.wrapMode &&
				loop == other.loop &&			
				orientationOffsetY == other.orientationOffsetY &&
				level == other.level &&
				cycleOffset == other.cycleOffset &&			
				loopTime == other.loopTime &&
				loopBlend == other.loopBlend &&
				loopBlendOrientation == other.loopBlendOrientation &&
				loopBlendPositionY == other.loopBlendPositionY &&
				loopBlendPositionXZ == other.loopBlendPositionXZ &&
				keepOriginalOrientation == other.keepOriginalOrientation &&
				keepOriginalPositionY == other.keepOriginalPositionY &&
				keepOriginalPositionXZ == other.keepOriginalPositionXZ &&
				heightFromFeet == other.heightFromFeet &&
				mirror == other.mirror &&
				maskType == other.maskType &&
				maskSource == other.maskSource;
	}
	
	template<class TransferFunction>
	void Transfer (TransferFunction& transfer)
	{
		transfer.SetVersion(16);

		transfer.Transfer (name, "name", kTransferAsArrayEntryNameInMetaFiles);
		transfer.Transfer (takeName, "takeName");
		TRANSFER(firstFrame);
		TRANSFER(lastFrame);
		TRANSFER(wrapMode);
		TRANSFER(orientationOffsetY);
		TRANSFER(level);
		TRANSFER(cycleOffset);
		TRANSFER(loop);

		TRANSFER(loopTime);
		TRANSFER(loopBlend);

		loopTime = transfer.IsVersionSmallerOrEqual(15) ? loopBlend : loopTime;

		TRANSFER(loopBlendOrientation);
		TRANSFER(loopBlendPositionY);
		TRANSFER(loopBlendPositionXZ);
		TRANSFER(keepOriginalOrientation);
		TRANSFER(keepOriginalPositionY);
		TRANSFER(keepOriginalPositionXZ);
		TRANSFER(heightFromFeet);
		TRANSFER(mirror);
		transfer.Align();
		TRANSFER(bodyMask);
		TRANSFER(curves);
		TRANSFER(events);
		TRANSFER(transformMask);
		TRANSFER_ENUM(maskType);
		
		if(maskType == kCreateHumanoidFromThisModel)
			maskType = kCreateFromThisModel;

		TRANSFER(maskSource);
	}

	void ToAnimationClipSettings(AnimationClipSettings &info, float sampleRate) const
	{
		info.m_StartTime = 0.0F;
		info.m_StopTime = (lastFrame - firstFrame) / sampleRate;
		
		info.m_OrientationOffsetY = orientationOffsetY;
		info.m_Level = level;
		info.m_CycleOffset = cycleOffset;
		
		info.m_LoopTime = loopTime;
		info.m_LoopBlend = loopBlend;
		info.m_LoopBlendOrientation = loopBlendOrientation;
		info.m_LoopBlendPositionY = loopBlendPositionY;
		info.m_LoopBlendPositionXZ = loopBlendPositionXZ;
		info.m_KeepOriginalOrientation = keepOriginalOrientation;
		info.m_KeepOriginalPositionY = keepOriginalPositionY;
		info.m_KeepOriginalPositionXZ = keepOriginalPositionXZ;
		info.m_HeightFromFeet = heightFromFeet;
		info.m_Mirror = mirror;
	}
};
