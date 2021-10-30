#include "UnityPrefix.h"

#if UNITY_EDITOR

#include "DebugUtility.h"
#include "Editor/Src/Gizmos/GizmoUtil.h"
#include "Runtime/Math/Color.h"
#include "Runtime/Math/Vector3.h"
#include "Runtime/Input/TimeManager.h"
#include "Runtime/BaseClasses/IsPlaying.h"
#include "Editor/Src/Gizmos/GizmoManager.h"
#include "Editor/Src/Gizmos/GizmoRenderer.h"
#include <vector>
#include "Runtime/Utilities/InitializeAndCleanup.h"

using namespace std;

class DebugLineRenderer 
{
public:

	void AddLine (const Vector3f& v0, const Vector3f& v1, const ColorRGBAf& color, double durationSeconds, bool depthTest)
	{
		// We do not support timed debug lines in edit mode
		if (!IsWorldPlaying ())
			durationSeconds = 0.0;

		Line l;
		l.start = v0; 
		l.end = v1; 
		l.color = color;
		l.depthTest = depthTest;
		if (durationSeconds > 0.0)
		{
			// We use game time (CurTime) to be able to pause game and investigate debug lines.
			l.removeTime = GetTimeManager ().GetCurTime () + durationSeconds;  
			m_TimedLines.push_back (l);
		}
		else
		{
			l.removeTime = -1.0;
			if (GetTimeManager ().IsUsingFixedTimeStep ())
				m_FixedStepLines.push_back (l);
			else
				m_DynamicStepLines.push_back (l);
		}
	}

	void Clear ()
	{
		if (GetTimeManager ().IsUsingFixedTimeStep ())
		{
			m_FixedStepLines.clear ();
		}
		else
		{
			m_DynamicStepLines.clear ();

			if (!m_TimedLines.empty())
			{
				float curTime = GetTimeManager ().GetCurTime ();
				list<Line>::iterator it = m_TimedLines.begin();
				while (it != m_TimedLines.end())
				{
					if (curTime >= it->removeTime)
						it = m_TimedLines.erase (it);
					else
						it++;
				}
			}
		}
	}

	void ClearAll ()
	{
		m_DynamicStepLines.clear ();
		m_FixedStepLines.clear ();
		m_TimedLines.clear ();
	}

	void Draw () const
	{
		for (unsigned i=0; i<m_FixedStepLines.size(); ++i)
		{
			const Line& line = m_FixedStepLines[i];
			gizmos::g_GizmoColor = line.color;
			DrawLine(line.start, line.end, line.depthTest);
		}
		for (unsigned i=0; i<m_DynamicStepLines.size(); ++i)
		{
			const Line& line = m_DynamicStepLines[i];
			gizmos::g_GizmoColor = line.color;
			DrawLine(line.start, line.end, line.depthTest);
		}

		for (list<Line>::const_iterator it = m_TimedLines.begin(); it != m_TimedLines.end(); it++)
		{
			const Line& line = *it;
			gizmos::g_GizmoColor = line.color;
			DrawLine(line.start, line.end, line.depthTest);
		}
	}
	static DebugLineRenderer& Get ();
	static void StaticDestroy();
private:	
	struct Line
	{
		Vector3f start;
		Vector3f end;
		ColorRGBAf color;
		double removeTime;
		bool depthTest;
	};
	list<Line> m_TimedLines;
	vector<Line> m_FixedStepLines;
	vector<Line> m_DynamicStepLines;

};

static RegisterRuntimeInitializeAndCleanup gDebugLineRendererInstance(NULL, DebugLineRenderer::StaticDestroy);
static DebugLineRenderer* g_DebugLineRenderer = NULL;

void DebugLineRenderer::StaticDestroy()
{
	if(g_DebugLineRenderer)
		UNITY_DELETE(g_DebugLineRenderer, kMemEditorUtility);
}

DebugLineRenderer& DebugLineRenderer::Get ()
{
	if(g_DebugLineRenderer == NULL)
		g_DebugLineRenderer = UNITY_NEW_AS_ROOT(DebugLineRenderer,kMemEditorUtility, "Manager", "DebugLineRenderer");
	return *g_DebugLineRenderer;
}

void ClearLines ()
{
	DebugLineRenderer::Get ().Clear ();
}

void ClearAllLines ()
{
	DebugLineRenderer::Get ().ClearAll ();
}

void DrawDebugLinesGizmo ()
{	
	DebugLineRenderer::Get ().Draw ();
}

void DebugDrawLine (const Vector3f& p0, const Vector3f& p1, const ColorRGBAf& color, double durationSeconds /*=0.0*/, bool depthTest /*=true*/)
{
	DebugLineRenderer::Get ().AddLine (p0, p1, color, durationSeconds, depthTest);
}

#endif
