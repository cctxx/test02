#include "UnityPrefix.h"
#include "TooltipManager.h"
#include "Editor/Platform/Interface/EditorWindows.h"
#include "Runtime/IMGUI/GUIManager.h"
#include "Runtime/BaseClasses/IsPlaying.h"
#include "Runtime/Misc/InputEvent.h"
#include <limits>

#define TOOLTIP_TIME .4f
#define TOOLTIP_FADEINSPEED 1000.0f
#define TOOLTIP_FADEOUTSPEED 2.0f



double GetTimeSinceStartup ();

static TooltipManager *s_TooltipManager = NULL;

TooltipManager &GetTooltipManager ()
{
	
	if (!s_TooltipManager)
		s_TooltipManager = new TooltipManager;
	
	return *s_TooltipManager;
}

TooltipManager::TooltipManager() :
	m_NextUpdate(std::numeric_limits<double>::infinity()),
	m_LastMousePos(Vector2f(0,0)),
	m_LastTime (0.0f),
	m_CurrentAlpha (0.0f),
	m_TargetAlpha (0.0f),
	m_TooltipRect (0.0f,0.0f,0.0f,0.0f),
	m_Tooltip() {}

void TooltipManager::Update ()
{
	if (IsWorldPlaying())
	{
		if (m_CurrentAlpha != 0.0f)
		{
			CallStaticMonoMethod("TooltipView", "Close");
			m_CurrentAlpha = m_TargetAlpha = 0.0f;
		}
		return;
	}
	
	Vector2f mousePos = GetMousePosition();
	// find out if we should scan for tooltips:
	// If we're showing a tooltip?
	bool rescan = false;
	double t = GetTimeSinceStartup();

	// Figure out from mouse movements and timing if we need to scan for tooltips.
	if (m_TargetAlpha > 0.0f)
	{
		if (!m_TooltipRect.Contains (mousePos))
			rescan = true;
	} 
	else 
	{
		if (mousePos != m_LastMousePos)
		{
			m_NextUpdate = t + TOOLTIP_TIME;
		} 
		else
		{
			if (t > m_NextUpdate || m_CurrentAlpha > 0.0f)
			{
				rescan = true;
				m_NextUpdate = std::numeric_limits<double>::infinity();
			}
		}
		m_LastMousePos = mousePos;
	}
	
	// rescan, find out if we want to show tooltips
	if (rescan)
	{
		GUIView* view = GetMouseOverWindow();		
		if (view)
		{
			m_TargetAlpha = 0.0f;
			CallStaticMonoMethod("EditorGUI", "BeginCollectTooltips");
			view->ForceRepaint();
			CallStaticMonoMethod("EditorGUI", "EndCollectTooltips");
			if (m_TargetAlpha)
			{
				void * params[] = { m_Tooltip.GetScriptingString(), &m_TooltipRect };
				CallStaticMonoMethod("TooltipView", "Show", params);
			}
		}
	}
		
	if (m_CurrentAlpha != m_TargetAlpha)
	{
		float deltaTime = t - m_LastTime;
		if (m_CurrentAlpha < m_TargetAlpha)
			m_CurrentAlpha = clamp01 (m_CurrentAlpha + deltaTime * TOOLTIP_FADEINSPEED);
		else if (m_CurrentAlpha > m_TargetAlpha)
			m_CurrentAlpha = clamp01 (m_CurrentAlpha - deltaTime * TOOLTIP_FADEOUTSPEED);
		if (m_CurrentAlpha == 0)
			CallStaticMonoMethod ("TooltipView", "Close");
		else
		{
			void * params[] = { &m_CurrentAlpha };
			CallStaticMonoMethod("TooltipView", "SetAlpha", params);
		}
	}
	m_LastTime = t;
}


void TooltipManager::SendEvent(const InputEvent& event)
{
	if (event.type == InputEvent::kMouseDown)
	{
		// hide current tooltip
		if (m_CurrentAlpha != 0.0f)
		{
			CallStaticMonoMethod("TooltipView", "Close");
			m_CurrentAlpha = m_TargetAlpha = 0.0f;
		}
	}		
}

void TooltipManager::SetTooltip (const UTF16String& tooltip, const Rectf& rect)
{
	m_TargetAlpha = 1.0f;
	m_TooltipRect = rect;
	m_Tooltip.CopyString (tooltip);
}
