#ifndef TIMEMANAGER_H
#define TIMEMANAGER_H

#include "Runtime/BaseClasses/GameManager.h"
#include "Configuration/UnityConfigure.h"
#include "Runtime/ClusterRenderer/ClusterRendererDefines.h"

class TimeManager;

EXPORT_COREMODULE TimeManager& GetTimeManager ();
/// The time since startup
double GetTimeSinceStartup ();
double TimeSinceStartupImpl ();



/* 
Time requirements:
- Delta time shall never be less than kMinimumDeltaTime (So people dont get nans when dividing by delta time)
- The first frame when starting up is always at zero and kStartupDeltaTime delta time.
- delta time is clamped to a maximum of kMaximumDeltaTime
- adding up delta time always gives you the current time

- after loading a level or pausing, the first frames delta time is kStartupDeltaTime
- fixed delta time is always smaller or equal to dynamic time

- When starting up there is always one physics frame before the first display!
*/

class TimeManager : public GlobalGameManager
{
	public:

	struct TimeHolder
	{
		TimeHolder();
		double m_CurFrameTime;
		double m_LastFrameTime;
		float m_DeltaTime;
		float m_SmoothDeltaTime;
		float m_SmoothingWeight;
		float m_InvDeltaTime;
	};
	
	TimeManager (MemLabelId label, ObjectCreationMode mode);
	// ~TimeManager (); declared-by-macro

	REGISTER_DERIVED_CLASS (TimeManager, GlobalGameManager)
	DECLARE_OBJECT_SERIALIZE (TimeManager)
	// for cluster renderer
	DECLARE_CLUSTER_SERIALIZE(TimeManager)
	
	void AwakeFromLoad (AwakeFromLoadMode mode);
	virtual void CheckConsistency ();

	virtual void Update ();
	
	#if UNITY_EDITOR
	/// Called from the editor to bump the frameCount. Later, this would also update GraphicsTime,
	/// So we can get animations from the editor.
	void NextFrameEditor ();
	#endif

	void ResetTime ();
	
	void SetPause(bool pause);

	
//	void SetMinimumDeltaTime (float c) { m_MinimumDeltaTime = c; }
	
	inline double 	GetCurTime ()  const		{ return m_ActiveTime.m_CurFrameTime; }
	inline double 	GetTimeSinceLevelLoad ()  const		{ return m_ActiveTime.m_CurFrameTime + m_LevelLoadOffset; }
	inline float 	GetDeltaTime () const		{ return m_ActiveTime.m_DeltaTime; }
	inline float 	GetSmoothDeltaTime ()  const	{ return m_ActiveTime.m_SmoothDeltaTime; }

	inline float 	GetInvDeltaTime () const	 	{ return m_ActiveTime.m_InvDeltaTime; }
	inline int	 	GetFrameCount () const		{ return m_FrameCount; }

	inline int 		GetRenderFrameCount () const { return m_RenderFrameCount; }

	inline float	GetFixedDeltaTime () {return m_FixedTime.m_DeltaTime; }
	void	        SetFixedDeltaTime (float fixedStep);
	inline double	GetFixedTime () {return m_FixedTime.m_CurFrameTime; }
	
	inline float	GetMaximumDeltaTime () {return m_MaximumTimestep; }
	void			SetMaximumDeltaTime (float maxStep);

	
	/// Steps the fixed time step until the dynamic time is exceeded.
	/// Returns true if it has to be called again to reach the dynamic time
	bool StepFixedTime ();
	
	void SetTimeManually (bool manually) { m_SetTimeManually = manually; }
	void SetTime (double time);
	void SetDeltaTimeHack (float dt);
	void SetTimeScale (float scale);
	float GetTimeScale () { return m_TimeScale; }
	
	inline bool IsUsingFixedTimeStep () const { return m_UseFixedTimeStep; }

	void DidFinishLoadingLevel ();

	void SetCaptureFramerate (int rate) { m_CaptureFramerate = rate; }
	int GetCaptureFramerate () { return m_CaptureFramerate; }

	double GetRealtime();

	void Sync(float framerate);

	#if SUPPORT_REPRODUCE_LOG
	void WriteLog (std::ofstream& out);
	void ReadLog (std::ifstream& in);
	#endif

	private:
	
	TimeHolder  m_FixedTime;
	TimeHolder  m_DynamicTime;
	TimeHolder  m_ActiveTime;

	bool        m_FirstFrameAfterReset;
	bool        m_FirstFrameAfterPause;
	bool        m_FirstFixedFrameAfterReset;

	int		    m_FrameCount;	
	int		    m_RenderFrameCount;
	int			m_CullFrameCount;
	int			m_CaptureFramerate;
	double      m_ZeroTime;
	double      m_RealZeroTime;
	double      m_LevelLoadOffset;
	double      m_RealtimeStartOfFrame;
	
	bool        m_SetTimeManually;
	bool        m_UseFixedTimeStep;
	float       m_TimeScale;///< How fast compared to the real time does the game time progress (1.0 is realtime, .5 slow motion) range { -infinity, 100 }
	float		m_MaximumTimestep;

	double      m_LastSyncEnd;
};

inline double	GetCurTime () 	{ return GetTimeManager ().GetCurTime (); }
inline float	GetDeltaTime () 		{ return GetTimeManager ().GetDeltaTime (); }
inline float	GetInvDeltaTime () 	{ return GetTimeManager ().GetInvDeltaTime (); }
float CalcInvDeltaTime (float dt);

const float kMinimumDeltaTime = 0.00001F;

#endif
