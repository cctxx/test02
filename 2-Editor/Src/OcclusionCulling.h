#pragma once

#include "Runtime/Math/Vector3.h"
#include "Runtime/Utilities/dynamic_array.h"
#include "Editor/Src/Gizmos/GizmoVertexFormats.h"
#include "SceneBackgroundTask.h"
#include "Runtime/Profiler/TimeHelper.h"
#include "Runtime/BaseClasses/BaseObject.h"

namespace Umbra { class Task; class Scene; }
namespace Unity { class Material; }
class Renderer;
class OcclusionPortal;

class OcclusionCullingTask;
template<class T> class PPtr;
struct AnalyticsProcessTracker;

class OcclusionCullingTask : SceneBackgroundTask
{
	Umbra::Task*                          m_Task;
	dynamic_array<PPtr<Renderer> >        m_StaticRenderers;
	dynamic_array<PPtr<OcclusionPortal> > m_Portals;
	AnalyticsProcessTracker*              m_Analytics;

	static dynamic_array<gizmos::ColorVertex>	s_lines;
	static ABSOLUTE_TIME						s_lastTime;

	public:

	static bool GenerateTome ();
	static bool GenerateTomeInBackground ();
	
    static void GetUmbraPreVisualization (dynamic_array<gizmos::ColorVertex>& lines);
	
	static void ClearUmbraTome ();
	
	
	static bool IsRunning () { return IsTaskRunning(kOcclusionBuildingTask); }
	static void Cancel ()    { CleanupSingletonBackgroundTask(kOcclusionBuildingTask); }

    static bool  DoesSceneHaveManualPortals ();

	static void UpdateOcclusionCullingProgressVisualization(Umbra::Task* Task);
	
	void SetUmbraTomeSize (int size);
	
	virtual float GetProgress ();
	virtual std::string GetStatusText ()     { return "Computing Occlusion"; }
	virtual std::string GetDialogTitle ()    { return "Computing Occlusion"; }

	virtual void        BackgroundStatusChanged ();
	virtual bool        IsFinished ();
	virtual bool        Complete ();
	virtual SceneBackgroundTaskType GetTaskType () { return kOcclusionBuildingTask; }
	
	protected:

	static OcclusionCullingTask* CreateUmbraTask ();

	~OcclusionCullingTask ();

};

bool IsTransparentOrCutoutMaterial (Unity::Material& material);
bool IsTransparentOrCutoutRenderer (Renderer& renderer);
