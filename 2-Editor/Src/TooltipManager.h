#ifndef __TOOLTIP_H__
#define __TOOLTIP_H__

#include "Runtime/Math/Vector2.h"
#include "Runtime/Math/Rect.h"
#include "Runtime/IMGUI/TextUtil.h"
struct InputEvent;

class TooltipManager  {
public:	
	TooltipManager();
	
	void Update();	
	void SendEvent(const InputEvent& event);
	void SetTooltip (const UTF16String& tooltip, const Rectf& rect);
private:	
	Vector2f m_LastMousePos;
	double m_NextUpdate;
	double m_LastTime;
	float m_CurrentAlpha;
	float m_TargetAlpha;
	Rectf m_TooltipRect;
	UTF16String m_Tooltip;
};

TooltipManager &GetTooltipManager ();

#endif // __TOOLTIP_H__
