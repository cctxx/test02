#pragma once

// --------------------------------------------------------------------------


struct Physics2DStats;


// --------------------------------------------------------------------------


class IPhysics2D
{
public:	
	virtual void FixedUpdate () = 0;
	virtual void DynamicUpdate () = 0;
	virtual void ResetInterpolations () = 0;

#if ENABLE_PROFILER
	virtual void GetProfilerStats (Physics2DStats& stats) = 0;
#endif
};


// --------------------------------------------------------------------------


IPhysics2D* GetIPhysics2D ();
void SetIPhysics2D (IPhysics2D* manager);
