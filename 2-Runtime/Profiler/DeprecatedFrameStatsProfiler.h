#pragma once

struct FrameStats
{
	typedef signed long long Timestamp;
	
	void reset() {
		fixedBehaviourManagerDt = 0;
		fixedPhysicsManagerDt = 0;
		dynamicBehaviourManagerDt = 0;
		coroutineDt = 0;
		skinMeshUpdateDt = 0;
		animationUpdateDt = 0;
		renderDt = 0;
		
		fixedUpdateCount = 0;
	}
	
	Timestamp fixedBehaviourManagerDt;
	Timestamp fixedPhysicsManagerDt;
	Timestamp dynamicBehaviourManagerDt;
	Timestamp coroutineDt;
	Timestamp skinMeshUpdateDt;
	Timestamp animationUpdateDt;
	Timestamp renderDt;
	int fixedUpdateCount;
};
FrameStats const& GetFrameStats();